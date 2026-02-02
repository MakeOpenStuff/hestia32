#include "protocols/mqtt/mqtt_protocol.h"
#include "protocols/mqtt/wifi_provisioning.h"
#include "protocols/mqtt/wifi_manager.h"
#include "protocols/mqtt/ota_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "mqtt_protocol";

// MQTT protocol implementation
static esp_err_t mqtt_init(void) {
    ESP_LOGI(TAG, "Initializing MQTT protocol");

    // Initialize WiFi provisioning system
    esp_err_t err = wifi_prov_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi provisioning init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "MQTT protocol initialized");
    return ESP_OK;
}

static esp_err_t mqtt_start(void) {
    ESP_LOGI(TAG, "Starting MQTT protocol");

    // Check if device is provisioned
    bool is_provisioned = wifi_prov_is_provisioned();

    if (!is_provisioned) {
        ESP_LOGI(TAG, "Device not provisioned - starting provisioning AP");
        esp_err_t err = wifi_prov_start_ap();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start provisioning AP: %s", esp_err_to_name(err));
            return err;
        }

        // In provisioning mode - will restart after provisioning complete
        ESP_LOGI(TAG, "Provisioning mode active - waiting for configuration");
        return ESP_OK;
    }

    // Device is provisioned - load credentials and connect
    ESP_LOGI(TAG, "Device provisioned - connecting to WiFi");

    // Load WiFi credentials from NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_config", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    char ssid[32] = {0};
    char password[64] = {0};
    size_t ssid_len = sizeof(ssid);
    size_t pass_len = sizeof(password);

    err = nvs_get_str(nvs_handle, "ssid", ssid, &ssid_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read SSID from NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_get_str(nvs_handle, "password", password, &pass_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read password from NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);

    // Initialize WiFi
    err = wifi_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(err));
        return err;
    }

    // Connect with credentials from NVS
    err = wifi_connect(ssid, password, 10);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connect failed: %s", esp_err_to_name(err));
        return err;
    }

    // Wait for WiFi connection
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    while (!wifi_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "WiFi connected");

    // Initialize OTA manager
    err = ota_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "OTA init failed: %s", esp_err_to_name(err));
        // Non-fatal - continue without OTA
    }

    ESP_LOGI(TAG, "MQTT protocol started successfully");
    return ESP_OK;
}

static esp_err_t mqtt_stop(void) {
    ESP_LOGI(TAG, "Stopping MQTT protocol");

    wifi_disconnect();

    return ESP_OK;
}

static esp_err_t mqtt_report_temperature(float temperature) {
    // TODO: Publish to MQTT topic
    ESP_LOGD(TAG, "Report temperature: %.1f°C", temperature);
    return ESP_OK;
}

static esp_err_t mqtt_report_humidity(float humidity) {
    // TODO: Publish to MQTT topic
    ESP_LOGD(TAG, "Report humidity: %.1f%%", humidity);
    return ESP_OK;
}

static esp_err_t mqtt_report_setpoint(float heat_setpoint, float cool_setpoint) {
    // TODO: Publish to MQTT topic
    ESP_LOGD(TAG, "Report setpoints: heat=%.1f°C, cool=%.1f°C", heat_setpoint, cool_setpoint);
    return ESP_OK;
}

static esp_err_t mqtt_report_mode(const char* mode) {
    // TODO: Publish to MQTT topic
    ESP_LOGD(TAG, "Report mode: %s", mode);
    return ESP_OK;
}

static esp_err_t mqtt_report_relay_state(bool heating, bool cooling, bool fan,
                                          bool humidifier, bool dehumidifier, bool hot_water) {
    // TODO: Publish to MQTT topic
    ESP_LOGD(TAG, "Report relays: H=%d C=%d F=%d Hum=%d Dehum=%d HW=%d",
             heating, cooling, fan, humidifier, dehumidifier, hot_water);
    return ESP_OK;
}

static esp_err_t mqtt_report_boost_state(const char* domain, uint32_t remaining_sec) {
    // TODO: Publish to MQTT topic
    ESP_LOGD(TAG, "Report boost: domain=%s, remaining=%lu", domain, remaining_sec);
    return ESP_OK;
}

static bool mqtt_is_connected(void) {
    return wifi_is_connected();
}

static const char* mqtt_get_name(void) {
    return "MQTT";
}

// Protocol interface for MQTT
static const protocol_interface_t mqtt_protocol = {
    .init = mqtt_init,
    .start = mqtt_start,
    .stop = mqtt_stop,
    .report_temperature = mqtt_report_temperature,
    .report_humidity = mqtt_report_humidity,
    .report_setpoint = mqtt_report_setpoint,
    .report_mode = mqtt_report_mode,
    .report_relay_state = mqtt_report_relay_state,
    .report_boost_state = mqtt_report_boost_state,
    .is_connected = mqtt_is_connected,
    .get_name = mqtt_get_name,
};

const protocol_interface_t* protocol_get_mqtt(void) {
    return &mqtt_protocol;
}
