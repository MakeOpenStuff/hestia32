#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "core/core_config.h"
#include "core/display_config.h"
#include "protocols/mqtt/mqtt_config.h"
#include "core/protocol_manager.h"
#include "core/display_manager.h"
#include "core/display_ui.h"
#include "core/relay_manager.h"
#include "core/sensor_sht45.h"
#include "core/user_settings.h"
#include "core/core_config.h"
#include "lvgl.h"

static const char *TAG = "main";

#if defined(CONFIG_BOARD_TYPE_DEVKIT) || defined(CONFIG_BOARD_TYPE_XIAO)
#define DISPLAY_ENABLED 1
#else
#define DISPLAY_ENABLED 0
#endif

static void log_active_pin_config(void)
{
#ifdef CONFIG_BOARD_TYPE_XIAO
		ESP_LOGI(TAG, "Board profile: XIAO ESP32-C5");
#else
		ESP_LOGI(TAG, "Board profile: ESP32-C5 DevKit");
#endif

		ESP_LOGI(TAG,
						 "Display pins: SCK=%d MOSI=%d CS=%d DC=%d RST=%d BL=%d TOUCH_CS=%d TOUCH_IRQ=%d TOUCH_MISO=%d",
						 TFT_SCLK, TFT_MOSI, TFT_CS, TFT_DC, TFT_RST, TFT_BL,
						 TOUCH_CS, TOUCH_IRQ, TOUCH_MISO);
		ESP_LOGI(TAG, "Display settings: enabled=%d pclk=%dHz", DISPLAY_ENABLED, LCD_PIXEL_CLOCK_HZ);
}

static void poll_factory_reset_button_runtime(void)
{
#if FACTORY_RESET_GPIO >= 0
		static bool s_button_init = false;
		static int s_hold_count = 0;

		if (!s_button_init) {
				gpio_config_t io_conf = {
						.pin_bit_mask = (1ULL << FACTORY_RESET_GPIO),
						.mode = GPIO_MODE_INPUT,
						.pull_up_en = GPIO_PULLUP_ENABLE,
						.pull_down_en = GPIO_PULLDOWN_DISABLE,
						.intr_type = GPIO_INTR_DISABLE,
				};
				gpio_config(&io_conf);
				s_button_init = true;
				ESP_LOGI(TAG, "Factory reset runtime button enabled on GPIO %d", FACTORY_RESET_GPIO);
		}

		if (gpio_get_level((gpio_num_t)FACTORY_RESET_GPIO) == 0) {
				s_hold_count++;
				if (s_hold_count == 1) {
						ESP_LOGW(TAG, "BOOT pressed: hold for 5 seconds to factory reset");
				}
				if (s_hold_count >= 5) {
						ESP_LOGW(TAG, "========================================");
						ESP_LOGW(TAG, "Factory reset confirmed (runtime button hold)");
						ESP_LOGW(TAG, "Erasing WiFi credentials and touchscreen calibration...");
						ESP_LOGW(TAG, "========================================");

						ESP_ERROR_CHECK(nvs_flash_erase());
						ESP_ERROR_CHECK(nvs_flash_init());

						ESP_LOGW(TAG, "Factory reset complete. Rebooting...");
						vTaskDelay(pdMS_TO_TICKS(1000));
						esp_restart();
				}
		} else if (s_hold_count > 0) {
				ESP_LOGI(TAG, "BOOT released before timeout (%d/5 seconds)", s_hold_count);
				s_hold_count = 0;
		}
#endif
}

