#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include "esp_err.h"
#include "protocols/mqtt/wifi_provisioning.h"

/**
 * Initialize OTA manager
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_init(void);

/**
 * Perform OTA update from HTTPS server
 * @param url URL of the firmware binary
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_update_from_url(const char *url);

/**
 * Get current firmware version
 * @return Version string
 */
const char* ota_get_version(void);

/**
 * Check and perform OTA update if available
 * This is a simple implementation that always attempts update
 * In production, you should check version/hash before updating
 *
 * @param url URL to check for firmware
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_check_and_update(const char *url);

/**
 * Check GitHub releases for new firmware and update if available
 * Validates partition space, compares versions, and downloads only if newer
 * 
 * @param release_channel 0 = stable (latest release only), 1 = develop (all releases including pre-release)
 * @return ESP_OK if no update needed or update successful, error code otherwise
 */
esp_err_t ota_check_github_release(uint8_t release_channel);

/**
 * Start the periodic OTA check task
 * Creates a background task that checks for updates at the configured interval
 * 
 * @param config Pointer to WiFi configuration (must remain valid for task lifetime)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_start_check_task(wifi_config_data_t *config);

#endif // OTA_MANAGER_H
