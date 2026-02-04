#include "protocols/mqtt/ota_manager.h"
#include "protocols/mqtt/wifi_provisioning.h"
#include "core/core_config.h"
#include "core/display_manager.h"

#include <string.h>
#include <ctype.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_app_format.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"

static const char *TAG = "ota";

/**
 * Compare two semantic versions
 * Handles formats: "v0.8.2", "0.8.2", "v0.8.2-beta"
 * @param v1 First version string
 * @param v2 Second version string
 * @return -1 if v1 < v2, 0 if equal, 1 if v1 > v2
 */
static int ota_version_compare(const char *v1, const char *v2)
{
		if (!v1 || !v2) {
				return 0;
		}

		// Strip 'v' prefix if present
		const char *ver1 = (v1[0] == 'v' || v1[0] == 'V') ? v1 + 1 : v1;
		const char *ver2 = (v2[0] == 'v' || v2[0] == 'V') ? v2 + 1 : v2;

		int major1 = 0, minor1 = 0, patch1 = 0;
		int major2 = 0, minor2 = 0, patch2 = 0;

		// Parse version numbers
		sscanf(ver1, "%d.%d.%d", &major1, &minor1, &patch1);
		sscanf(ver2, "%d.%d.%d", &major2, &minor2, &patch2);

		// Compare major.minor.patch
		if (major1 != major2) return (major1 > major2) ? 1 : -1;
		if (minor1 != minor2) return (minor1 > minor2) ? 1 : -1;
		if (patch1 != patch2) return (patch1 > patch2) ? 1 : -1;

		// Check for pre-release suffixes (-beta, -alpha, -rc)
		const char *suffix1 = strchr(ver1, '-');
		const char *suffix2 = strchr(ver2, '-');

		// If base versions are equal but one has pre-release suffix
		if (suffix1 && !suffix2) return -1;  // v0.8.2-beta < v0.8.2
		if (!suffix1 && suffix2) return 1;   // v0.8.2 > v0.8.2-beta

		// Both have suffixes or neither has - equal
		return 0;
}

/**
 * @brief Post-OTA boot confirmation
 * Called on first boot after successful OTA update
 * Shows clear confirmation to user that update was successful
 */
static void ota_show_boot_status(bool success)
{
		if (success) {
				ESP_LOGI(TAG, "");
				ESP_LOGI(TAG, "========================================");
				ESP_LOGI(TAG, "✓ OTA UPDATE CONFIRMED");
				ESP_LOGI(TAG, "========================================");
				ESP_LOGI(TAG, "Successfully booted from new partition");
				ESP_LOGI(TAG, "New firmware version: %s", APP_VERSION);
				ESP_LOGI(TAG, "OTA rollback protection: ACTIVE");
				ESP_LOGI(TAG, "System is now running updated firmware");
				ESP_LOGI(TAG, "========================================");
				ESP_LOGI(TAG, "");
		}
}

esp_err_t ota_init(void)
{
		const esp_partition_t *running = esp_ota_get_running_partition();
		esp_ota_img_states_t ota_state;

		if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
				if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
						// First boot after OTA update - turn off green LED and confirm
						extern void display_ota_led_off(void);  // Cleanup green LED from successful OTA
						display_ota_led_off();

						ESP_LOGI(TAG, "First boot after OTA update detected");
						esp_ota_mark_app_valid_cancel_rollback();
						ota_show_boot_status(true);
				}
		}

		ESP_LOGI(TAG, "");
		ESP_LOGI(TAG, "========================================");
		ESP_LOGI(TAG, "ACTIVE PARTITION: %s (offset 0x%08lx)",
						 running->label, running->address);
		ESP_LOGI(TAG, "FIRMWARE VERSION: %s", APP_VERSION);
		ESP_LOGI(TAG, "========================================");
		ESP_LOGI(TAG, "");

		return ESP_OK;
}