// Relay test function - comment out the call in app_main() when not needed
static void test_relay_cycle(void)
{
		ESP_LOGI(TAG, "========================================");
		ESP_LOGI(TAG, "RELAY TEST: Cycling through all relays...");
		ESP_LOGI(TAG, "========================================");

		for (int cycle = 0; cycle < 3; cycle++) {
				ESP_LOGI(TAG, "Test cycle %d/3", cycle + 1);

				for (int relay = 0; relay < RELAY_COUNT; relay++) {
						ESP_LOGI(TAG, "  Relay %d: ON", relay);
						relay_manager_set_relay(relay, true);
						vTaskDelay(pdMS_TO_TICKS(1000));

						ESP_LOGI(TAG, "  Relay %d: OFF", relay);
						relay_manager_set_relay(relay, false);
						vTaskDelay(pdMS_TO_TICKS(1000));
				}

				vTaskDelay(pdMS_TO_TICKS(1000));
		}

		ESP_LOGI(TAG, "========================================");
		ESP_LOGI(TAG, "RELAY TEST: Complete!");
		ESP_LOGI(TAG, "========================================");
}

#if DISPLAY_ENABLED
// Task to handle display during provisioning
static void display_provisioning_task(void* param)
{
		esp_err_t ret = display_init();
		if (ret == ESP_OK) {
				display_create_ui(true, true);
				// Keep provisioning UI responsive while WiFi task continues independently.
				while (1) {
						display_update();
						vTaskDelay(pdMS_TO_TICKS(30));
				}
		}
		vTaskDelete(NULL);
}
#endif

void app_main(void) {
		ESP_LOGI(TAG, "========================================");
		ESP_LOGI(TAG, "Hestia32 ESP32-C5 Application");
		ESP_LOGI(TAG, "Firmware Version: %s", APP_VERSION);
		ESP_LOGI(TAG, "========================================");
		log_active_pin_config();

		// Initialize NVS
		esp_err_t ret = nvs_flash_init();
		if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
				ESP_LOGW(TAG, "NVS partition was truncated, erasing...");
				ESP_ERROR_CHECK(nvs_flash_erase());
				ret = nvs_flash_init();
		}
		ESP_ERROR_CHECK(ret);
		ESP_LOGI(TAG, "NVS initialized");

		ESP_LOGI(TAG, "Factory reset: hold BOOT for 5 seconds while running");

		// PRIORITY 1: Check if calibration exists - run wizard if needed BEFORE WiFi provisioning
#if DISPLAY_ENABLED
		bool has_calibration = display_has_calibration();

		if (!has_calibration) {
				ESP_LOGI(TAG, "No calibration found - running wizard first");
				ret = display_init();
				if (ret == ESP_OK) {
						display_create_ui(false, false);  // Run calibration wizard, full UI
						ESP_LOGI(TAG, "Calibration complete - clearing display");

						// Clear display before restart
						display_clear_screen();
						display_update();
				}
				// Restart to apply calibration and continue with WiFi provisioning
				ESP_LOGI(TAG, "Restarting to continue with WiFi provisioning...");
				vTaskDelay(pdMS_TO_TICKS(1000));
				esp_restart();
		}
#else
		ESP_LOGI(TAG, "Display disabled for this board configuration");
#endif

		// Initialize relay control
		ret = relay_manager_init();
		if (ret != ESP_OK) {
				ESP_LOGW(TAG, "Relay manager initialization failed (continuing anyway)");
		} else {
				ESP_LOGI(TAG, "Relay manager initialized - test cycle disabled for display testing");
				// Test relay control - comment out when not needed
				test_relay_cycle();
		}

		// Initialize protocol manager
		ret = protocol_manager_init();
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Protocol manager initialization failed");
				return;
		}

		bool is_provisioned = protocol_manager_is_provisioned();

		if (!is_provisioned) {
				// Start WiFi AP FIRST
				ESP_LOGI(TAG, "Device not provisioned. Starting provisioning AP...");
				ESP_LOGI(TAG, "Connect to WiFi: %s", PROV_AP_SSID);
				ESP_LOGI(TAG, "Password: %s", PROV_AP_PASSWORD);
				ESP_LOGI(TAG, "Then open http://192.168.4.1 in your browser");

				ret = protocol_manager_start_provisioning();
				if (ret != ESP_OK) {
						ESP_LOGE(TAG, "Failed to start provisioning");
						return;
				}
				ESP_LOGI(TAG, "Provisioning started");

				// Wait for DHCP to initialize
				vTaskDelay(pdMS_TO_TICKS(2000));

				// Initialize display on a low priority task to not interfere with DHCP
		#if DISPLAY_ENABLED
				xTaskCreate(display_provisioning_task, "display_prov", 4096, NULL, 2, NULL);
		#else
				ESP_LOGI(TAG, "Provisioning mode running headless (display disabled)");
		#endif

				// Main task just waits (device will restart after provisioning)
				while (1) {
						vTaskDelay(pdMS_TO_TICKS(100));
				}
		}

		// Device is provisioned and calibrated - initialize display with full UI
