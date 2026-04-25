#ifndef USER_SETTINGS_H
#define USER_SETTINGS_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Temperature unit preference
 */
typedef enum {
		TEMP_UNIT_CELSIUS = 0,
		TEMP_UNIT_FAHRENHEIT = 1
} temp_unit_t;

/**
 * @brief User settings structure
 */
typedef struct {
		temp_unit_t temp_unit;
} user_settings_t;

/**
 * @brief Initialize user settings from NVS
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t user_settings_init(void);

/**
 * @brief Get current user settings
 *
 * @param settings Pointer to store settings
 * @return esp_err_t ESP_OK on success
 */
esp_err_t user_settings_get(user_settings_t *settings);

/**
 * @brief Set temperature unit preference
 *
 * @param unit Temperature unit (TEMP_UNIT_CELSIUS or TEMP_UNIT_FAHRENHEIT)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t user_settings_set_temp_unit(temp_unit_t unit);

/**
 * @brief Get temperature unit preference
 *
 * @return temp_unit_t Current temperature unit
 */
temp_unit_t user_settings_get_temp_unit(void);

/**
 * @brief Convert Celsius to Fahrenheit
 *
 * @param celsius Temperature in Celsius
 * @return float Temperature in Fahrenheit
 */
float temp_c_to_f(float celsius);

/**
 * @brief Convert Fahrenheit to Celsius
 *
 * @param fahrenheit Temperature in Fahrenheit
 * @return float Temperature in Celsius
 */
float temp_f_to_c(float fahrenheit);

/**
 * @brief Convert temperature to user's preferred unit
 *
 * @param celsius Temperature in Celsius
 * @return float Temperature in user's preferred unit
 */
float temp_to_user_unit(float celsius);

/**
 * @brief Get temperature unit symbol string
 *
 * @return const char* "°C" or "°F"
 */
const char* user_settings_get_temp_unit_symbol(void);

#ifdef __cplusplus
}
#endif

#endif // USER_SETTINGS_H
