#include "protocols/mqtt/ota_manager.h"
#include "core/core_config.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_app_format.h"

static const char *TAG = "ota";

esp_err_t ota_init(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;

    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            // First boot after OTA update
            ESP_LOGI(TAG, "OTA update successful, marking as valid");
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }

    ESP_LOGI(TAG, "Running partition: %s (offset 0x%08lx)",
             running->label, running->address);
    ESP_LOGI(TAG, "Firmware version: %s", APP_VERSION);

    return ESP_OK;
}

esp_err_t ota_update_from_url(const char *url)
{
    ESP_LOGI(TAG, "Starting OTA update from: %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 5000,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    ESP_LOGI(TAG, "Attempting to download update...");
    esp_err_t ret = esp_https_ota(&ota_config);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA update successful! Rebooting...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(ret));
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
