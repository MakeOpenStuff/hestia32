#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "config.h"
#include "wifi_manager.h"
#include "wifi_provisioning.h"
#include "ota_manager.h"
#include "display_manager.h"

static const char *TAG = "main";

void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Hestia32 ESP32-C5 Application");
    ESP_LOGI(TAG, "Firmware Version: %s", APP_VERSION);
    ESP_LOGI(TAG, "========================================");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");

    // Initialize display and UI
    ESP_LOGI(TAG, "Initializing display...");
    ret = display_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Display initialized successfully (minimal mode)");
    } else {
        ESP_LOGE(TAG, "Display initialization failed");
    }

    // Initialize OTA manager (checks boot partition, marks as valid if needed)
    ret = ota_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OTA initialization failed");
    }

    // Initialize WiFi provisioning
    wifi_prov_init();
    if (!wifi_prov_is_provisioned()) {
        ESP_LOGI(TAG, "Device not provisioned. Starting provisioning AP...");
        ESP_LOGI(TAG, "Connect to WiFi: %s", PROV_AP_SSID);
        ESP_LOGI(TAG, "Password: %s", PROV_AP_PASSWORD);
        ESP_LOGI(TAG, "Then open http://192.168.4.1 in your browser");

        wifi_prov_start_ap();

        // Wait in provisioning mode (device will restart after config is saved)
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    // Device is provisioned, get configuration
    wifi_config_data_t wifi_config;
    ret = wifi_prov_get_config(&wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get WiFi config");
        // Continue without WiFi
    } else {
        ESP_LOGI(TAG, "Device provisioned. Node name: %s",
                 wifi_config.node_name[0] ? wifi_config.node_name : "Unknown");

        // Initialize and connect to WiFi
        ESP_LOGI(TAG, "Connecting to WiFi: %s", wifi_config.ssid);
        ret = wifi_init();
        if (ret == ESP_OK) {
            wifi_connect(wifi_config.ssid, wifi_config.password, WIFI_MAX_RETRY);
        }
    }

    // Main loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}