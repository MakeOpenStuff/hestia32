#include "power_test.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "power_test";

// I2C handle
static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t bme280_dev = NULL;

// BME280 registers
#define BME280_REG_CHIP_ID      0xD0
#define BME280_REG_RESET        0xE0
#define BME280_REG_CTRL_HUM     0xF2
#define BME280_REG_STATUS       0xF3
#define BME280_REG_CTRL_MEAS    0xF4
#define BME280_REG_CONFIG       0xF5
#define BME280_REG_PRESS_MSB    0xF7
#define BME280_REG_CALIB_00     0x88
#define BME280_REG_CALIB_26     0xE1

// Calibration data
typedef struct {
		uint16_t dig_T1;
		int16_t  dig_T2;
		int16_t  dig_T3;
		uint16_t dig_P1;
		int16_t  dig_P2;
		int16_t  dig_P3;
		int16_t  dig_P4;
		int16_t  dig_P5;
		int16_t  dig_P6;
		int16_t  dig_P7;
		int16_t  dig_P8;
		int16_t  dig_P9;
		uint8_t  dig_H1;
		int16_t  dig_H2;
		uint8_t  dig_H3;
		int16_t  dig_H4;
		int16_t  dig_H5;
		int8_t   dig_H6;
		int32_t  t_fine;
} bme280_calib_t;

static bme280_calib_t calib;

static esp_err_t bme280_write_reg(uint8_t reg, uint8_t value)
{
		uint8_t data[2] = {reg, value};
		return i2c_master_transmit(bme280_dev, data, 2, -1);
}

static esp_err_t bme280_read_reg(uint8_t reg, uint8_t *data, size_t len)
{
		return i2c_master_transmit_receive(bme280_dev, &reg, 1, data, len, -1);
}

static esp_err_t bme280_read_calibration(void)
{
		uint8_t calib_data[26];
		uint8_t calib_data_h[7];

		// Read temperature and pressure calibration
		esp_err_t ret = bme280_read_reg(BME280_REG_CALIB_00, calib_data, 26);
		if (ret != ESP_OK) return ret;

		// Read humidity calibration
		ret = bme280_read_reg(BME280_REG_CALIB_26, calib_data_h, 7);
		if (ret != ESP_OK) return ret;

		// Parse calibration data
		calib.dig_T1 = (calib_data[1] << 8) | calib_data[0];
		calib.dig_T2 = (calib_data[3] << 8) | calib_data[2];
		calib.dig_T3 = (calib_data[5] << 8) | calib_data[4];
		calib.dig_P1 = (calib_data[7] << 8) | calib_data[6];
		calib.dig_P2 = (calib_data[9] << 8) | calib_data[8];
		calib.dig_P3 = (calib_data[11] << 8) | calib_data[10];
		calib.dig_P4 = (calib_data[13] << 8) | calib_data[12];
		calib.dig_P5 = (calib_data[15] << 8) | calib_data[14];
		calib.dig_P6 = (calib_data[17] << 8) | calib_data[16];
		calib.dig_P7 = (calib_data[19] << 8) | calib_data[18];
		calib.dig_P8 = (calib_data[21] << 8) | calib_data[20];
		calib.dig_P9 = (calib_data[23] << 8) | calib_data[22];
		calib.dig_H1 = calib_data[25];
		calib.dig_H2 = (calib_data_h[1] << 8) | calib_data_h[0];
		calib.dig_H3 = calib_data_h[2];
		calib.dig_H4 = (calib_data_h[3] << 4) | (calib_data_h[4] & 0x0F);
		calib.dig_H5 = (calib_data_h[5] << 4) | (calib_data_h[4] >> 4);
		calib.dig_H6 = calib_data_h[6];

		return ESP_OK;
}

static int32_t bme280_compensate_temperature(int32_t adc_T)
{
		int32_t var1, var2, T;
		var1 = ((((adc_T >> 3) - ((int32_t)calib.dig_T1 << 1))) * ((int32_t)calib.dig_T2)) >> 11;
		var2 = (((((adc_T >> 4) - ((int32_t)calib.dig_T1)) * ((adc_T >> 4) - ((int32_t)calib.dig_T1))) >> 12) * ((int32_t)calib.dig_T3)) >> 14;
		calib.t_fine = var1 + var2;
		T = (calib.t_fine * 5 + 128) >> 8;
		return T;
}

