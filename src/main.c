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
#include "lvgl.h"

static const char *TAG = "main";

// Task to handle display during provisioning
static void display_provisioning_task(void* param)
{
    esp_err_t ret = display_init();
    if (ret == ESP_OK) {
        display_create_ui(true, true);
        // Very slow updates to minimize CPU usage
        while (1) {
            display_update();
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    vTaskDelete(NULL);
}

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

    // Check for factory reset button (BOOT button held during boot)
#if FACTORY_RESET_GPIO >= 0
    ESP_LOGI(TAG, "Checking BOOT button on GPIO %d...", FACTORY_RESET_GPIO);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << FACTORY_RESET_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Give GPIO time to settle
    vTaskDelay(pdMS_TO_TICKS(50));

    int initial_level = gpio_get_level((gpio_num_t)FACTORY_RESET_GPIO);
    ESP_LOGI(TAG, "BOOT button GPIO %d level: %d (0=pressed, 1=released)",
             FACTORY_RESET_GPIO, initial_level);

    if (initial_level == 0) {
        ESP_LOGW(TAG, "BOOT button pressed - checking for factory reset...");
        ESP_LOGW(TAG, "Hold BOOT button for 3 seconds to erase settings");

        int hold_count = 0;
        for (int i = 0; i < 30; i++) {  // Check for 3 seconds (30 * 100ms)
            vTaskDelay(pdMS_TO_TICKS(100));
            if (gpio_get_level((gpio_num_t)FACTORY_RESET_GPIO) == 0) {
                hold_count++;
            } else {
                ESP_LOGI(TAG, "BOOT button released - continuing normal boot");
                break;
            }
        }

        if (hold_count >= 30) {
            ESP_LOGW(TAG, "========================================");
            ESP_LOGW(TAG, "Factory reset confirmed!");
            ESP_LOGW(TAG, "Erasing WiFi credentials and touchscreen calibration...");
            ESP_LOGW(TAG, "========================================");

            // Erase all NVS data (includes WiFi config and touch calibration)
            ESP_ERROR_CHECK(nvs_flash_erase());
            ESP_ERROR_CHECK(nvs_flash_init());

            ESP_LOGW(TAG, "Factory reset complete. Rebooting...");
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
        }
    }
#endif

// PRIORITY 1: Check if calibration exists - run wizard if needed BEFORE WiFi provisioning
    bool has_calibration = display_has_calibration();

    if (!has_calibration) {
        ESP_LOGI(TAG, "No calibration found - running wizard first");
        ret = display_init();
        if (ret == ESP_OK) {
            display_create_ui(false, false);  // Run calibration wizard, full UI
            ESP_LOGI(TAG, "Calibration complete - clearing display");

            // Clear display before restart
            display_clear_screen();
            display_update();
        }
        // Restart to apply calibration and continue with WiFi provisioning
        ESP_LOGI(TAG, "Restarting to continue with WiFi provisioning...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }

    wifi_prov_init();
    bool is_provisioned = wifi_prov_is_provisioned();

    if (!is_provisioned) {
        // Start WiFi AP FIRST
        ESP_LOGI(TAG, "Device not provisioned. Starting provisioning AP...");
        ESP_LOGI(TAG, "Connect to WiFi: %s", PROV_AP_SSID);
        ESP_LOGI(TAG, "Password: %s", PROV_AP_PASSWORD);
        ESP_LOGI(TAG, "Then open http://192.168.4.1 in your browser");

        wifi_prov_start_ap();
        ESP_LOGI(TAG, "Provisioning AP started");

        // Wait for DHCP to initialize
        vTaskDelay(pdMS_TO_TICKS(2000));

        // Initialize display on a low priority task (priority 2) to not interfere with DHCP
        xTaskCreate(display_provisioning_task, "display_prov", 4096, NULL, 2, NULL);

        // Main task just waits (device will restart after provisioning)
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    // Device is provisioned and calibrated - initialize display with full UI
    ESP_LOGI(TAG, "Initializing display...");
    ret = display_init();
    if (ret == ESP_OK) {
        display_create_ui(false, false);  // Load calibration from NVS, show full UI
        display_start_lvgl_task();  // Start dedicated LVGL task for UI updates
        ESP_LOGI(TAG, "Display initialized successfully");
    } else {
        ESP_LOGE(TAG, "Display initialization failed");
    }

    // Connect to WiFi (device is provisioned)
    wifi_config_data_t wifi_config;
    ret = wifi_prov_get_config(&wifi_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Device provisioned. Node name: %s",
                 wifi_config.node_name[0] ? wifi_config.node_name : "Unknown");

        // Initialize and connect to WiFi
        ESP_LOGI(TAG, "Connecting to WiFi: %s", wifi_config.ssid);
        ret = wifi_init();
        if (ret == ESP_OK) {
            wifi_connect(wifi_config.ssid, wifi_config.password, WIFI_MAX_RETRY);
        }
    } else {
        ESP_LOGE(TAG, "Failed to get WiFi config");
    }

    // Initialize OTA manager (checks boot partition, marks as valid if needed)
    ret = ota_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OTA initialization failed");
    }

    // Main task can now suspend - LVGL runs in its own task
    ESP_LOGI(TAG, "Main initialization complete. Main task suspending.");

    // Optional: Add application logic here or just suspend
    while (1) {
        // Main task suspended, other tasks (LVGL, WiFi, etc.) continue running
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}