#if DISPLAY_ENABLED
		ESP_LOGI(TAG, "Initializing display...");
		ret = display_init();
		if (ret == ESP_OK) {
				display_create_ui(false, false);  // Load calibration from NVS, show full UI
				display_start_lvgl_task();  // Start dedicated LVGL task for UI updates
				ESP_LOGI(TAG, "Display initialized successfully");
		} else {
				ESP_LOGE(TAG, "Display initialization failed");
		}
#else
		ESP_LOGI(TAG, "Running headless mode (display disabled)");
#endif

		// Device is provisioned and calibrated - start protocol
		ESP_LOGI(TAG, "Starting protocol connection...");
		ret = protocol_manager_start();
		if (ret == ESP_OK) {
				ESP_LOGI(TAG, "Protocol started successfully");
		} else {
				ESP_LOGE(TAG, "Failed to start protocol");
		}

		// Initialize SHT45 sensor and user settings for temperature/humidity monitoring
		ESP_LOGI(TAG, "Initializing SHT45 sensor...");
		user_settings_init();

		// Use same I2C pins as TCA9555/relay manager
#ifdef CONFIG_BOARD_TYPE_XIAO
		ret = sht45_init(I2C_MASTER_SCL_IO, I2C_MASTER_SDA_IO);  // XIAO: GPIO 24 (SCL), GPIO 23 (SDA)
#else
		ret = sht45_init(4, 5);  // DevKit: GPIO 4 (SCL), GPIO 5 (SDA)
#endif

		if (ret == ESP_OK && sht45_is_available()) {
				ESP_LOGI(TAG, "SHT45 sensor initialized successfully");
		} else {
				ESP_LOGW(TAG, "SHT45 sensor initialization failed (continuing anyway)");
		}

		// Main task can now suspend - LVGL runs in its own task
		ESP_LOGI(TAG, "Main initialization complete. Main task suspending.");

	// Wait for sensor to stabilize before first read (SHT45 needs ~1s warm-up)
	vTaskDelay(pdMS_TO_TICKS(1500));

	// Optional: Add application logic here or just suspend
	uint32_t last_sensor_read = 0;
	bool first_read_done = false;

	while (1) {
		// Main task suspended, other tasks (LVGL, WiFi, etc.) continue running
		poll_factory_reset_button_runtime();

		// Read sensor immediately on first iteration, then every 5 seconds
		uint32_t now = esp_timer_get_time() / 1000000;  // Convert to seconds
		if (!first_read_done || (now - last_sensor_read >= 5)) {
			float temp, humidity;
			esp_err_t read_ret = sht45_read(&temp, &humidity);
			if (read_ret == ESP_OK) {
				float temp_converted = temp_to_user_unit(temp);
				const char* unit = user_settings_get_temp_unit_symbol();
				bool is_celsius = (user_settings_get_temp_unit() == TEMP_UNIT_CELSIUS);

				// Update display
				#if DISPLAY_ENABLED
				display_ui_update_sensor(temp_converted, humidity, is_celsius);
				#endif

				// Log to serial
				ESP_LOGI(TAG, "┌─────────────────────────────────┐");
				ESP_LOGI(TAG, "│ Temperature: %6.2f %-2s          │", temp_converted, unit);
				ESP_LOGI(TAG, "│ Humidity:   %6.1f %%            │", humidity);
				ESP_LOGI(TAG, "└─────────────────────────────────┘");

				first_read_done = true;
						}
						last_sensor_read = now;
				}

				vTaskDelay(pdMS_TO_TICKS(1000));
		}
}