static uint32_t bme280_compensate_pressure(int32_t adc_P)
{
		int64_t var1, var2, p;
		var1 = ((int64_t)calib.t_fine) - 128000;
		var2 = var1 * var1 * (int64_t)calib.dig_P6;
		var2 = var2 + ((var1 * (int64_t)calib.dig_P5) << 17);
		var2 = var2 + (((int64_t)calib.dig_P4) << 35);
		var1 = ((var1 * var1 * (int64_t)calib.dig_P3) >> 8) + ((var1 * (int64_t)calib.dig_P2) << 12);
		var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)calib.dig_P1) >> 33;

		if (var1 == 0) return 0;

		p = 1048576 - adc_P;
		p = (((p << 31) - var2) * 3125) / var1;
		var1 = (((int64_t)calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
		var2 = (((int64_t)calib.dig_P8) * p) >> 19;
		p = ((p + var1 + var2) >> 8) + (((int64_t)calib.dig_P7) << 4);

		return (uint32_t)p;
}

static uint32_t bme280_compensate_humidity(int32_t adc_H)
{
		int32_t v_x1_u32r;
		v_x1_u32r = (calib.t_fine - ((int32_t)76800));
		v_x1_u32r = (((((adc_H << 14) - (((int32_t)calib.dig_H4) << 20) - (((int32_t)calib.dig_H5) * v_x1_u32r)) +
									 ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)calib.dig_H6)) >> 10) *
									 (((v_x1_u32r * ((int32_t)calib.dig_H3)) >> 11) + ((int32_t)32768))) >> 10) +
									 ((int32_t)2097152)) * ((int32_t)calib.dig_H2) + 8192) >> 14));
		v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)calib.dig_H1)) >> 4));
		v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
		v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
		return (uint32_t)(v_x1_u32r >> 12);
}

static esp_err_t bme280_read_measurements(float *temp_c, float *pressure_hpa, float *humidity_pct)
{
		uint8_t data[8];
		esp_err_t ret = bme280_read_reg(BME280_REG_PRESS_MSB, data, 8);
		if (ret != ESP_OK) return ret;

		int32_t adc_P = ((uint32_t)data[0] << 12) | ((uint32_t)data[1] << 4) | ((uint32_t)data[2] >> 4);
		int32_t adc_T = ((uint32_t)data[3] << 12) | ((uint32_t)data[4] << 4) | ((uint32_t)data[5] >> 4);
		int32_t adc_H = ((uint32_t)data[6] << 8) | (uint32_t)data[7];

		int32_t temp = bme280_compensate_temperature(adc_T);
		uint32_t press = bme280_compensate_pressure(adc_P);
		uint32_t hum = bme280_compensate_humidity(adc_H);

		*temp_c = temp / 100.0f;
		*pressure_hpa = press / 25600.0f;
		*humidity_pct = hum / 1024.0f;

		return ESP_OK;
}

static void set_relay(int relay_num, bool state)
{
		gpio_num_t gpio;
		switch (relay_num) {
				case 1: gpio = RELAY1_GPIO; break;
				case 2: gpio = RELAY2_GPIO; break;
				case 3: gpio = RELAY3_GPIO; break;
				case 4: gpio = RELAY4_GPIO; break;
				default: return;
		}
		gpio_set_level(gpio, state ? 1 : 0);
}

esp_err_t power_test_init(void)
{
		ESP_LOGI(TAG, "Initializing power consumption test");

		// Initialize relay GPIOs
		gpio_config_t io_conf = {
				.pin_bit_mask = (1ULL << RELAY1_GPIO) | (1ULL << RELAY2_GPIO) |
												(1ULL << RELAY3_GPIO) | (1ULL << RELAY4_GPIO),
				.mode = GPIO_MODE_OUTPUT,
				.pull_up_en = GPIO_PULLUP_DISABLE,
				.pull_down_en = GPIO_PULLDOWN_DISABLE,
				.intr_type = GPIO_INTR_DISABLE,
		};
		esp_err_t ret = gpio_config(&io_conf);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Failed to configure relay GPIOs");
				return ret;
		}

		// Set all relays OFF initially
		for (int i = 1; i <= 4; i++) {
				set_relay(i, false);
		}

		ESP_LOGI(TAG, "Relays initialized on GPIOs: %d, %d, %d, %d",
						 RELAY1_GPIO, RELAY2_GPIO, RELAY3_GPIO, RELAY4_GPIO);

		// Initialize I2C bus
		i2c_master_bus_config_t bus_config = {
				.i2c_port = I2C_NUM_0,
				.sda_io_num = BME280_I2C_SDA,
				.scl_io_num = BME280_I2C_SCL,
				.clk_source = I2C_CLK_SRC_DEFAULT,
				.glitch_ignore_cnt = 7,
				.flags.enable_internal_pullup = true,
		};

		ret = i2c_new_master_bus(&bus_config, &i2c_bus);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
				return ret;
		}

		// Add BME280 device
		i2c_device_config_t dev_config = {
				.dev_addr_length = I2C_ADDR_BIT_LEN_7,
				.device_address = BME280_I2C_ADDR,
				.scl_speed_hz = 100000,
		};

		ret = i2c_master_bus_add_device(i2c_bus, &dev_config, &bme280_dev);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Failed to add BME280 device: %s", esp_err_to_name(ret));
				return ret;
		}

		ESP_LOGI(TAG, "I2C initialized (SDA: GPIO%d, SCL: GPIO%d)", BME280_I2C_SDA, BME280_I2C_SCL);

		// Check BME280 chip ID
		uint8_t chip_id;
		ret = bme280_read_reg(BME280_REG_CHIP_ID, &chip_id, 1);
		if (ret != ESP_OK || chip_id != 0x60) {
				ESP_LOGE(TAG, "BME280 not found (chip_id: 0x%02X, expected: 0x60)", chip_id);
				return ESP_FAIL;
		}

		ESP_LOGI(TAG, "BME280 detected (chip_id: 0x%02X)", chip_id);

		// Reset BME280
		bme280_write_reg(BME280_REG_RESET, 0xB6);
		vTaskDelay(pdMS_TO_TICKS(10));

		// Read calibration data
		ret = bme280_read_calibration();
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Failed to read BME280 calibration");
				return ret;
		}

		// Configure BME280
		// Humidity oversampling x1
		bme280_write_reg(BME280_REG_CTRL_HUM, 0x01);

		// Temperature oversampling x1, pressure oversampling x1, normal mode
		bme280_write_reg(BME280_REG_CTRL_MEAS, 0x27);

		// Standby 1000ms, filter off
		bme280_write_reg(BME280_REG_CONFIG, 0xA0);

		ESP_LOGI(TAG, "BME280 configured and ready");

		return ESP_OK;
}

