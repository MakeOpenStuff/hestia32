#include "protocols/mqtt/wifi_provisioning.h"
#include "protocols/mqtt/mqtt_config.h"
#include "protocols/mqtt/provisioning_html.h"

#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

static const char *TAG = "wifi_prov";
#define NVS_NAMESPACE "wifi_config"
#define DNS_PORT 53

static httpd_handle_t server = NULL;
static int dns_socket = -1;
static bool dns_running = false;
static esp_netif_t *s_ap_netif = NULL;  /* Track AP netif for cleanup */
static bool s_wifi_skipped = false;  /* Track if user explicitly skipped WiFi */

// Simple DNS server task for captive portal
static void dns_server_task(void *pvParameters)
{
		struct sockaddr_in server_addr;
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = INADDR_ANY;
		server_addr.sin_port = htons(DNS_PORT);

		dns_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (dns_socket < 0) {
				ESP_LOGE(TAG, "Failed to create DNS socket");
				vTaskDelete(NULL);
				return;
		}

		if (bind(dns_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
				ESP_LOGE(TAG, "Failed to bind DNS socket");
				close(dns_socket);
				vTaskDelete(NULL);
				return;
		}

		ESP_LOGI(TAG, "DNS server started on port 53");
		dns_running = true;

		uint8_t rx_buffer[512];
		struct sockaddr_in client_addr;
		socklen_t client_addr_len = sizeof(client_addr);

		while (dns_running) {
				int len = recvfrom(dns_socket, rx_buffer, sizeof(rx_buffer), 0,
													(struct sockaddr *)&client_addr, &client_addr_len);

				if (len > 0) {
						// Build DNS response pointing to our IP (192.168.4.1)
						uint8_t tx_buffer[512];
						memcpy(tx_buffer, rx_buffer, len);

						// Set response flags
						tx_buffer[2] = 0x81;
						tx_buffer[3] = 0x80;

						// Append answer section
						uint8_t *answer = tx_buffer + len;
						// Pointer to question name
						*answer++ = 0xC0;
						*answer++ = 0x0C;
						// Type A
						*answer++ = 0x00;
						*answer++ = 0x01;
						// Class IN
						*answer++ = 0x00;
						*answer++ = 0x01;
						// TTL
						*answer++ = 0x00;
						*answer++ = 0x00;
						*answer++ = 0x00;
						*answer++ = 0x3C;
						// Data length
						*answer++ = 0x00;
						*answer++ = 0x04;
						// IP address 192.168.4.1
						*answer++ = 192;
						*answer++ = 168;
						*answer++ = 4;
						*answer++ = 1;

						// Update answer count
						tx_buffer[6] = 0x00;
						tx_buffer[7] = 0x01;

						int response_len = answer - tx_buffer;
						sendto(dns_socket, tx_buffer, response_len, 0,
									 (struct sockaddr *)&client_addr, client_addr_len);
				}

				vTaskDelay(pdMS_TO_TICKS(10));
		}

		close(dns_socket);
		vTaskDelete(NULL);
}

// HTTP handler for root page
static esp_err_t root_handler(httpd_req_t *req)
{
		httpd_resp_set_type(req, "text/html");

		// Send HTML in chunks to avoid buffer limits
		size_t total_len = strlen(html_page);
		size_t chunk_size = 512;
		size_t sent = 0;

		while (sent < total_len) {
				size_t to_send = (total_len - sent < chunk_size) ? (total_len - sent) : chunk_size;
				httpd_resp_send_chunk(req, html_page + sent, to_send);
				sent += to_send;
		}

		httpd_resp_send_chunk(req, NULL, 0);
		return ESP_OK;
}

// HTTP handler for captive portal detection
static esp_err_t captive_portal_handler(httpd_req_t *req)
{
		httpd_resp_set_status(req, "302 Found");
		httpd_resp_set_hdr(req, "Location", "http://192.168.4.1");
		httpd_resp_send(req, NULL, 0);
		return ESP_OK;
}

// HTTP handler for network scan
static esp_err_t scan_handler(httpd_req_t *req)
{
		wifi_scan_config_t scan_config = {
				.ssid = NULL,
				.bssid = NULL,
				.channel = 0,
				.show_hidden = false
		};

		ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

		uint16_t ap_count = 0;
		esp_wifi_scan_get_ap_num(&ap_count);

		if (ap_count > 20) ap_count = 20;

		wifi_ap_record_t *ap_info = malloc(sizeof(wifi_ap_record_t) * ap_count);
		if (!ap_info) {
				httpd_resp_send_500(req);
				return ESP_FAIL;
		}

		ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_info));

		cJSON *root = cJSON_CreateArray();
		for (int i = 0; i < ap_count; i++) {
				cJSON *item = cJSON_CreateObject();
				cJSON_AddStringToObject(item, "ssid", (char *)ap_info[i].ssid);
				cJSON_AddNumberToObject(item, "rssi", ap_info[i].rssi);
				cJSON_AddItemToArray(root, item);
		}

		char *json_str = cJSON_Print(root);
		httpd_resp_set_type(req, "application/json");
		httpd_resp_send(req, json_str, strlen(json_str));

		free(json_str);
		cJSON_Delete(root);
		free(ap_info);

		return ESP_OK;
}

