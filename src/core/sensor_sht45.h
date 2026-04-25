#ifndef SENSOR_SHT45_H
#define SENSOR_SHT45_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SHT45 I2C address
 */
#define SHT45_I2C_ADDR 0x44

/**
 * @brief Initialize SHT45 sensor
 *
 * @param scl_pin I2C SCL GPIO pin
 * @param sda_pin I2C SDA GPIO pin
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sht45_init(int scl_pin, int sda_pin);

/**
 * @brief Read temperature and humidity from SHT45
 *
 * @param temperature Pointer to store temperature in Celsius
 * @param humidity Pointer to store relative humidity percentage
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sht45_read(float *temperature, float *humidity);

/**
 * @brief Check if sensor is available
 *
 * @return true if sensor responds
 */
bool sht45_is_available(void);

/**
 * @brief Deinitialize SHT45 sensor and I2C driver
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sht45_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // SENSOR_SHT45_H
