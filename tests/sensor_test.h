#ifndef SENSOR_TEST_H
#define SENSOR_TEST_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize sensor test module
 *
 * This initializes the SHT45 sensor on pins 4 (SCL) and 5 (SDA)
 * and initializes user settings for temperature unit preference.
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sensor_test_init(void);

/**
 * @brief Read and print sensor data to serial console
 *
 * Reads temperature and humidity from SHT45 and prints:
 * - Raw temperature in Celsius
 * - Temperature in user's preferred unit (C or F)
 * - Humidity percentage
 *
 * Call this periodically from your main loop for testing.
 * Comment out the call once you've verified the sensor is working.
 */
void sensor_test_read_and_print(void);

/**
 * @brief Start periodic sensor reading task
 *
 * Creates a FreeRTOS task that reads and prints sensor data every 5 seconds.
 * Useful for testing without modifying main loop.
 *
 * @param period_ms Reading period in milliseconds (default: 5000ms)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sensor_test_start_periodic_task(uint32_t period_ms);

/**
 * @brief Stop periodic sensor reading task
 */
void sensor_test_stop_periodic_task(void);

#ifdef __cplusplus
}
#endif

#endif // SENSOR_TEST_H
