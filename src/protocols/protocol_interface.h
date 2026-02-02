#ifndef PROTOCOL_INTERFACE_H
#define PROTOCOL_INTERFACE_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/**
 * @brief Abstract protocol interface for different communication protocols
 *
 * This interface allows the firmware to support multiple protocols (MQTT, Zigbee, Matter)
 * by implementing a common API. Each protocol implementation provides these functions.
 */

/**
 * @brief Protocol initialization function type
 *
 * Called once at boot to initialize the protocol stack.
 * Does not start networking/communication yet.
 *
 * @return ESP_OK on success, error code otherwise
 */
typedef esp_err_t (*protocol_init_fn)(void);

/**
 * @brief Protocol start function type
 *
 * Called after initialization to start networking/communication.
 * May block during provisioning/pairing or return immediately if already configured.
 *
 * @return ESP_OK on success, error code otherwise
 */
typedef esp_err_t (*protocol_start_fn)(void);

/**
 * @brief Protocol stop function type
 *
 * Gracefully stops protocol communication (e.g., disconnect from network).
 *
 * @return ESP_OK on success, error code otherwise
 */
typedef esp_err_t (*protocol_stop_fn)(void);

/**
 * @brief Report current temperature to protocol
 *
 * @param temperature Temperature in Celsius
 * @return ESP_OK on success, error code otherwise
 */
typedef esp_err_t (*protocol_report_temperature_fn)(float temperature);

/**
 * @brief Report current humidity to protocol
 *
 * @param humidity Relative humidity percentage (0-100)
 * @return ESP_OK on success, error code otherwise
 */
typedef esp_err_t (*protocol_report_humidity_fn)(float humidity);

/**
 * @brief Report setpoint values to protocol
 *
 * @param heat_setpoint Heating setpoint in Celsius
 * @param cool_setpoint Cooling setpoint in Celsius
 * @return ESP_OK on success, error code otherwise
 */
typedef esp_err_t (*protocol_report_setpoint_fn)(float heat_setpoint, float cool_setpoint);

/**
 * @brief Report thermostat mode to protocol
 *
 * @param mode Mode string: "off", "heat", "cool", "auto"
 * @return ESP_OK on success, error code otherwise
 */
typedef esp_err_t (*protocol_report_mode_fn)(const char* mode);

/**
 * @brief Report relay states to protocol
 *
 * @param heating True if heating relay is active
 * @param cooling True if cooling relay is active
 * @param fan True if fan relay is active
 * @param humidifier True if humidifier relay is active
 * @param dehumidifier True if dehumidifier relay is active
 * @param hot_water True if hot water relay is active
 * @return ESP_OK on success, error code otherwise
 */
typedef esp_err_t (*protocol_report_relay_state_fn)(bool heating, bool cooling,
                                                     bool fan, bool humidifier,
                                                     bool dehumidifier, bool hot_water);

/**
 * @brief Report boost state to protocol
 *
 * @param domain Domain name: "heating", "cooling", "hot_water"
 * @param remaining_sec Seconds remaining (0 if not active)
 * @return ESP_OK on success, error code otherwise
 */
typedef esp_err_t (*protocol_report_boost_state_fn)(const char* domain, uint32_t remaining_sec);

/**
 * @brief Check if protocol is connected/ready
 *
 * @return true if connected and ready to communicate, false otherwise
 */
typedef bool (*protocol_is_connected_fn)(void);

/**
 * @brief Check if device is provisioned/paired
 *
 * @return true if device is provisioned, false otherwise
 */
typedef bool (*protocol_is_provisioned_fn)(void);

/**
 * @brief Start provisioning/pairing process
 *
 * @return ESP_OK on success, error code otherwise
 */
typedef esp_err_t (*protocol_start_provisioning_fn)(void);

/**
 * @brief Get protocol name
 *
 * @return Protocol name string (e.g., "MQTT", "Zigbee", "Matter")
 */
typedef const char* (*protocol_get_name_fn)(void);

/**
 * @brief Protocol interface structure
 *
 * Each protocol implementation fills this structure with function pointers.
 */
typedef struct {
    protocol_init_fn init;
    protocol_start_fn start;
    protocol_stop_fn stop;
    protocol_report_temperature_fn report_temperature;
    protocol_report_humidity_fn report_humidity;
    protocol_report_setpoint_fn report_setpoint;
    protocol_report_mode_fn report_mode;
    protocol_report_relay_state_fn report_relay_state;
    protocol_report_boost_state_fn report_boost_state;
    protocol_is_connected_fn is_connected;
    protocol_is_provisioned_fn is_provisioned;
    protocol_start_provisioning_fn start_provisioning;
    protocol_get_name_fn get_name;
} protocol_interface_t;

/**
 * @brief Protocol command callbacks
 *
 * The protocol layer calls these functions when commands are received.
 * The application should register these callbacks with the protocol manager.
 */
typedef struct {
    /**
     * @brief Callback when setpoint change command is received
     *
     * @param heat_setpoint New heating setpoint in Celsius
     * @param cool_setpoint New cooling setpoint in Celsius
     */
    void (*on_setpoint_change)(float heat_setpoint, float cool_setpoint);

    /**
     * @brief Callback when mode change command is received
     *
     * @param mode Mode string: "off", "heat", "cool", "auto"
     */
    void (*on_mode_change)(const char* mode);

    /**
     * @brief Callback when boost command is received
     *
     * @param domain Domain name: "heating", "cooling", "hot_water"
     * @param duration_sec Boost duration in seconds (0 to cancel)
     */
    void (*on_boost_command)(const char* domain, uint32_t duration_sec);
} protocol_callbacks_t;

#endif // PROTOCOL_INTERFACE_H
