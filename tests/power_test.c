#include "power_test.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "../core/sensor_sht45.h"

static const char *TAG = "power_test";

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

		// Initialize SHT45 sensor
		ret = sht45_init(SHT45_I2C_SCL, SHT45_I2C_SDA);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Failed to initialize SHT45 sensor");
				return ret;
		}

		if (!sht45_is_available()) {
				ESP_LOGE(TAG, "SHT45 sensor not detected");
				return ESP_FAIL;
		}

		ESP_LOGI(TAG, "SHT45 sensor initialized (SDA: GPIO%d, SCL: GPIO%d)", SHT45_I2C_SDA, SHT45_I2C_SCL);

		return ESP_OK;
}

void power_test_run(void)
{
		ESP_LOGI(TAG, "========================================");
		ESP_LOGI(TAG, "Starting power consumption test");
		ESP_LOGI(TAG, "Relay cycle: 0→1→2→3→4 active, 5s hold at 4");
		ESP_LOGI(TAG, "SHT45 readings every 1 second");
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

				// Read SHT45 every second
				if (now_ms - last_read_ms >= 1000) {
						float temp, humidity;
						esp_err_t ret = sht45_read(&temp, &humidity);

						if (ret == ESP_OK) {
								ESP_LOGI(TAG, "SHT45: Temp=%.1f°C, Humidity=%.1f%%", temp, humidity);
						} else {
								ESP_LOGW(TAG, "SHT45: Failed to read measurements");
						}

						last_read_ms = now_ms;
				}

				vTaskDelay(pdMS_TO_TICKS(100));
		}
}
