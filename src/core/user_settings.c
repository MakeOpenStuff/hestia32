#include "user_settings.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "user_settings";
#define NVS_NAMESPACE "user_settings"
#define NVS_KEY_TEMP_UNIT "temp_unit"

static user_settings_t current_settings = {
		.temp_unit = TEMP_UNIT_CELSIUS  // Default to Celsius
};

static bool settings_initialized = false;

esp_err_t user_settings_init(void) {
		if (settings_initialized) {
				return ESP_OK;
		}

		nvs_handle_t nvs_handle;
		esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);

		if (err != ESP_OK) {
				ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
				return err;
		}

		// Try to read temperature unit preference
		uint8_t temp_unit = (uint8_t)TEMP_UNIT_CELSIUS;
		err = nvs_get_u8(nvs_handle, NVS_KEY_TEMP_UNIT, &temp_unit);

		if (err == ESP_ERR_NVS_NOT_FOUND) {
				// First time - save default (Celsius)
				ESP_LOGI(TAG, "No temperature unit preference found, defaulting to Celsius");
				err = nvs_set_u8(nvs_handle, NVS_KEY_TEMP_UNIT, (uint8_t)TEMP_UNIT_CELSIUS);
				if (err == ESP_OK) {
						err = nvs_commit(nvs_handle);
				}
				current_settings.temp_unit = TEMP_UNIT_CELSIUS;
		} else if (err == ESP_OK) {
				// Validate and load
				if (temp_unit <= TEMP_UNIT_FAHRENHEIT) {
						current_settings.temp_unit = (temp_unit_t)temp_unit;
						ESP_LOGI(TAG, "Loaded temperature unit preference: %s",
										 (temp_unit == TEMP_UNIT_CELSIUS) ? "Celsius" : "Fahrenheit");
				} else {
						ESP_LOGW(TAG, "Invalid temperature unit value, resetting to Celsius");
						current_settings.temp_unit = TEMP_UNIT_CELSIUS;
						nvs_set_u8(nvs_handle, NVS_KEY_TEMP_UNIT, (uint8_t)TEMP_UNIT_CELSIUS);
						nvs_commit(nvs_handle);
				}
		} else {
				ESP_LOGE(TAG, "Failed to read temperature unit: %s", esp_err_to_name(err));
		}

		nvs_close(nvs_handle);
		settings_initialized = true;
		return ESP_OK;
}

esp_err_t user_settings_get(user_settings_t *settings) {
		if (!settings_initialized) {
				esp_err_t err = user_settings_init();
				if (err != ESP_OK) {
						return err;
				}
		}

		if (settings == NULL) {
				return ESP_ERR_INVALID_ARG;
		}

		*settings = current_settings;
		return ESP_OK;
}

esp_err_t user_settings_set_temp_unit(temp_unit_t unit) {
		if (unit > TEMP_UNIT_FAHRENHEIT) {
				return ESP_ERR_INVALID_ARG;
		}

		if (!settings_initialized) {
				esp_err_t err = user_settings_init();
				if (err != ESP_OK) {
						return err;
				}
		}

		nvs_handle_t nvs_handle;
		esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);

		if (err != ESP_OK) {
				ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
				return err;
		}

		err = nvs_set_u8(nvs_handle, NVS_KEY_TEMP_UNIT, (uint8_t)unit);
		if (err == ESP_OK) {
				err = nvs_commit(nvs_handle);
				if (err == ESP_OK) {
						current_settings.temp_unit = unit;
						ESP_LOGI(TAG, "Temperature unit set to: %s",
										 (unit == TEMP_UNIT_CELSIUS) ? "Celsius" : "Fahrenheit");
				}
		}

		nvs_close(nvs_handle);
		return err;
}

temp_unit_t user_settings_get_temp_unit(void) {
		if (!settings_initialized) {
				user_settings_init();
		}
		return current_settings.temp_unit;
}

float temp_c_to_f(float celsius) {
		return (celsius * 9.0f / 5.0f) + 32.0f;
}

float temp_f_to_c(float fahrenheit) {
		return (fahrenheit - 32.0f) * 5.0f / 9.0f;
}

float temp_to_user_unit(float celsius) {
		if (!settings_initialized) {
				user_settings_init();
		}

		if (current_settings.temp_unit == TEMP_UNIT_FAHRENHEIT) {
				return temp_c_to_f(celsius);
		}
		return celsius;
}

const char* user_settings_get_temp_unit_symbol(void) {
		if (!settings_initialized) {
				user_settings_init();
		}

		return (current_settings.temp_unit == TEMP_UNIT_CELSIUS) ? "°C" : "°F";
}
