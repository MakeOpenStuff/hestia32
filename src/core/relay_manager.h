#ifndef RELAY_MANAGER_H
#define RELAY_MANAGER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize relay control system
 *
 * Automatically detects and initializes either:
 * - Direct GPIO control (DevKit)
 * - TCA9555 I2C GPIO expander (XIAO)
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t relay_manager_init(void);

/**
 * @brief Deinitialize relay control system
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t relay_manager_deinit(void);

/**
 * @brief Set relay state
 *
 * @param relay_num Relay number (0-3)
 * @param state Relay state (true=ON, false=OFF)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t relay_manager_set_relay(uint8_t relay_num, bool state);

/**
 * @brief Get relay state
 *
 * @param relay_num Relay number (0-3)
 * @param state Pointer to store relay state (true=ON, false=OFF)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t relay_manager_get_relay(uint8_t relay_num, bool *state);

/**
 * @brief Set all relays at once
 *
 * @param states 4-bit value where bit 0=relay0, bit 1=relay1, etc.
 * @return esp_err_t ESP_OK on success
 */
esp_err_t relay_manager_set_all(uint8_t states);

/**
 * @brief Check if using TCA9555 I2C expander
 *
 * @return true if TCA9555 mode, false if direct GPIO mode
 */
bool relay_manager_using_tca9555(void);

#ifdef __cplusplus
}
#endif

#endif // RELAY_MANAGER_H
