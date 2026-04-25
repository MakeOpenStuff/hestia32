#ifndef TCA9555_H
#define TCA9555_H

#include "esp_err.h"
#include "driver/i2c.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// TCA9555 I2C address (can be 0x20-0x27 depending on A0/A1/A2 pins)
#define TCA9555_DEFAULT_ADDR 0x20

// TCA9555 register addresses
#define TCA9555_REG_INPUT_PORT0     0x00
#define TCA9555_REG_INPUT_PORT1     0x01
#define TCA9555_REG_OUTPUT_PORT0    0x02
#define TCA9555_REG_OUTPUT_PORT1    0x03
#define TCA9555_REG_POLARITY_PORT0  0x04
#define TCA9555_REG_POLARITY_PORT1  0x05
#define TCA9555_REG_CONFIG_PORT0    0x06
#define TCA9555_REG_CONFIG_PORT1    0x07

// TCA9555 pin direction
#define TCA9555_PIN_INPUT  1
#define TCA9555_PIN_OUTPUT 0

/**
 * @brief Initialize TCA9555 I2C GPIO expander
 *
 * @param i2c_port I2C port number
 * @param sda_pin GPIO pin for I2C SDA
 * @param scl_pin GPIO pin for I2C SCL
 * @param i2c_addr TCA9555 I2C address (typically 0x20)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t tca9555_init(i2c_port_t i2c_port, int sda_pin, int scl_pin, uint8_t i2c_addr);

/**
 * @brief Deinitialize TCA9555 and I2C driver
 *
 * @param i2c_port I2C port number
 * @return esp_err_t ESP_OK on success
 */
esp_err_t tca9555_deinit(i2c_port_t i2c_port);

/**
 * @brief Probe if TCA9555 device exists at specified address
 *
 * @param i2c_port I2C port number
 * @param i2c_addr TCA9555 I2C address to probe
 * @return true if device found, false otherwise
 */
bool tca9555_probe(i2c_port_t i2c_port, uint8_t i2c_addr);

/**
 * @brief Set pin direction (input or output)
 *
 * @param i2c_port I2C port number
 * @param i2c_addr TCA9555 I2C address
 * @param pin Pin number (0-15)
 * @param direction TCA9555_PIN_INPUT or TCA9555_PIN_OUTPUT
 * @return esp_err_t ESP_OK on success
 */
esp_err_t tca9555_set_pin_mode(i2c_port_t i2c_port, uint8_t i2c_addr, uint8_t pin, uint8_t direction);

/**
 * @brief Set output pin level
 *
 * @param i2c_port I2C port number
 * @param i2c_addr TCA9555 I2C address
 * @param pin Pin number (0-15)
 * @param level Pin level (0=low, 1=high)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t tca9555_set_pin_level(i2c_port_t i2c_port, uint8_t i2c_addr, uint8_t pin, uint8_t level);

/**
 * @brief Read input pin level
 *
 * @param i2c_port I2C port number
 * @param i2c_addr TCA9555 I2C address
 * @param pin Pin number (0-15)
 * @param level Pointer to store pin level (0=low, 1=high)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t tca9555_get_pin_level(i2c_port_t i2c_port, uint8_t i2c_addr, uint8_t pin, uint8_t *level);

/**
 * @brief Set multiple output pins at once
 *
 * @param i2c_port I2C port number
 * @param i2c_addr TCA9555 I2C address
 * @param port0_value Output value for port 0 (pins 0-7)
 * @param port1_value Output value for port 1 (pins 8-15)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t tca9555_set_ports(i2c_port_t i2c_port, uint8_t i2c_addr, uint8_t port0_value, uint8_t port1_value);

#ifdef __cplusplus
}
#endif

#endif // TCA9555_H