void power_test_run(void)
{
		ESP_LOGI(TAG, "========================================");
		ESP_LOGI(TAG, "Starting power consumption test");
		ESP_LOGI(TAG, "Relay cycle: 0→1→2→3→4 active, 5s hold at 4");
		ESP_LOGI(TAG, "BME280 readings every 1 second");
		ESP_LOGI(TAG, "========================================");

		int relay_state = 0;  // 0 = all off, 1-4 = that many relays on
		uint32_t last_change_ms = 0;
		uint32_t last_read_ms = 0;

		while (1) {
				uint32_t now_ms = esp_timer_get_time() / 1000;

				// Handle relay state changes
				if (now_ms - last_change_ms >= 1000) {
						if (relay_state == 4 && now_ms - last_change_ms >= 5000) {
								// After holding all 4 relays for 5 seconds, restart cycle
								relay_state = 0;
								for (int i = 1; i <= 4; i++) {
										set_relay(i, false);
								}
								last_change_ms = now_ms;
								ESP_LOGI(TAG, "RELAYS: All OFF (cycle restart)");
						} else if (relay_state < 4 || (relay_state == 4 && now_ms - last_change_ms < 5000)) {
								// Turn off all relays first
								for (int i = 1; i <= 4; i++) {
										set_relay(i, false);
								}

								// Turn on the required number of relays
								for (int i = 1; i <= relay_state; i++) {
										set_relay(i, true);
								}

								if (relay_state == 0) {
										ESP_LOGI(TAG, "RELAYS: 0 active (all OFF)");
								} else if (relay_state == 1) {
										ESP_LOGI(TAG, "RELAYS: 1 active (GPIO %d)", RELAY1_GPIO);
								} else if (relay_state == 2) {
										ESP_LOGI(TAG, "RELAYS: 2 active (GPIO %d, %d)", RELAY1_GPIO, RELAY2_GPIO);
								} else if (relay_state == 3) {
										ESP_LOGI(TAG, "RELAYS: 3 active (GPIO %d, %d, %d)", RELAY1_GPIO, RELAY2_GPIO, RELAY3_GPIO);
								} else if (relay_state == 4) {
										ESP_LOGI(TAG, "RELAYS: 4 active (GPIO %d, %d, %d, %d) - HOLDING 5s", RELAY1_GPIO, RELAY2_GPIO, RELAY3_GPIO, RELAY4_GPIO);
								}

								if (relay_state < 4) {
										relay_state++;
										last_change_ms = now_ms;
								}
						}
				}

				// Read BME280 every second
				if (now_ms - last_read_ms >= 1000) {
						float temp, pressure, humidity;
						esp_err_t ret = bme280_read_measurements(&temp, &pressure, &humidity);

						if (ret == ESP_OK) {
								ESP_LOGI(TAG, "BME280: Temp=%.1f°C, Pressure=%.1f hPa, Humidity=%.1f%%",
												 temp, pressure, humidity);
						} else {
								ESP_LOGW(TAG, "BME280: Failed to read measurements");
						}

						last_read_ms = now_ms;
				}

				vTaskDelay(pdMS_TO_TICKS(100));
		}
}
