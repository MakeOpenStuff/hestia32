#include "i2c_scanner.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "i2c_scan";

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_TIMEOUT_MS 100

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

		esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "I2C config failed: %s", esp_err_to_name(ret));
				return;
		}

		ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
				return;
		}

		ESP_LOGI(TAG, "Scanning I2C bus (0x00 - 0x7F)...");
		ESP_LOGI(TAG, "");

		int devices_found = 0;
		for (uint8_t addr = 0x00; addr <= 0x7F; addr++) {
				i2c_cmd_handle_t cmd = i2c_cmd_link_create();
				i2c_master_start(cmd);
				i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
				i2c_master_stop(cmd);

				ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
				i2c_cmd_link_delete(cmd);

				if (ret == ESP_OK) {
						ESP_LOGI(TAG, "Device found at address 0x%02X", addr);
						devices_found++;
				}

				vTaskDelay(pdMS_TO_TICKS(10));
		}

		ESP_LOGI(TAG, "");
		ESP_LOGI(TAG, "Scan complete. Found %d device(s).", devices_found);

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
