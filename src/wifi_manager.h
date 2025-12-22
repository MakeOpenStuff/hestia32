#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * Initialize WiFi in station mode
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_init(void);

/**
 * Connect to WiFi access point
 * @param ssid WiFi SSID
 * @param password WiFi password
 * @param max_retry Maximum connection retry attempts
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_connect(const char *ssid, const char *password, int max_retry);

/**
 * Disconnect from WiFi
 */
void wifi_disconnect(void);

/**
 * Check if WiFi is connected
 * @return true if connected, false otherwise
 */
bool wifi_is_connected(void);

#endif // WIFI_MANAGER_H