// HTTP handler for WiFi connection
static esp_err_t connect_handler(httpd_req_t *req)
{
		char buf[512];  // Increased buffer size for OTA fields
		int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
		if (ret <= 0) {
				httpd_resp_send_500(req);
				return ESP_FAIL;
		}
		buf[ret] = '\0';

		cJSON *root = cJSON_Parse(buf);
		if (!root) {
				httpd_resp_send_500(req);
				return ESP_FAIL;
		}

		cJSON *ssid_json = cJSON_GetObjectItem(root, "ssid");
		cJSON *password_json = cJSON_GetObjectItem(root, "password");
		cJSON *ota_enabled_json = cJSON_GetObjectItem(root, "ota_enabled");
		cJSON *ota_channel_json = cJSON_GetObjectItem(root, "ota_channel");
		cJSON *ota_interval_json = cJSON_GetObjectItem(root, "ota_interval");

		if (!ssid_json || !cJSON_IsString(ssid_json)) {
				cJSON_Delete(root);
				httpd_resp_send_500(req);
				return ESP_FAIL;
		}

		// Save WiFi credentials and OTA settings to NVS
		nvs_handle_t nvs_handle;
		if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
				// WiFi credentials
				nvs_set_str(nvs_handle, "ssid", ssid_json->valuestring);
				if (password_json && cJSON_IsString(password_json)) {
						nvs_set_str(nvs_handle, "password", password_json->valuestring);
				} else {
						nvs_set_str(nvs_handle, "password", "");
				}
				nvs_set_u8(nvs_handle, "provisioned", 1);

				// OTA settings
				if (ota_enabled_json && cJSON_IsBool(ota_enabled_json)) {
						nvs_set_u8(nvs_handle, "ota_enabled", cJSON_IsTrue(ota_enabled_json) ? 1 : 0);
				}
				if (ota_channel_json && cJSON_IsNumber(ota_channel_json)) {
						nvs_set_u8(nvs_handle, "ota_channel", (uint8_t)ota_channel_json->valueint);
				}
				if (ota_interval_json && cJSON_IsNumber(ota_interval_json)) {
						nvs_set_u32(nvs_handle, "ota_interval", (uint32_t)ota_interval_json->valueint);
				}

				nvs_commit(nvs_handle);
				nvs_close(nvs_handle);

				ESP_LOGI(TAG, "Configuration saved to NVS: SSID=%s, OTA=%s",
								 ssid_json->valuestring,
								 (ota_enabled_json && cJSON_IsTrue(ota_enabled_json)) ? "enabled" : "disabled");
		}

		cJSON_Delete(root);

		cJSON *response = cJSON_CreateObject();
		cJSON_AddBoolToObject(response, "success", true);
		char *resp_str = cJSON_Print(response);
		httpd_resp_set_type(req, "application/json");
		httpd_resp_send(req, resp_str, strlen(resp_str));
		free(resp_str);
		cJSON_Delete(response);

		ESP_LOGI(TAG, "Configuration saved. Rebooting in 2 seconds...");
		vTaskDelay(pdMS_TO_TICKS(2000));
		esp_restart();

		return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
		httpd_config_t config = HTTPD_DEFAULT_CONFIG();
		config.server_port = 80;
		config.max_uri_handlers = 16;
		config.lru_purge_enable = true;
		config.stack_size = 8192;
		config.backlog_conn = 5;
		config.recv_wait_timeout = 10;
		config.send_wait_timeout = 10;

		if (httpd_start(&server, &config) == ESP_OK) {
				httpd_uri_t root_uri = {
						.uri = "/",
						.method = HTTP_GET,
						.handler = root_handler
				};
				httpd_register_uri_handler(server, &root_uri);

				httpd_uri_t scan_uri = {
						.uri = "/scan",
						.method = HTTP_GET,
						.handler = scan_handler
				};
				httpd_register_uri_handler(server, &scan_uri);

				httpd_uri_t connect_uri = {
						.uri = "/connect",
						.method = HTTP_POST,
						.handler = connect_handler
				};
				httpd_register_uri_handler(server, &connect_uri);

				// Captive portal detection endpoints
				httpd_uri_t generate_204_uri = {
						.uri = "/generate_204",
						.method = HTTP_GET,
						.handler = captive_portal_handler
				};
				httpd_register_uri_handler(server, &generate_204_uri);

				httpd_uri_t hotspot_detect_uri = {
						.uri = "/hotspot-detect.html",
						.method = HTTP_GET,
						.handler = captive_portal_handler
				};
				httpd_register_uri_handler(server, &hotspot_detect_uri);

				httpd_uri_t library_test_uri = {
						.uri = "/library/test/success.html",
						.method = HTTP_GET,
						.handler = captive_portal_handler
				};
				httpd_register_uri_handler(server, &library_test_uri);

				httpd_uri_t ncsi_uri = {
						.uri = "/ncsi.txt",
						.method = HTTP_GET,
						.handler = captive_portal_handler
				};
				httpd_register_uri_handler(server, &ncsi_uri);

				ESP_LOGI(TAG, "HTTP server started on port 80");
				return server;
		}

		ESP_LOGE(TAG, "Failed to start HTTP server");
		return NULL;
}esp_err_t wifi_prov_init(void)
{
		esp_err_t ret = nvs_flash_init();
		if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
				ESP_ERROR_CHECK(nvs_flash_erase());
				ret = nvs_flash_init();
		}

		ESP_ERROR_CHECK(esp_netif_init());
		ESP_ERROR_CHECK(esp_event_loop_create_default());

		return ESP_OK;
}

