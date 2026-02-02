#ifndef PROTOCOL_MANAGER_H
#define PROTOCOL_MANAGER_H

#include "protocols/protocol_interface.h"
#include "esp_err.h"

/**
 * @brief Protocol Manager
 *
 * Manages the active protocol implementation and provides a unified interface
 * for the application to communicate with the selected protocol (MQTT/Zigbee/Matter).
 */

/**
 * @brief Initialize the protocol manager
 *
 * Selects and initializes the protocol based on build-time configuration.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t protocol_manager_init(void);

/**
 * @brief Start the active protocol
 *
 * Starts networking/communication for the selected protocol.
 * May block during provisioning/pairing.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t protocol_manager_start(void);

/**
 * @brief Stop the active protocol
 *
 * Gracefully stops protocol communication.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t protocol_manager_stop(void);

/**
 * @brief Register application callbacks for protocol commands
 *
 * @param callbacks Pointer to callback structure
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t protocol_manager_register_callbacks(const protocol_callbacks_t* callbacks);

/**
 * @brief Get the active protocol interface
 *
 * Returns the protocol interface for direct access to protocol functions.
 *
 * @return Pointer to protocol interface, or NULL if not initialized
 */
const protocol_interface_t* protocol_manager_get_interface(void);

/**
 * @brief Check if protocol is connected
 *
 * @return true if connected and ready, false otherwise
 */
bool protocol_manager_is_connected(void);

/**
 * @brief Get active protocol name
 *
 * @return Protocol name string (e.g., "MQTT", "Zigbee", "Matter")
 */
const char* protocol_manager_get_name(void);

#endif // PROTOCOL_MANAGER_H
