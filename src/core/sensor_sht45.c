#include "sensor_sht45.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sht45";

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000  // 100kHz
#define I2C_MASTER_TIMEOUT_MS 1000

// SHT45 Commands
#define SHT45_CMD_MEASURE_HIGH_PRECISION 0xFD
#define SHT45_CMD_READ_SERIAL 0x89
#define SHT45_CMD_SOFT_RESET 0x94

static bool sht45_initialized = false;

/**
 * @brief Calculate CRC-8 for SHT45 data validation
 */
static uint8_t sht45_crc8(const uint8_t *data, int len) {
		const uint8_t POLYNOMIAL = 0x31;
		uint8_t crc = 0xFF;

		for (int j = len; j; --j) {
				crc ^= *data++;
				for (int i = 8; i; --i) {
						crc = (crc & 0x80) ? (crc << 1) ^ POLYNOMIAL : (crc << 1);
				}
		}
		return crc;
}

esp_err_t sht45_init(int scl_pin, int sda_pin) {
		if (sht45_initialized) {
				ESP_LOGW(TAG, "SHT45 already initialized");
				return ESP_OK;
		}

		i2c_config_t conf = {
				.mode = I2C_MODE_MASTER,
				.sda_io_num = sda_pin,
				.scl_io_num = scl_pin,
				.sda_pullup_en = GPIO_PULLUP_ENABLE,
				.scl_pullup_en = GPIO_PULLUP_ENABLE,
				.master.clk_speed = I2C_MASTER_FREQ_HZ,
		};

	// Check if driver is already installed
	esp_err_t ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

	if (ret == ESP_FAIL || ret == ESP_ERR_INVALID_STATE) {
		// Driver already installed - delete and reinstall with our pins
		ESP_LOGI(TAG, "I2C driver already installed, reconfiguring for SCL=%d, SDA=%d", scl_pin, sda_pin);
		i2c_driver_delete(I2C_MASTER_NUM);
		vTaskDelay(pdMS_TO_TICKS(50));
	}

	// Configure I2C parameters BEFORE installing driver
	ret = i2c_param_config(I2C_MASTER_NUM, &conf);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
		return ret;
	}

	// Now install the driver with configured parameters
	ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
		return ret;
	}

	ESP_LOGI(TAG, "I2C driver initialized on SCL=%d, SDA=%d", scl_pin, sda_pin);

	// Wait for sensor to be ready
	vTaskDelay(pdMS_TO_TICKS(10));

	// Try to communicate with sensor
	if (!sht45_is_available()) {
		ESP_LOGE(TAG, "SHT45 sensor not detected at address 0x%02X", SHT45_I2C_ADDR);
		ESP_LOGE(TAG, "Check wiring: SCL should be on GPIO %d, SDA on GPIO %d", scl_pin, sda_pin);
		return ESP_ERR_NOT_FOUND;
	}

	// Wait for sensor to fully stabilize before first measurement
	vTaskDelay(pdMS_TO_TICKS(100));
	sht45_initialized = true;
	ESP_LOGI(TAG, "SHT45 sensor initialized successfully");
	return ESP_OK;
}

esp_err_t sht45_read(float *temperature, float *humidity) {
		if (!sht45_initialized) {
				ESP_LOGE(TAG, "SHT45 not initialized");
				return ESP_ERR_INVALID_STATE;
		}

		if (temperature == NULL || humidity == NULL) {
				ESP_LOGE(TAG, "NULL pointer passed to sht45_read");
				return ESP_ERR_INVALID_ARG;
		}

		uint8_t data[6];
		esp_err_t ret;

		// Send measurement command (high precision, no clock stretching)
		i2c_cmd_handle_t cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, (SHT45_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
		i2c_master_write_byte(cmd, SHT45_CMD_MEASURE_HIGH_PRECISION, true);
		i2c_master_stop(cmd);
		ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
		i2c_cmd_link_delete(cmd);

		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Failed to send measurement command: %s", esp_err_to_name(ret));
				return ret;
		}

		// Wait for measurement to complete (high precision: ~8.3ms, add margin for safety)
		vTaskDelay(pdMS_TO_TICKS(20));

		// Read 6 bytes: temp_msb, temp_lsb, temp_crc, hum_msb, hum_lsb, hum_crc
		cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, (SHT45_I2C_ADDR << 1) | I2C_MASTER_READ, true);
		i2c_master_read(cmd, data, 6, I2C_MASTER_LAST_NACK);
		i2c_master_stop(cmd);
		ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
		i2c_cmd_link_delete(cmd);

		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Failed to read measurement data: %s", esp_err_to_name(ret));
				return ret;
		}

		// Verify CRC for temperature
		if (sht45_crc8(data, 2) != data[2]) {
				ESP_LOGE(TAG, "Temperature CRC mismatch");
				return ESP_ERR_INVALID_CRC;
		}

		// Verify CRC for humidity
		if (sht45_crc8(data + 3, 2) != data[5]) {
				ESP_LOGE(TAG, "Humidity CRC mismatch");
				return ESP_ERR_INVALID_CRC;
		}

		// Convert raw values to physical units
		uint16_t temp_raw = (data[0] << 8) | data[1];
		uint16_t hum_raw = (data[3] << 8) | data[4];

		// SHT45 conversion formulas from datasheet
		*temperature = -45.0f + 175.0f * ((float)temp_raw / 65535.0f);
		*humidity = 100.0f * ((float)hum_raw / 65535.0f);

		// Clamp humidity to valid range
		if (*humidity < 0.0f) *humidity = 0.0f;
		if (*humidity > 100.0f) *humidity = 100.0f;

		ESP_LOGD(TAG, "Temperature: %.2f°C, Humidity: %.1f%%", *temperature, *humidity);
		return ESP_OK;
}

bool sht45_is_available(void) {
		i2c_cmd_handle_t cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, (SHT45_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
		i2c_master_stop(cmd);

		esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
		i2c_cmd_link_delete(cmd);

		return (ret == ESP_OK);
}

esp_err_t sht45_deinit(void) {
		if (!sht45_initialized) {
				return ESP_OK;
		}

		esp_err_t ret = i2c_driver_delete(I2C_MASTER_NUM);
		if (ret == ESP_OK) {
				sht45_initialized = false;
				ESP_LOGI(TAG, "SHT45 sensor deinitialized");
		}
		return ret;
}