bool wifi_prov_is_provisioned(void)
{
		nvs_handle_t nvs_handle;
		uint8_t provisioned = 0;

		if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle) == ESP_OK) {
				nvs_get_u8(nvs_handle, "provisioned", &provisioned);
				nvs_close(nvs_handle);
		}

		return provisioned == 1;
}

esp_err_t wifi_prov_start_ap(void)
{
	esp_err_t err;

	/* Don't start AP if user explicitly skipped provisioning */
	if (s_wifi_skipped) {
		ESP_LOGI(TAG, "WiFi provisioning was skipped by user - not starting AP");
		return ESP_OK;
	}

	/* Check if AP netif already exists */
	if (s_ap_netif != NULL) {
		ESP_LOGW(TAG, "AP netif already exists - not creating again");
		return ESP_OK;
	}

	s_ap_netif = esp_netif_create_default_wifi_ap();
	if (s_ap_netif == NULL) {
		ESP_LOGE(TAG, "Failed to create default WiFi AP netif");
		return ESP_ERR_NO_MEM;
	}
	esp_netif_t *ap_netif = s_ap_netif;
	esp_netif_ip_info_t ip_info;		IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
		IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
		IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
		err = esp_netif_dhcps_stop(ap_netif);
		if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
			ESP_LOGE(TAG, "esp_netif_dhcps_stop failed: %s", esp_err_to_name(err));
			goto fail;
		}

		err = esp_netif_set_ip_info(ap_netif, &ip_info);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "esp_netif_set_ip_info failed: %s", esp_err_to_name(err));
			goto fail;
		}

		err = esp_netif_dhcps_start(ap_netif);
		if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
			ESP_LOGE(TAG, "esp_netif_dhcps_start failed: %s", esp_err_to_name(err));
			goto fail;
		}

		wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
		err = esp_wifi_init(&cfg);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
			goto fail;
		}

		err = esp_wifi_set_mode(WIFI_MODE_APSTA);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "esp_wifi_set_mode failed: %s", esp_err_to_name(err));
			goto fail_wifi;
		}

		wifi_config_t wifi_config = {
				.ap = {
						.channel = 1,
						.max_connection = 4,
						.authmode = strlen(PROV_AP_PASSWORD) > 0 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN,
				},
		};
		strncpy((char *)wifi_config.ap.ssid, PROV_AP_SSID, sizeof(wifi_config.ap.ssid));
		wifi_config.ap.ssid_len = strlen(PROV_AP_SSID);
		if (strlen(PROV_AP_PASSWORD) > 0) {
				strncpy((char *)wifi_config.ap.password, PROV_AP_PASSWORD, sizeof(wifi_config.ap.password));
		}

		err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "esp_wifi_set_config(AP) failed: %s", esp_err_to_name(err));
			goto fail_wifi;
		}

		err = esp_wifi_start();
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(err));
			goto fail_wifi;
		}

		// Start DNS server for captive portal
		xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, NULL);

		// Start HTTP server
		start_webserver();

		ESP_LOGI(TAG, "========================================");
		ESP_LOGI(TAG, "Provisioning started");
		ESP_LOGI(TAG, "WiFi SSID: %s", PROV_AP_SSID);
		ESP_LOGI(TAG, "WiFi Password: %s", strlen(PROV_AP_PASSWORD) > 0 ? PROV_AP_PASSWORD : "(Open)");
		ESP_LOGI(TAG, "Captive Portal: http://192.168.4.1");
		ESP_LOGI(TAG, "========================================");

		return ESP_OK;

