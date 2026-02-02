#ifndef WIFI_PROVISIONING_H
#define WIFI_PROVISIONING_H

#include "esp_err.h"
#include <stdbool.h>

// Maximum length for configuration strings
#define MAX_SSID_LEN 32
#define MAX_PASSWORD_LEN 64
#define MAX_SERVER_URL_LEN 128
#define MAX_NODE_NAME_LEN 32

// Configuration structure stored in NVS
typedef struct {
    char ssid[MAX_SSID_LEN];
    char password[MAX_PASSWORD_LEN];
    char server_url[MAX_SERVER_URL_LEN];
    char node_name[MAX_NODE_NAME_LEN];
    bool provisioned;
    
    // OTA update settings
    bool ota_auto_update;           // Enable automatic OTA checks
    uint8_t ota_release_channel;    // 0 = stable, 1 = develop (includes pre-releases)
    uint32_t ota_check_interval;    // Check interval in hours
    uint64_t ota_last_check;        // Unix timestamp of last check
} wifi_config_data_t;

/**
 * Initialize WiFi provisioning system
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_prov_init(void);

/**
 * Check if device is already provisioned
 * @return true if provisioned, false otherwise
 */
bool wifi_prov_is_provisioned(void);

/**
 * Start WiFi provisioning AP mode
 * Creates an access point for configuration
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_prov_start_ap(void);

/**
 * Stop WiFi provisioning AP mode
 */
void wifi_prov_stop_ap(void);

/**
 * Get stored WiFi configuration
 * @param config Pointer to config structure to fill
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_prov_get_config(wifi_config_data_t *config);

/**
 * Save WiFi configuration to NVS
 * @param config Pointer to config structure to save
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_prov_save_config(const wifi_config_data_t *config);

/**
 * Reset provisioning (clear stored credentials)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_prov_reset(void);

#endif // WIFI_PROVISIONING_H
