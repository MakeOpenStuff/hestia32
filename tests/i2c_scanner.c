#include "i2c_scanner.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdint.h>

static const char *TAG = "i2c_scan";

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_TIMEOUT_MS 100

#define SHT45_ADDR 0x44
#define TCA9555_ADDR 0x20

static uint8_t crc8_sht45(const uint8_t *data, int len) {
		const uint8_t polynomial = 0x31;
		uint8_t crc = 0xFF;

		for (int j = len; j; --j) {
				crc ^= *data++;
				for (int i = 8; i; --i) {
						crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ polynomial) : (uint8_t)(crc << 1);
				}
		}
		return crc;
}

static const char *known_device_name(uint8_t addr) {
		switch (addr) {
				case SHT45_ADDR: return "SHT45";
				case TCA9555_ADDR: return "TCA9555";
				default: return "unknown";
		}
}

static void probe_sht45_details(void) {
		uint8_t cmd = 0x89;  // Read serial number
		uint8_t rx[6] = {0};
		esp_err_t ret = i2c_master_write_read_device(I2C_MASTER_NUM, SHT45_ADDR, &cmd, 1, rx, sizeof(rx), pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));

		if (ret != ESP_OK) {
				ESP_LOGW(TAG, "SHT45 detail probe failed: %s", esp_err_to_name(ret));
				return;
		}

		bool crc1_ok = (crc8_sht45(rx, 2) == rx[2]);
		bool crc2_ok = (crc8_sht45(rx + 3, 2) == rx[5]);
		uint32_t serial = ((uint32_t)rx[0] << 24) | ((uint32_t)rx[1] << 16) | ((uint32_t)rx[3] << 8) | rx[4];

		ESP_LOGI(TAG, "SHT45 serial raw: %02X %02X %02X %02X %02X %02X", rx[0], rx[1], rx[2], rx[3], rx[4], rx[5]);
		ESP_LOGI(TAG, "SHT45 serial: 0x%08lX (CRC1=%s, CRC2=%s)", (unsigned long)serial, crc1_ok ? "OK" : "FAIL", crc2_ok ? "OK" : "FAIL");
}

static void probe_sht45_measurement(void) {
		uint8_t measure_cmd = 0xFD;  // High precision measurement
		uint8_t rx[6] = {0};

		esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_NUM, SHT45_ADDR, &measure_cmd, 1, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
		if (ret != ESP_OK) {
				ESP_LOGW(TAG, "SHT45 measure command failed: %s", esp_err_to_name(ret));
				return;
		}

		vTaskDelay(pdMS_TO_TICKS(20));

		ret = i2c_master_read_from_device(I2C_MASTER_NUM, SHT45_ADDR, rx, sizeof(rx), pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
		if (ret != ESP_OK) {
				ESP_LOGW(TAG, "SHT45 measurement read failed: %s", esp_err_to_name(ret));
				return;
		}

		bool temp_crc_ok = (crc8_sht45(rx, 2) == rx[2]);
		bool hum_crc_ok = (crc8_sht45(rx + 3, 2) == rx[5]);

		uint16_t temp_raw = ((uint16_t)rx[0] << 8) | rx[1];
		uint16_t hum_raw = ((uint16_t)rx[3] << 8) | rx[4];

		float temp_c = -45.0f + 175.0f * ((float)temp_raw / 65535.0f);
		float humidity = 100.0f * ((float)hum_raw / 65535.0f);

		ESP_LOGI(TAG, "SHT45 measurement raw: %02X %02X %02X %02X %02X %02X", rx[0], rx[1], rx[2], rx[3], rx[4], rx[5]);
		ESP_LOGI(TAG, "SHT45 decoded: temp=%.2fC hum=%.1f%% (temp_crc=%s hum_crc=%s)",
				 temp_c, humidity, temp_crc_ok ? "OK" : "FAIL", hum_crc_ok ? "OK" : "FAIL");
}

static void probe_tca9555_details(void) {
		uint8_t reg = 0x00;  // Input port 0
		uint8_t value = 0;
		esp_err_t ret = i2c_master_write_read_device(I2C_MASTER_NUM, TCA9555_ADDR, &reg, 1, &value, 1, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
		if (ret == ESP_OK) {
				ESP_LOGI(TAG, "TCA9555 input port0: 0x%02X", value);
		} else {
				ESP_LOGW(TAG, "TCA9555 detail probe failed: %s", esp_err_to_name(ret));
		}
}

void i2c_scanner_scan(int scl_pin, int sda_pin) {
		ESP_LOGI(TAG, "========================================");
		ESP_LOGI(TAG, "I2C Scanner");
		ESP_LOGI(TAG, "SCL: GPIO %d, SDA: GPIO %d", scl_pin, sda_pin);
		ESP_LOGI(TAG, "========================================");

		i2c_config_t conf = {
				.mode = I2C_MODE_MASTER,
				.sda_io_num = sda_pin,
				.scl_io_num = scl_pin,
				.sda_pullup_en = GPIO_PULLUP_ENABLE,
				.scl_pullup_en = GPIO_PULLUP_ENABLE,
				.master.clk_speed = 100000,
		};

		esp_err_t ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
		if (ret == ESP_ERR_INVALID_STATE || ret == ESP_FAIL) {
				ESP_LOGW(TAG, "I2C driver already installed, reconfiguring...");
			i2c_driver_delete(I2C_MASTER_NUM);
				vTaskDelay(pdMS_TO_TICKS(20));
		}

		ret = i2c_param_config(I2C_MASTER_NUM, &conf);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "I2C config failed: %s", esp_err_to_name(ret));
				return;
		}

		ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
				return;
		}

		ESP_LOGI(TAG, "Scanning I2C bus (0x03 - 0x77)...");
		ESP_LOGI(TAG, "");

		int devices_found = 0;
		int timeout_count = 0;
		for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
				i2c_cmd_handle_t cmd = i2c_cmd_link_create();
				i2c_master_start(cmd);
				i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
				i2c_master_stop(cmd);

				ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
				i2c_cmd_link_delete(cmd);

				if (ret == ESP_OK) {
						ESP_LOGI(TAG, "Device found at 0x%02X (%s)", addr, known_device_name(addr));
						devices_found++;
				} else if (ret == ESP_ERR_TIMEOUT) {
						timeout_count++;
				}

				vTaskDelay(pdMS_TO_TICKS(2));
		}

		ESP_LOGI(TAG, "");
		ESP_LOGI(TAG, "Scan complete. Found %d device(s).", devices_found);
		if (timeout_count > 0) {
				ESP_LOGW(TAG, "I2C timeouts during scan: %d", timeout_count);
		}

		// Extra probes for known devices to validate real communication beyond ACK.
		probe_tca9555_details();
		probe_sht45_details();
		probe_sht45_measurement();

		if (devices_found == 0) {
				ESP_LOGW(TAG, "No I2C devices found!");
				ESP_LOGW(TAG, "Possible issues:");
				ESP_LOGW(TAG, "  - Check wiring (SDA, SCL, VCC, GND)");
				ESP_LOGW(TAG, "  - Verify 3.3V power supply");
				ESP_LOGW(TAG, "  - Add 4.7kΩ pull-up resistors on SDA and SCL");
				ESP_LOGW(TAG, "  - Check for GPIO conflicts with other hardware");
		}
		ESP_LOGI(TAG, "========================================");

		i2c_driver_delete(I2C_MASTER_NUM);
}