fail_wifi:
		esp_wifi_stop();
		esp_wifi_deinit();

fail:
		if (s_ap_netif != NULL) {
			esp_netif_destroy(s_ap_netif);
			s_ap_netif = NULL;
		}
		return err;
}

void wifi_prov_stop_ap(void)
{
	ESP_LOGI(TAG, "Stopping provisioning AP...");

	if (server) {
		httpd_stop(server);
		server = NULL;
	}

	dns_running = false;

	esp_wifi_stop();
	esp_wifi_deinit();

	/* Destroy AP netif if it exists */
	if (s_ap_netif != NULL) {
		esp_netif_destroy(s_ap_netif);
		s_ap_netif = NULL;
		ESP_LOGI(TAG, "AP netif destroyed");
	}
}

void wifi_prov_mark_skipped(void)
{
	s_wifi_skipped = true;
	ESP_LOGI(TAG, "WiFi provisioning marked as skipped");
}

bool wifi_prov_is_skipped(void)
{
	return s_wifi_skipped;
}

esp_err_t wifi_prov_reset(void)
{
		ESP_LOGI(TAG, "Starting factory reset...");

		// Erase all WiFi credentials from our NVS namespace
		nvs_handle_t nvs_handle;
		if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
				nvs_erase_all(nvs_handle);  // Erase entire namespace including SSID, password, provisioned flag
				nvs_commit(nvs_handle);
				nvs_close(nvs_handle);
				ESP_LOGI(TAG, "Custom config namespace erased");
		}

		// Erase ESP-IDF WiFi configuration (stored in default NVS partition)
		// Try different namespace names used by ESP-IDF
		const char* wifi_namespaces[] = {"nvs.net80211", "esp_wifi", "wifi", "nvs"};
		for (int i = 0; i < 4; i++) {
				if (nvs_open(wifi_namespaces[i], NVS_READWRITE, &nvs_handle) == ESP_OK) {
						nvs_erase_all(nvs_handle);
						nvs_commit(nvs_handle);
						nvs_close(nvs_handle);
						ESP_LOGI(TAG, "Erased namespace: %s", wifi_namespaces[i]);
				}
		}

		// Force erase entire NVS partition as last resort
		ESP_LOGW(TAG, "Erasing entire NVS flash...");
		esp_err_t ret = nvs_flash_erase();
		if (ret == ESP_OK) {
				ESP_LOGI(TAG, "NVS flash erased successfully");
				// Reinitialize NVS after erase
				ret = nvs_flash_init();
				if (ret == ESP_OK) {
						ESP_LOGI(TAG, "NVS reinitialized");
				}
		}

		ESP_LOGI(TAG, "Provisioning reset complete - all credentials erased");
		return ESP_OK;
}esp_err_t wifi_prov_get_config(wifi_config_data_t *config)
{
		if (!config) {
				return ESP_ERR_INVALID_ARG;
		}
		memset(config, 0, sizeof(wifi_config_data_t));

		nvs_handle_t nvs_handle;
		if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle) == ESP_OK) {
				size_t len;

				// Read SSID
				len = MAX_SSID_LEN;
				nvs_get_str(nvs_handle, "ssid", config->ssid, &len);

				// Read password
				len = MAX_PASSWORD_LEN;
				nvs_get_str(nvs_handle, "password", config->password, &len);

				// Read server URL
				len = MAX_SERVER_URL_LEN;
				nvs_get_str(nvs_handle, "server_url", config->server_url, &len);

				// Read node name
				len = MAX_NODE_NAME_LEN;
				nvs_get_str(nvs_handle, "node_name", config->node_name, &len);

				// Check provisioned flag
				uint8_t provisioned = 0;
				nvs_get_u8(nvs_handle, "provisioned", &provisioned);
				config->provisioned = (provisioned == 1);

				// Read OTA settings (with defaults)
				uint8_t ota_enabled = 0;  // Disabled by default - enable via UI or provisioning
				nvs_get_u8(nvs_handle, "ota_enabled", &ota_enabled);
				config->ota_auto_update = (ota_enabled == 1);

				uint8_t channel = 0;  // Default: stable
				nvs_get_u8(nvs_handle, "ota_channel", &channel);
				config->ota_release_channel = channel;

				uint32_t interval = 24;  // Default: 24 hours
				nvs_get_u32(nvs_handle, "ota_interval", &interval);
				config->ota_check_interval = interval;

				uint64_t last_check = 0;
				nvs_get_u64(nvs_handle, "ota_last_check", &last_check);
				config->ota_last_check = last_check;

				nvs_close(nvs_handle);
		}

		return ESP_OK;
}

esp_err_t wifi_prov_save_config(const wifi_config_data_t *config)
{
		if (!config) {
				return ESP_ERR_INVALID_ARG;
		}

		nvs_handle_t nvs_handle;
		esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
		if (err != ESP_OK) {
				return err;
		}

		if (config->server_url[0]) {
				nvs_set_str(nvs_handle, "server_url", config->server_url);
		}
		if (config->node_name[0]) {
				nvs_set_str(nvs_handle, "node_name", config->node_name);
		}

		// Save OTA settings
		nvs_set_u8(nvs_handle, "ota_enabled", config->ota_auto_update ? 1 : 0);
		nvs_set_u8(nvs_handle, "ota_channel", config->ota_release_channel);
		nvs_set_u32(nvs_handle, "ota_interval", config->ota_check_interval);
		nvs_set_u64(nvs_handle, "ota_last_check", config->ota_last_check);

		nvs_commit(nvs_handle);
		nvs_close(nvs_handle);

		ESP_LOGI(TAG, "Custom configuration saved");
		return ESP_OK;
}
