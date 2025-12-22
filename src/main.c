#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "config.h"
#include "wifi_manager.h"
#include "wifi_provisioning.h"
#include "ota_manager.h"

static const char *TAG = "main";

void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ESP32 OTA Demo Application");
    ESP_LOGI(TAG, "Firmware Version: %s", APP_VERSION);
    ESP_LOGI(TAG, "========================================");

    // Initialize OTA manager (checks boot partition, marks as valid if needed)
    esp_err_t ret = ota_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OTA initialization failed");
    }

#if FACTORY_RESET_GPIO >= 0
    // Check for factory reset AFTER boot is complete to avoid download mode
    // Wait for system to fully boot first
    vTaskDelay(pdMS_TO_TICKS(500));

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << FACTORY_RESET_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // Check if button is held low (for factory reset)
    if (gpio_get_level(FACTORY_RESET_GPIO) == 0) {
        ESP_LOGW(TAG, "Factory reset button detected, hold for 3 seconds to confirm...");

        // Wait and check if button stays pressed
        bool reset_confirmed = true;
        for (int i = 0; i < 30; i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
            if (gpio_get_level(FACTORY_RESET_GPIO) != 0) {
                reset_confirmed = false;
                ESP_LOGI(TAG, "Factory reset cancelled");
                break;
            }
        }

        if (reset_confirmed) {
            ESP_LOGW(TAG, "Factory reset confirmed! Erasing WiFi credentials...");
            wifi_prov_reset();
            ESP_LOGI(TAG, "Factory reset complete. Rebooting...");
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
        }
    }
#endif

    // Initialize provisioning
    wifi_prov_init();    // Check if device is provisioned
    if (!wifi_prov_is_provisioned()) {
        ESP_LOGI(TAG, "Device not provisioned. Starting provisioning AP...");
        ESP_LOGI(TAG, "Connect to WiFi: %s", PROV_AP_SSID);
        ESP_LOGI(TAG, "Password: %s", PROV_AP_PASSWORD);
        ESP_LOGI(TAG, "Then open http://192.168.4.1 in your browser");

        wifi_prov_start_ap();

        // Wait in provisioning mode (device will restart after config is saved)
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    // Device is provisioned, get configuration
    wifi_config_data_t wifi_config;
    ret = wifi_prov_get_config(&wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get WiFi config");
        return;
    }

    // Validate that we actually have WiFi credentials
    if (strlen(wifi_config.ssid) == 0) {
        ESP_LOGW(TAG, "Provisioned flag set but no SSID found - resetting provisioning");
        wifi_prov_reset();
        esp_restart();
        return;
    }

    ESP_LOGI(TAG, "Device provisioned. Node name: %s",
             wifi_config.node_name[0] ? wifi_config.node_name : "Unknown");

    // Initialize and connect to WiFi using stored credentials
    ESP_LOGI(TAG, "Connecting to WiFi: %s", wifi_config.ssid);
    ret = wifi_init();
    if (ret == ESP_OK) {
        // Keep retrying indefinitely until connected
        while (wifi_connect(wifi_config.ssid, wifi_config.password, WIFI_MAX_RETRY) != ESP_OK) {
            ESP_LOGW(TAG, "WiFi connection failed. Retrying in 10 seconds...");
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
        ESP_LOGI(TAG, "WiFi connected successfully");
    }

    // Use server URL from config if available
    const char *ota_url = wifi_config.server_url[0] ? wifi_config.server_url : OTA_SERVER_URL;
    ESP_LOGI(TAG, "OTA Server: %s", ota_url);

    // Main application loop
    int count = 0;
    TickType_t last_ota_check = 0;

    while (1) {
        ESP_LOGI(TAG, "[%s] Application running... count: %d",
                 wifi_config.node_name[0] ? wifi_config.node_name : "ESP32", count++);

        // Check for OTA updates periodically (only if WiFi is connected)
        if (wifi_is_connected()) {
            TickType_t now = xTaskGetTickCount();
            if ((now - last_ota_check) * portTICK_PERIOD_MS >= OTA_CHECK_INTERVAL_MS) {
                ESP_LOGI(TAG, "Time to check for OTA updates");
                // Uncomment to enable automatic OTA updates:
                // ota_check_and_update(ota_url);
                last_ota_check = now;
            }
        } else {
            ESP_LOGW(TAG, "WiFi not connected, skipping OTA check");
        }

        // Sleep using FreeRTOS delay (allows other tasks to run, no watchdog issues)
        vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
    }
}