esp_err_t ota_update_from_url(const char *url)
{
		ESP_LOGI(TAG, "========================================");
		ESP_LOGI(TAG, "Starting OTA Update");
		ESP_LOGI(TAG, "========================================");
		ESP_LOGI(TAG, "Download URL: %s", url);
		ESP_LOGI(TAG, "Current version: %s", APP_VERSION);

		// Display already deinitialized by ota_check_github_release()
		ESP_LOGI(TAG, "Display already prepared for OTA");

		// Log available memory before OTA
		size_t free_heap = esp_get_free_heap_size();
		ESP_LOGI(TAG, "Free heap before OTA: %lu bytes", free_heap);

		// Configure HTTP client with certificate bundle and minimal buffer for ESP32-C5
		esp_http_client_config_t config = {
				.url = url,
				.timeout_ms = 60000,  // 60s timeout for large firmware downloads
				.keep_alive_enable = true,
				.crt_bundle_attach = esp_crt_bundle_attach,  // Enable HTTPS certificate verification
				.buffer_size = 1024,        // 1KB receive buffer (minimum for HTTPS)
				.buffer_size_tx = 1024,     // 1KB transmit buffer
		};

		esp_https_ota_config_t ota_config = {
				.http_config = &config,
				.bulk_flash_erase = true,   // Erase entire partition before writing (faster)
				.partial_http_download = false,  // Don't use range requests
		};

		ESP_LOGI(TAG, "HTTP buffer sizes: RX=1KB, TX=1KB");
		ESP_LOGI(TAG, "Starting firmware download and installation...");
		esp_err_t ret = esp_https_ota(&ota_config);

		if (ret == ESP_OK) {
				ESP_LOGI(TAG, "========================================");
				ESP_LOGI(TAG, "✓ OTA UPDATE SUCCESSFUL!");
				ESP_LOGI(TAG, "========================================");
				ESP_LOGI(TAG, "New firmware installed to update partition");
		ESP_LOGI(TAG, "Flashing green LED 3 times...");

		// Flash green LED 3 times to indicate success
		display_ota_restore(true);

				ESP_LOGI(TAG, "Rebooting now...");
				esp_restart();
		} else {
				ESP_LOGE(TAG, "========================================");
				ESP_LOGE(TAG, "✗ OTA UPDATE FAILED");
				ESP_LOGE(TAG, "========================================");
				ESP_LOGE(TAG, "Error: %s", esp_err_to_name(ret));
				ESP_LOGE(TAG, "Restoring display...");

				// RGB LED will turn RED during restore (failure)
				display_ota_restore(false);

				ESP_LOGI(TAG, "Device remains on current firmware: %s", APP_VERSION);
				return ret;
		}

		return ESP_OK;
}

const char* ota_get_version(void)
{
		return APP_VERSION;
}

esp_err_t ota_check_and_update(const char *url)
{
		ESP_LOGI(TAG, "Checking for updates...");

		// In a real implementation, you would:
		// 1. Query a version endpoint first
		// 2. Compare versions
		// 3. Only download if newer version available
		// 4. Verify firmware signature

		// For simplicity, this just attempts the update
		return ota_update_from_url(url);
}

/**
 * Check GitHub releases for new firmware and update if available
 * @param release_channel 0 = stable (latest release), 1 = develop (all releases including pre-release)
 * @return ESP_OK if no update needed or update successful, error code otherwise
 */
