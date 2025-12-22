#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include "esp_err.h"

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

#endif // OTA_MANAGER_H
