#include "sensor_test.h"
#include "sensor_sht45.h"
#include "user_settings.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sensor_test";

// Pin definitions for SHT45 sensor
#define SENSOR_SCL_PIN 4
#define SENSOR_SDA_PIN 5

static TaskHandle_t sensor_task_handle = NULL;
static bool sensor_initialized = false;

esp_err_t sensor_test_init(void) {
		if (sensor_initialized) {
				ESP_LOGW(TAG, "Sensor test already initialized");
				return ESP_OK;
		}

		ESP_LOGI(TAG, "========================================");
		ESP_LOGI(TAG, "Initializing SHT45 sensor test");
		ESP_LOGI(TAG, "========================================");

		// Initialize user settings (temperature unit preference)
		esp_err_t ret = user_settings_init();
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Failed to initialize user settings: %s", esp_err_to_name(ret));
				return ret;
		}

		// Initialize SHT45 sensor
		ret = sht45_init(SENSOR_SCL_PIN, SENSOR_SDA_PIN);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Failed to initialize SHT45 sensor: %s", esp_err_to_name(ret));
				ESP_LOGE(TAG, "Check wiring:");
				ESP_LOGE(TAG, "  SCL -> GPIO %d", SENSOR_SCL_PIN);
				ESP_LOGE(TAG, "  SDA -> GPIO %d", SENSOR_SDA_PIN);
				ESP_LOGE(TAG, "  VCC -> 3.3V");
				ESP_LOGE(TAG, "  GND -> GND");
				return ret;
		}

		sensor_initialized = true;
		ESP_LOGI(TAG, "Sensor test initialized successfully");
		ESP_LOGI(TAG, "Temperature unit preference: %s", user_settings_get_temp_unit_symbol());
		ESP_LOGI(TAG, "========================================");

		return ESP_OK;
}

void sensor_test_read_and_print(void) {
		if (!sensor_initialized) {
				ESP_LOGW(TAG, "Sensor not initialized, call sensor_test_init() first");
				return;
		}

		float temp_c, humidity;
		esp_err_t ret = sht45_read(&temp_c, &humidity);

		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Failed to read sensor: %s", esp_err_to_name(ret));
				return;
		}

		// Convert to user's preferred unit
		float temp_user = temp_to_user_unit(temp_c);
		const char* unit_symbol = user_settings_get_temp_unit_symbol();

		// Print formatted output
		ESP_LOGI(TAG, "┌─────────────────────────────────────┐");
		ESP_LOGI(TAG, "│ SHT45 Sensor Reading                │");
		ESP_LOGI(TAG, "├─────────────────────────────────────┤");
		ESP_LOGI(TAG, "│ Temperature: %6.2f %-4s           │", temp_user, unit_symbol);
		ESP_LOGI(TAG, "│ Humidity:    %6.1f %%              │", humidity);

		// Also show raw Celsius if user prefers Fahrenheit
		if (user_settings_get_temp_unit() == TEMP_UNIT_FAHRENHEIT) {
				ESP_LOGI(TAG, "│ (Raw: %.2f°C)                      │", temp_c);
		}

		ESP_LOGI(TAG, "└─────────────────────────────────────┘");
}

static void sensor_periodic_task(void *pvParameters) {
		uint32_t period_ms = *((uint32_t*)pvParameters);

		ESP_LOGI(TAG, "Periodic sensor reading task started (period: %lu ms)", period_ms);

		while (1) {
				sensor_test_read_and_print();
				vTaskDelay(pdMS_TO_TICKS(period_ms));
		}
}

esp_err_t sensor_test_start_periodic_task(uint32_t period_ms) {
		if (!sensor_initialized) {
				ESP_LOGE(TAG, "Sensor not initialized, call sensor_test_init() first");
				return ESP_ERR_INVALID_STATE;
		}

		if (sensor_task_handle != NULL) {
				ESP_LOGW(TAG, "Periodic task already running");
				return ESP_OK;
		}

		if (period_ms < 1000) {
				ESP_LOGW(TAG, "Period too short, setting to minimum 1000ms");
				period_ms = 1000;
		}

		static uint32_t period_storage = 0;
		period_storage = period_ms;

		BaseType_t ret = xTaskCreate(
				sensor_periodic_task,
				"sensor_test",
				4096,
				&period_storage,
				5,  // Priority
				&sensor_task_handle
		);

		if (ret != pdPASS) {
				ESP_LOGE(TAG, "Failed to create periodic task");
				sensor_task_handle = NULL;
				return ESP_FAIL;
		}

		ESP_LOGI(TAG, "Periodic sensor reading task created");
		return ESP_OK;
}

void sensor_test_stop_periodic_task(void) {
		if (sensor_task_handle != NULL) {
				vTaskDelete(sensor_task_handle);
				sensor_task_handle = NULL;
				ESP_LOGI(TAG, "Periodic sensor reading task stopped");
		}
}