esp_err_t ota_check_github_release(uint8_t release_channel)
{
		esp_err_t ret = ESP_FAIL;
		char *response_buffer = NULL;
		cJSON *json = NULL;

		// Determine API endpoint based on channel
		const char *api_url = (release_channel == 0)
				? "https://api.github.com/repos/MakeOpenStuff/hestia32-firmware/releases/latest"
				: "https://api.github.com/repos/MakeOpenStuff/hestia32-firmware/releases?per_page=30";

		ESP_LOGI(TAG, "Checking for updates from GitHub (%s channel)...",
						 release_channel == 0 ? "stable" : "develop");

	// Validate target partition has space
	const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
	if (!update_partition) {
			ESP_LOGE(TAG, "No OTA update partition found");
			return ESP_ERR_NOT_FOUND;
	}

	ESP_LOGI(TAG, "Target partition: %s (size: %lu bytes)",
					 update_partition->label, update_partition->size);

	// Prepare display for OTA check (free memory for GitHub API)
	ESP_LOGI(TAG, "Preparing display for OTA check (freeing memory)...");
	display_ota_prepare();
	ESP_LOGI(TAG, "Display deinitialized, proceeding with GitHub API check");

	// Allocate response buffer for GitHub API
	response_buffer = malloc(16384);  // 16KB should be enough for API response
	if (!response_buffer) {
			ESP_LOGE(TAG, "Failed to allocate response buffer");
			display_ota_restore(false);
			return ESP_ERR_NO_MEM;
	}

	// Configure HTTP client with certificate bundle for HTTPS
	esp_http_client_config_t config = {
			.url = api_url,
			.timeout_ms = 10000,
			.buffer_size = 4096,
			.user_agent = "Hestia32-OTA-Client/1.0",
			.crt_bundle_attach = esp_crt_bundle_attach,  // Enable certificate verification
	};

	esp_http_client_handle_t client = esp_http_client_init(&config);
	if (!client) {
			ESP_LOGE(TAG, "Failed to initialize HTTP client");
			free(response_buffer);
			return ESP_FAIL;
	}

	// Perform HTTP GET
	esp_err_t err = esp_http_client_open(client, 0);
	if (err != ESP_OK) {
			ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
			esp_http_client_cleanup(client);
			free(response_buffer);
			return err;
	}

	esp_http_client_fetch_headers(client);
	// Read response
	int read_len = esp_http_client_read(client, response_buffer, 16383);
	esp_http_client_cleanup(client);

	if (read_len <= 0) {
			ESP_LOGE(TAG, "Failed to read response");
			free(response_buffer);
			return ESP_FAIL;
	}

	ESP_LOGD(TAG, "Received %d bytes from GitHub API", read_len);

	// Parse JSON response
	json = cJSON_Parse(response_buffer);
	if (!json) {
			ESP_LOGE(TAG, "Failed to parse JSON response");
			free(response_buffer);
			return ESP_FAIL;
	}

	// Check for GitHub API error response (404, rate limit, etc.)
	cJSON *message = cJSON_GetObjectItem(json, "message");
	if (message && cJSON_IsString(message)) {
			ESP_LOGE(TAG, "GitHub API error: %s", message->valuestring);
			cJSON_Delete(json);
			free(response_buffer);
			return ESP_ERR_NOT_FOUND;
	}

	// Handle different response formats (single release vs array)
	cJSON *release = NULL;
	if (cJSON_IsArray(json)) {
			// Develop channel - get first (newest) release from array
			release = cJSON_GetArrayItem(json, 0);
	} else {
			// Stable channel - single release object
			release = json;
	}

	if (!release) {
			ESP_LOGE(TAG, "No releases found");
			cJSON_Delete(json);
			free(response_buffer);
			return ESP_ERR_NOT_FOUND;
	}

	// Extract version tag
	cJSON *tag_name = cJSON_GetObjectItem(release, "tag_name");
	if (!tag_name || !cJSON_IsString(tag_name)) {
			ESP_LOGE(TAG, "Invalid release format: no tag_name");
			cJSON_Delete(json);
			free(response_buffer);
			return ESP_FAIL;
	}

	const char *remote_version = tag_name->valuestring;
	ESP_LOGI(TAG, "Latest release: %s (current: %s)", remote_version, APP_VERSION);

	// Compare versions
	int cmp = ota_version_compare(remote_version, APP_VERSION);
	if (cmp <= 0) {
			ESP_LOGI(TAG, "Firmware is up to date");
			cJSON_Delete(json);
			free(response_buffer);
			// Restore display since we're not downloading (not an error, just no update needed)
			extern void display_ota_restore_no_update(void);
			display_ota_restore_no_update();
			return ESP_OK;
	}

	ESP_LOGI(TAG, "New version available: %s", remote_version);

	// Find firmware asset for MQTT protocol
	cJSON *assets = cJSON_GetObjectItem(release, "assets");
	if (!assets || !cJSON_IsArray(assets)) {
			ESP_LOGE(TAG, "No assets found in release");
			cJSON_Delete(json);
			free(response_buffer);
			return ESP_FAIL;
	}

	char asset_name[64];
	snprintf(asset_name, sizeof(asset_name), "hestia32-mqtt-%s.bin", remote_version);

	const char *download_url = NULL;
	int asset_size = 0;

	cJSON *asset = NULL;
	cJSON_ArrayForEach(asset, assets) {
			cJSON *name = cJSON_GetObjectItem(asset, "name");
			if (name && cJSON_IsString(name) &&
					strcmp(name->valuestring, asset_name) == 0) {

					cJSON *browser_download_url = cJSON_GetObjectItem(asset, "browser_download_url");
					cJSON *size = cJSON_GetObjectItem(asset, "size");

					if (browser_download_url && cJSON_IsString(browser_download_url)) {
							download_url = browser_download_url->valuestring;
					}
					if (size && cJSON_IsNumber(size)) {
							asset_size = size->valueint;
					}
					break;
			}
	}

	if (!download_url) {
			ESP_LOGE(TAG, "Firmware asset not found: %s", asset_name);
			cJSON_Delete(json);
			free(response_buffer);
			return ESP_ERR_NOT_FOUND;
	}

	ESP_LOGI(TAG, "Found firmware asset: %s (%d bytes)", asset_name, asset_size);

	// Validate partition size
	if ((uint32_t)asset_size > update_partition->size) {
			ESP_LOGE(TAG, "Firmware too large: %d bytes (partition: %lu bytes)",
							 asset_size, update_partition->size);
			cJSON_Delete(json);
			free(response_buffer);
			return ESP_ERR_INVALID_SIZE;
	}

	// Copy download URL (it will be freed with JSON)
	char *url_copy = strdup(download_url);

	// Clean up JSON before download
	cJSON_Delete(json);
	free(response_buffer);

	if (!url_copy) {
			ESP_LOGE(TAG, "Failed to allocate URL memory");
			display_ota_restore(false);
			return ESP_ERR_NO_MEM;
	}

	// Display already deinitialized for API check, now ready for firmware download
	ESP_LOGI(TAG, "Starting firmware download from GitHub...");

	// Download and install update
	ESP_LOGI(TAG, "Downloading firmware from: %s", url_copy);
	ret = ota_update_from_url(url_copy);

	free(url_copy);
	return ret;
}

// Task handle for periodic OTA check task
static TaskHandle_t ota_task_handle = NULL;

/**
 * Background task that periodically checks for OTA updates
 */
static void ota_check_task_function(void *pvParameters)
{
		wifi_config_data_t *config = (wifi_config_data_t *)pvParameters;

// Initial delay of 15 seconds for system stabilization
	ESP_LOGI(TAG, "OTA check task started. Waiting 15 seconds for system stabilization...");
	vTaskDelay(pdMS_TO_TICKS(15 * 1000));

		while (1) {
				// Check if auto-update is enabled
				if (!config->ota_auto_update) {
						ESP_LOGI(TAG, "Auto-update disabled, skipping check");
						vTaskDelay(pdMS_TO_TICKS(60 * 60 * 1000));  // Check every hour if it's been enabled
						continue;
				}

				ESP_LOGI(TAG, "Performing scheduled OTA check...");

				// Attempt update with exponential backoff on failure
				esp_err_t ret = ESP_FAIL;
				int retry_delay_ms = 60 * 1000;  // Start with 1 minute
				const int max_retries = 3;

				for (int retry = 0; retry < max_retries; retry++) {
						ret = ota_check_github_release(config->ota_release_channel);

						if (ret == ESP_OK) {
								// Update successful or no update needed
								// Update last check timestamp
								config->ota_last_check = (uint64_t)time(NULL);
								wifi_prov_save_config(config);

								ESP_LOGI(TAG, "OTA check completed successfully");
								break;
						}

						// Network or API failure - apply exponential backoff
						if (retry < max_retries - 1) {
								ESP_LOGW(TAG, "OTA check failed (retry %d/%d). Retrying in %d seconds...",
												 retry + 1, max_retries, retry_delay_ms / 1000);
								vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
								retry_delay_ms *= 2;  // Double delay: 1min -> 2min -> 4min
						} else {
								ESP_LOGE(TAG, "OTA check failed after %d retries", max_retries);
						}
				}

				// Sleep for configured interval
				uint32_t sleep_hours = config->ota_check_interval;
				if (sleep_hours == 0) {
						sleep_hours = 24;  // Default to 24 hours if invalid
				}

				ESP_LOGI(TAG, "Next OTA check in %lu hours", sleep_hours);
				vTaskDelay(pdMS_TO_TICKS(sleep_hours * 60 * 60 * 1000));
		}

		vTaskDelete(NULL);
}

/**
 * Start the periodic OTA check task
 * @param config Pointer to WiFi configuration (must remain valid for task lifetime)
 * @return ESP_OK on success, error code otherwise
 */
__attribute__((used))
esp_err_t ota_start_check_task(wifi_config_data_t *config)
{
		if (!config) {
				return ESP_ERR_INVALID_ARG;
		}

		if (ota_task_handle != NULL) {
				ESP_LOGW(TAG, "OTA check task already running");
				return ESP_OK;
		}

		ESP_LOGI(TAG, "Starting OTA check task (auto_update: %s, channel: %s, interval: %lu hours)",
						 config->ota_auto_update ? "enabled" : "disabled",
						 config->ota_release_channel == 0 ? "stable" : "develop",
						 config->ota_check_interval);

		BaseType_t ret = xTaskCreate(
				ota_check_task_function,
				"ota_check",
				8192,  // 8KB stack
				config,
				5,     // Medium priority
				&ota_task_handle
		);

		if (ret != pdPASS) {
				ESP_LOGE(TAG, "Failed to create OTA check task");
				return ESP_FAIL;
		}

		return ESP_OK;
}
