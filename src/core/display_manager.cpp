#include "core/display_config.h"
#include "core/display_manager.h"
#include "core/display_ui.h"
#include "core/core_config.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9488.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Static reusable line buffer (RGB666) to avoid large allocations
static uint8_t *s_line_rgb666 = NULL;
static size_t s_line_rgb666_size = 0;

static const char *TAG = "display";
//TODO: Check timeouts for calibration and blocking WiFi provisioning screens

// NVS namespace for calibration
#define NVS_CALIBRATION_NAMESPACE "touch_cal"
#define NVS_KEY_X_MIN "x_min"
#define NVS_KEY_X_MAX "x_max"
#define NVS_KEY_Y_MIN "y_min"
#define NVS_KEY_Y_MAX "y_max"
#define NVS_KEY_SWAP_XY "swap_xy"
#define NVS_KEY_FLIP_X "flip_x"
#define NVS_KEY_FLIP_Y "flip_y"

// Display handles
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
// LVGL buffers (disabled in minimal bring-up)
static lv_disp_drv_t disp_drv;
static lv_disp_draw_buf_t disp_buf;
static lv_color_t *buf1 = NULL;
static lv_color_t *buf2 = NULL;

// Limit LVGL handler cadence to reduce tearing
static uint32_t s_last_handler_ms = 0;
static volatile uint32_t s_flush_count = 0;
// Touch
static spi_device_handle_t s_touch_dev = NULL;
static lv_obj_t *touch_dot = NULL;
static TaskHandle_t s_lvgl_task_handle = NULL;
static lv_indev_t *s_touch_indev = NULL;
static bool sSwapXY = false;
static bool sFlipX = false;
static bool sFlipY = false;
static bool sCalibrated = false;
static const uint16_t TOUCH_ACTIVE_THRESH = 300; // increased from 50 to reduce noise

// RGB LED for OTA visual feedback
static rmt_channel_handle_t s_led_chan = NULL;
static rmt_encoder_handle_t s_led_encoder = NULL;
#define RMT_LED_RESOLUTION_HZ 10000000  // 10MHz

// Track hardware initialization state
static bool s_spi_initialized = false;

// Touch calibration (raw min/max)
static uint16_t s_cal_x_min = 200, s_cal_x_max = 3900;
static uint16_t s_cal_y_min = 200, s_cal_y_max = 3900;

static bool xpt2046_read_raw(uint16_t *x, uint16_t *y)
{
		if (!s_touch_dev) {
				return false;
		}
		auto xpt_read = [](uint8_t cmd) -> uint16_t {
				// Full-duplex: send 3 bytes (cmd + 2 dummy) to clock out 3 bytes response
				uint8_t tx[3] = {cmd, 0x00, 0x00};
				uint8_t rx[3] = {0};
				spi_transaction_t t = {};
				t.length = 8 * sizeof(tx); // 24 bits TX and RX
				t.tx_buffer = tx;
				t.rx_buffer = rx;
				esp_err_t ret = spi_device_transmit(s_touch_dev, &t);
				if (ret != ESP_OK) {
						ESP_LOGW(TAG, "xpt2046 transmit err: %d", ret);
						return 0;
				}

				// XPT2046 returns data in bytes 1 and 2 (first byte is garbage)
				uint16_t val = ((uint16_t)rx[1] << 8) | rx[2];
				val >>= 4; // 12-bit value
				return val;
		};

		// Average a few samples for stability
		const int samples = 4;
		uint32_t sx = 0, sy = 0;
		for (int i = 0; i < samples; i++) {
				sx += xpt_read(0xD0); // X position
				vTaskDelay(pdMS_TO_TICKS(5)); // Allow conversion
				sy += xpt_read(0x90); // Y position
				vTaskDelay(pdMS_TO_TICKS(5)); // Allow conversion
		}
		*x = (uint16_t)(sx / samples);
		*y = (uint16_t)(sy / samples);
		return true;
}

// Backlight control
static void backlight_init(void)
{
		ledc_timer_config_t ledc_timer = {
				.speed_mode       = LEDC_LOW_SPEED_MODE,
				.duty_resolution  = LEDC_TIMER_8_BIT,
				.timer_num        = LEDC_TIMER_0,
				.freq_hz          = 5000,
				.clk_cfg          = LEDC_AUTO_CLK,
				.deconfigure      = false
		};
		ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

		ledc_channel_config_t ledc_channel = {
				.gpio_num       = TFT_BL,
				.speed_mode     = LEDC_LOW_SPEED_MODE,
				.channel        = LEDC_CHANNEL_0,
				.intr_type      = LEDC_INTR_DISABLE,
				.timer_sel      = LEDC_TIMER_0,
				.duty           = 0,
				.hpoint         = 0,
				.sleep_mode     = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
				.flags          = {}
		};
		ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
		ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 255));
		ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
}

// LVGL flush callback with RGB565 to RGB666 conversion for ILI9488
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
		esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t) drv->user_data;
		const int offsetx1 = area->x1;
		const int offsetx2 = area->x2;
		const int offsety1 = area->y1;
		const int offsety2 = area->y2;

		const int width = (offsetx2 - offsetx1 + 1);
		const int height = (offsety2 - offsety1 + 1);

		// Buffer already allocated at init
		const uint16_t *src = (const uint16_t *)color_map;
		for (int y = 0; y < height; y++) {
				const uint16_t *row = src + (size_t)y * (size_t)width;
				for (int x = 0; x < width; x++) {
						const uint16_t pixel = row[x];
						// Inline RGB565->RGB666 conversion
						const size_t idx = (size_t)x * 3;
						s_line_rgb666[idx + 0] = ((pixel >> 11) & 0x1F) << 3; // R
						s_line_rgb666[idx + 1] = ((pixel >> 5) & 0x3F) << 2;  // G
						s_line_rgb666[idx + 2] = (pixel & 0x1F) << 3;         // B
				}
				esp_lcd_panel_draw_bitmap(panel, offsetx1, offsety1 + y, offsetx2 + 1, offsety1 + y + 1, s_line_rgb666);
		}
		s_flush_count = s_flush_count + 1;
		lv_disp_flush_ready(drv);
}

esp_err_t display_init(void)
{
		ESP_LOGI(TAG, "Initializing display");

		// Try to load calibration from NVS
		nvs_handle_t nvs_handle;
		esp_err_t err = nvs_open(NVS_CALIBRATION_NAMESPACE, NVS_READONLY, &nvs_handle);
		if (err == ESP_OK) {
				uint16_t x_min, x_max, y_min, y_max;
				uint8_t swap_xy, flip_x, flip_y;

				if (nvs_get_u16(nvs_handle, NVS_KEY_X_MIN, &x_min) == ESP_OK &&
						nvs_get_u16(nvs_handle, NVS_KEY_X_MAX, &x_max) == ESP_OK &&
						nvs_get_u16(nvs_handle, NVS_KEY_Y_MIN, &y_min) == ESP_OK &&
						nvs_get_u16(nvs_handle, NVS_KEY_Y_MAX, &y_max) == ESP_OK &&
						nvs_get_u8(nvs_handle, NVS_KEY_SWAP_XY, &swap_xy) == ESP_OK &&
						nvs_get_u8(nvs_handle, NVS_KEY_FLIP_X, &flip_x) == ESP_OK &&
						nvs_get_u8(nvs_handle, NVS_KEY_FLIP_Y, &flip_y) == ESP_OK) {

						s_cal_x_min = x_min;
						s_cal_x_max = x_max;
						s_cal_y_min = y_min;
						s_cal_y_max = y_max;
						sSwapXY = swap_xy != 0;
						sFlipX = flip_x != 0;
						sFlipY = flip_y != 0;
						sCalibrated = true;

						ESP_LOGI(TAG, "Loaded calibration from NVS: X[%u..%u] Y[%u..%u] swap=%d flip_x=%d flip_y=%d",
										 x_min, x_max, y_min, y_max, (int)sSwapXY, (int)sFlipX, (int)sFlipY);
				}
				nvs_close(nvs_handle);
		}

		// Initialize backlight
		backlight_init();

		// Allocate RGB666 line buffer FIRST
		s_line_rgb666_size = TFT_WIDTH * 3;
		s_line_rgb666 = (uint8_t *)heap_caps_malloc(s_line_rgb666_size, MALLOC_CAP_DMA);
		if (!s_line_rgb666) {
				ESP_LOGE(TAG, "Failed to allocate RGB666 buffer");
				return ESP_ERR_NO_MEM;
		}

		// Configure SPI bus (skip if already initialized for reinit scenario)
		if (!s_spi_initialized) {
				spi_bus_config_t buscfg = {};
				buscfg.mosi_io_num = TFT_MOSI;
				buscfg.miso_io_num = TOUCH_MISO; // Dedicated to touch only
				buscfg.sclk_io_num = TFT_SCLK;
				buscfg.quadwp_io_num = -1;
				buscfg.quadhd_io_num = -1;
				buscfg.max_transfer_sz = TFT_WIDTH * LVGL_BUFFER_HEIGHT * 3;
				buscfg.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_SCLK | SPICOMMON_BUSFLAG_MOSI | SPICOMMON_BUSFLAG_MISO;
				ESP_ERROR_CHECK(spi_bus_initialize(TFT_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
				s_spi_initialized = true;
				ESP_LOGI(TAG, "SPI bus initialized");
		} else {
				ESP_LOGI(TAG, "SPI bus already initialized, skipping");
		}

		// Configure panel I/O
		esp_lcd_panel_io_spi_config_t io_config = {};
		io_config.cs_gpio_num = TFT_CS;
		io_config.dc_gpio_num = TFT_DC;
		io_config.spi_mode = 0;
		io_config.pclk_hz = LCD_PIXEL_CLOCK_HZ;
		io_config.trans_queue_depth = 3;  // Lower depth to reduce interleaving and tearing
		io_config.lcd_cmd_bits = LCD_CMD_BITS;
		io_config.lcd_param_bits = LCD_PARAM_BITS;
		ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)TFT_SPI_HOST, &io_config, &io_handle));

		// Add touch device (XPT2046) on same SPI bus
		{
				spi_device_interface_config_t devcfg = {};
				devcfg.clock_speed_hz = 2 * 1000 * 1000; // 2 MHz for touch
				devcfg.mode = 0; // Try mode 0 first (CPOL=0, CPHA=0)
				devcfg.spics_io_num = TOUCH_CS;
				devcfg.queue_size = 3;
				devcfg.flags = 0; // full duplex
				devcfg.input_delay_ns = 0; // Remove delay for now
				ESP_ERROR_CHECK(spi_bus_add_device(TFT_SPI_HOST, &devcfg, &s_touch_dev));

				// Configure IRQ pin (active low)
				gpio_config_t io_conf = {};
				io_conf.pin_bit_mask = 1ULL << TOUCH_IRQ;
				io_conf.mode = GPIO_MODE_INPUT;
				io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
				io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
				io_conf.intr_type = GPIO_INTR_DISABLE;
				ESP_ERROR_CHECK(gpio_config(&io_conf));
		}

		// Configure ILI9488 panel
		esp_lcd_panel_dev_config_t panel_config = {};
		panel_config.reset_gpio_num = TFT_RST;
		panel_config.rgb_endian = LCD_RGB_ENDIAN_RGB;
		panel_config.bits_per_pixel = 24; // ILI9488 SPI expects 18-bit (24bpp container)
		ESP_ERROR_CHECK(esp_lcd_new_panel_ili9488(io_handle, &panel_config, &panel_handle));

		// Reset and initialize panel
		ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
		ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

		// Ensure pixel format 18-bit (RGB666) for ILI9488 SPI
		{
				uint8_t pf = 0x66; // 18-bit/pixel RGB666
				ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_handle, 0x3A, &pf, 1));
		}

		// Orientation tweaks if needed (disable inversion for now)
		ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
		ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, false));
		ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));
		ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

		lv_init();

		size_t buffer_size = TFT_WIDTH * LVGL_BUFFER_HEIGHT;
		buf1 = (lv_color_t *)heap_caps_malloc(buffer_size * sizeof(lv_color_t), MALLOC_CAP_DMA);
		buf2 = (lv_color_t *)heap_caps_malloc(buffer_size * sizeof(lv_color_t), MALLOC_CAP_DMA);
		if (!buf1) {
				ESP_LOGE(TAG, "Failed to allocate LVGL buffer 1");
				return ESP_ERR_NO_MEM;
		}
		if (!buf2) {
				ESP_LOGW(TAG, "Failed to allocate LVGL buffer 2, using single buffer mode");
				buf2 = NULL;  // Single buffer mode
		}

		lv_disp_draw_buf_init(&disp_buf, buf1, buf2, buffer_size);

		lv_disp_drv_init(&disp_drv);
		disp_drv.hor_res = TFT_WIDTH;
		disp_drv.ver_res = TFT_HEIGHT;
		disp_drv.flush_cb = lvgl_flush_cb;
		disp_drv.draw_buf = &disp_buf;
		disp_drv.user_data = panel_handle;
		lv_disp_drv_register(&disp_drv);

		// Register LVGL touch input device
		{
				static lv_indev_drv_t indev_drv;
				lv_indev_drv_init(&indev_drv);
				indev_drv.type = LV_INDEV_TYPE_POINTER;
				indev_drv.read_cb = [](lv_indev_drv_t *indev, lv_indev_data_t *data){
						// Read averaged raw values via helper
						uint16_t raw_x = 0, raw_y = 0;
						if (!xpt2046_read_raw(&raw_x, &raw_y)) {
								data->state = LV_INDEV_STATE_REL;
								return;
						}

						bool irq_low = (gpio_get_level((gpio_num_t)TOUCH_IRQ) == 0);
						// XPT2046 returns extreme values when not touched (0 or ~4095)
						// Valid touch: both axes in reasonable range
						bool valid_x = (raw_x > 100 && raw_x < 3900);
						bool valid_y = (raw_y > 100 && raw_y < 3900);
						bool active_by_thresh = valid_x && valid_y;
						if (!irq_low && !active_by_thresh) {
								data->state = LV_INDEV_STATE_REL;
								return;
						}

						// Preserve original values for logging
						const uint16_t raw_x0 = raw_x;
						const uint16_t raw_y0 = raw_y;

						// Apply calibration min/max (clamp) after sampling
						if (raw_x < s_cal_x_min) raw_x = s_cal_x_min;
						if (raw_x > s_cal_x_max) raw_x = s_cal_x_max;
						if (raw_y < s_cal_y_min) raw_y = s_cal_y_min;
						if (raw_y > s_cal_y_max) raw_y = s_cal_y_max;

						// Map to screen coordinates
						int32_t x = (int32_t)(raw_x - s_cal_x_min) * (int32_t)(TFT_WIDTH - 1) / (int32_t)(s_cal_x_max - s_cal_x_min);
						int32_t y = (int32_t)(raw_y - s_cal_y_min) * (int32_t)(TFT_HEIGHT - 1) / (int32_t)(s_cal_y_max - s_cal_y_min);

						// Orientation adjustments
						if (sSwapXY) { int32_t tmp = x; x = y; y = tmp; }
						if (sFlipX) { x = (TFT_WIDTH - 1) - x; }
						if (sFlipY) { y = (TFT_HEIGHT - 1) - y; }

						// Clamp to screen bounds
						if (x < 0) {
								x = 0;
						} else if (x > (TFT_WIDTH - 1)) {
								x = (TFT_WIDTH - 1);
						}
						if (y < 0) {
								y = 0;
						} else if (y > (TFT_HEIGHT - 1)) {
								y = (TFT_HEIGHT - 1);
						}

						data->point.x = (lv_coord_t)x;
						data->point.y = (lv_coord_t)y;
						data->state = LV_INDEV_STATE_PR;

						// Debug: print mapping on touch (reduced frequency)
						static uint32_t s_last_touch_log = 0;
						uint32_t now_ms = lv_tick_get();
						if (now_ms - s_last_touch_log > 1000) {
								ESP_LOGI(TAG, "touch raw:(%u,%u) clamped:(%u,%u) mapped:(%d,%d)",
												 (unsigned)raw_x0, (unsigned)raw_y0,
												 (unsigned)raw_x, (unsigned)raw_y,
												 (int)x, (int)y);
								s_last_touch_log = now_ms;
						}
				};
				s_touch_indev = lv_indev_drv_register(&indev_drv);
		}

		ESP_LOGI(TAG, "Display initialized successfully (LVGL enabled)");

		return ESP_OK;
}

bool display_has_calibration(void)
{
		nvs_handle_t nvs_handle;
		esp_err_t err = nvs_open(NVS_CALIBRATION_NAMESPACE, NVS_READONLY, &nvs_handle);
		if (err != ESP_OK) {
				return false;
		}

		uint16_t x_min;
		bool has_cal = (nvs_get_u16(nvs_handle, NVS_KEY_X_MIN, &x_min) == ESP_OK);
		nvs_close(nvs_handle);
		return has_cal;
}

void display_clear_screen(void)
{
		lv_obj_clean(lv_scr_act());
		lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);
}

		static void run_touch_calibration(lv_obj_t *scr)
		{
				ESP_LOGI(TAG, "Starting touch calibration (60 second timeout)");

				// Clear screen and set black background for wizard
				lv_obj_clean(scr);
				lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

				lv_obj_t *msg = lv_label_create(scr);
				lv_label_set_text(msg, "Touch target 1/5 (60s remaining)");
				lv_obj_set_style_text_color(msg, lv_color_hex(0xFFFFFF), 0);
				lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 8);

				// Target marker objects (crosshair)
				lv_obj_t *marker_h = lv_obj_create(scr);
				lv_obj_t *marker_v = lv_obj_create(scr);
				lv_obj_set_style_bg_color(marker_h, lv_color_hex(0xFFFFFF), 0);
				lv_obj_set_style_bg_color(marker_v, lv_color_hex(0xFFFFFF), 0);
				lv_obj_set_size(marker_h, 20, 2);
				lv_obj_set_size(marker_v, 2, 20);

				struct Target { int x; int y; };
				Target targets[5] = {
						{10, 10},                          // Top-left
						{TFT_WIDTH - 10, 10},              // Top-right
						{TFT_WIDTH - 10, TFT_HEIGHT - 10}, // Bottom-right
						{10, TFT_HEIGHT - 10},             // Bottom-left
						{TFT_WIDTH / 2, TFT_HEIGHT / 2}    // Center
				};
				uint16_t xs[5] = {0}, ys[5] = {0};

				bool aborted = false;
				const uint32_t wait_timeout_ms = 60000; // 60 seconds total timeout
				const uint32_t per_target_timeout_ms = 15000; // 15 seconds per target
				uint32_t calibration_start_ms = lv_tick_get();
				uint32_t last_countdown_update = 0;

				for (int i = 0; i < 5; i++) {
						lv_obj_set_pos(marker_h, targets[i].x - 10, targets[i].y - 1);
						lv_obj_set_pos(marker_v, targets[i].x - 1, targets[i].y - 10);
						// Force a refresh to ensure marker is visible
						lv_timer_handler();

						// Wait until pressed (IRQ low or raw threshold)
								{
										uint32_t ws = lv_tick_get();
										while (true) {
												uint32_t elapsed = lv_tick_get() - calibration_start_ms;
												uint32_t remaining_sec = (wait_timeout_ms - elapsed) / 1000;

												// Update countdown every second
												if (lv_tick_get() - last_countdown_update > 1000) {
														char buf[64];
														snprintf(buf, sizeof(buf), "Touch target %d/5 (%us remaining)", i + 1, (unsigned)remaining_sec);
														lv_label_set_text(msg, buf);
														last_countdown_update = lv_tick_get();
												}

												// Check overall calibration timeout (60 seconds)
												if (elapsed > wait_timeout_ms) {
														ESP_LOGW(TAG, "Calibration aborted: 60 second timeout reached");
														aborted = true;
														break;
												}

												uint16_t rx, ry;
												bool have_raw = xpt2046_read_raw(&rx, &ry);
												bool irq_low = (gpio_get_level((gpio_num_t)TOUCH_IRQ) == 0);
												// Valid touch: both axes in reasonable range
												bool valid_x = (rx > 100 && rx < 3900);
												bool valid_y = (ry > 100 && ry < 3900);
												bool active_by_thresh = have_raw && valid_x && valid_y;
												if (irq_low || active_by_thresh) break;
												lv_timer_handler();
												vTaskDelay(pdMS_TO_TICKS(50));  // Increased delay to give more CPU to WiFi/DHCP tasks

												// Per-target timeout
												if ((lv_tick_get() - ws) > per_target_timeout_ms) {
														ESP_LOGW(TAG, "Calibration aborted: no touch detected on target %d", i);
														aborted = true;
														break;
												}
										}
										if (aborted) break;
								}
						// Average several samples while pressed for stability
						{
								const int samples = 8;
								uint32_t ax = 0, ay = 0; int got = 0;
								for (int s = 0; s < samples; s++) {
										uint16_t rx, ry;
										if (xpt2046_read_raw(&rx, &ry)) { ax += rx; ay += ry; got++; }
										vTaskDelay(pdMS_TO_TICKS(4));
								}
								if (got == 0) { xs[i] = 0; ys[i] = 0; }
								else { xs[i] = (uint16_t)(ax / got); ys[i] = (uint16_t)(ay / got); }
						}
						// Wait until release (IRQ high)
						{
								uint32_t ws = lv_tick_get();
								while (gpio_get_level((gpio_num_t)TOUCH_IRQ) == 0) {
										lv_timer_handler();
										vTaskDelay(pdMS_TO_TICKS(10));
										if ((lv_tick_get() - ws) > per_target_timeout_ms) {
												// If stuck pressed, continue to next target
												ESP_LOGW(TAG, "Calibration: stuck press, continuing");
												break;
										}
								}
						}
				}

				if (aborted) {
						// Clean up calibration UI and use defaults
						lv_obj_del(marker_h);
						lv_obj_del(marker_v);
						lv_obj_del(msg);
						s_cal_x_min = 200; s_cal_x_max = 3900;
						s_cal_y_min = 200; s_cal_y_max = 3900;
						ESP_LOGW(TAG, "Calibration skipped; using defaults X[%u..%u] Y[%u..%u]", s_cal_x_min, s_cal_x_max, s_cal_y_min, s_cal_y_max);
						return;
				}

				// Determine axis mapping and flips using corner samples
				// 0=TL, 1=TR, 2=BR, 3=BL
				int dx_raw_x = (int)xs[1] - (int)xs[0];
				int dx_raw_y = (int)ys[1] - (int)ys[0];
				int dy_raw_x = (int)xs[3] - (int)xs[0];
				int dy_raw_y = (int)ys[3] - (int)ys[0];

				// Decide if screen X corresponds to raw X or raw Y
				sSwapXY = (abs(dx_raw_y) > abs(dx_raw_x));

				if (!sSwapXY) {
						// X uses raw_x, Y uses raw_y
						sFlipX = (dx_raw_x < 0);
						sFlipY = (dy_raw_y < 0);
						uint16_t raw_x_left  = (uint16_t)((xs[0] + xs[3]) / 2);
						uint16_t raw_x_right = (uint16_t)((xs[1] + xs[2]) / 2);
						uint16_t raw_y_top   = (uint16_t)((ys[0] + ys[1]) / 2);
						uint16_t raw_y_bot   = (uint16_t)((ys[3] + ys[2]) / 2);
						s_cal_x_min = sFlipX ? raw_x_right : raw_x_left;
						s_cal_x_max = sFlipX ? raw_x_left  : raw_x_right;
						s_cal_y_min = sFlipY ? raw_y_bot   : raw_y_top;
						s_cal_y_max = sFlipY ? raw_y_top   : raw_y_bot;
				} else {
						// X uses raw_y (horizontal movement), Y uses raw_x (vertical movement)
						sFlipX = (dx_raw_y < 0);
						sFlipY = (dy_raw_x < 0);
						uint16_t raw_y_left  = (uint16_t)((ys[0] + ys[3]) / 2);
						uint16_t raw_y_right = (uint16_t)((ys[1] + ys[2]) / 2);
						uint16_t raw_x_top   = (uint16_t)((xs[0] + xs[1]) / 2);
						uint16_t raw_x_bot   = (uint16_t)((xs[3] + xs[2]) / 2);
						// Because swap occurs after mapping, set ranges to match pre-swap axes
						// Pre-swap y (from raw_y) becomes final x; so use raw_y extremes in s_cal_y_*
						s_cal_y_min = sFlipX ? raw_y_right : raw_y_left;
						s_cal_y_max = sFlipX ? raw_y_left  : raw_y_right;
						// Pre-swap x (from raw_x) becomes final y; so use raw_x extremes in s_cal_x_*
						s_cal_x_min = sFlipY ? raw_x_bot   : raw_x_top;
						s_cal_x_max = sFlipY ? raw_x_top   : raw_x_bot;
				}

				// Validate ranges; fallback to defaults if invalid
				if (s_cal_x_min >= s_cal_x_max || s_cal_y_min >= s_cal_y_max) {
						ESP_LOGW(TAG, "Invalid calibration ranges; using defaults");
						s_cal_x_min = 200; s_cal_x_max = 3900;
						s_cal_y_min = 200; s_cal_y_max = 3900;
				} else {
						// Add 5% margin to ranges to prevent edge overshoot
						uint16_t x_margin = (s_cal_x_max - s_cal_x_min) / 20;
						uint16_t y_margin = (s_cal_y_max - s_cal_y_min) / 20;
						s_cal_x_min = (s_cal_x_min > x_margin) ? (s_cal_x_min - x_margin) : 100;
						s_cal_x_max = (s_cal_x_max < (4000 - x_margin)) ? (s_cal_x_max + x_margin) : 3900;
						s_cal_y_min = (s_cal_y_min > y_margin) ? (s_cal_y_min - y_margin) : 100;
						s_cal_y_max = (s_cal_y_max < (4000 - y_margin)) ? (s_cal_y_max + y_margin) : 3900;
				}

				// Save calibration to NVS
				nvs_handle_t nvs_handle;
				esp_err_t err = nvs_open(NVS_CALIBRATION_NAMESPACE, NVS_READWRITE, &nvs_handle);
				if (err == ESP_OK) {
						nvs_set_u16(nvs_handle, NVS_KEY_X_MIN, s_cal_x_min);
						nvs_set_u16(nvs_handle, NVS_KEY_X_MAX, s_cal_x_max);
						nvs_set_u16(nvs_handle, NVS_KEY_Y_MIN, s_cal_y_min);
						nvs_set_u16(nvs_handle, NVS_KEY_Y_MAX, s_cal_y_max);
						nvs_set_u8(nvs_handle, NVS_KEY_SWAP_XY, sSwapXY ? 1 : 0);
						nvs_set_u8(nvs_handle, NVS_KEY_FLIP_X, sFlipX ? 1 : 0);
						nvs_set_u8(nvs_handle, NVS_KEY_FLIP_Y, sFlipY ? 1 : 0);
						nvs_commit(nvs_handle);
						nvs_close(nvs_handle);
						ESP_LOGI(TAG, "Calibration saved to NVS");
				} else {
						ESP_LOGE(TAG, "Failed to open NVS for calibration save: %d", err);
				}

				// Clean up calibration UI
				lv_obj_del(marker_h);
				lv_obj_del(marker_v);
				lv_obj_del(msg);
				ESP_LOGI(TAG, "Calibration set: X[%u..%u] Y[%u..%u]", s_cal_x_min, s_cal_x_max, s_cal_y_min, s_cal_y_max);
				// Print mapping details for debugging
				int32_t x_range = (int32_t)s_cal_x_max - (int32_t)s_cal_x_min;
				int32_t y_range = (int32_t)s_cal_y_max - (int32_t)s_cal_y_min;
				ESP_LOGI(TAG, "Mapping: x=(raw_x-%u)*(%d)/%d, y=(raw_y-%u)*(%d)/%d [swapXY=%d, flipX=%d, flipY=%d]",
						 s_cal_x_min, (TFT_WIDTH - 1), x_range,
						 s_cal_y_min, (TFT_HEIGHT - 1), y_range,
						 (int)sSwapXY, (int)sFlipX, (int)sFlipY);
				sCalibrated = true;
		}

void display_create_ui(bool skip_calibration, bool provisioning_mode)
{
		ESP_LOGI(TAG, "Creating UI (provisioning_mode=%d)", provisioning_mode);

		// Skip calibration wizard if already calibrated from NVS or if explicitly skipped
		if (!sCalibrated && !skip_calibration) {
				run_touch_calibration(lv_scr_act());
				// Clean screen after calibration before creating main UI
				lv_obj_clean(lv_scr_act());
		} else {
				if (sCalibrated) {
						ESP_LOGI(TAG, "Using saved calibration - skipping wizard");
				} else {
						ESP_LOGI(TAG, "Skipping calibration wizard (requested by caller)");
				}
		}

		// Create UI based on mode
		if (provisioning_mode) {
				display_ui_create_provisioning(lv_scr_act());
		} else {
				display_ui_create_main(lv_scr_act(), &touch_dot, &s_flush_count);
		}
}

// LVGL task that runs continuously
static void lvgl_task(void *arg)
{
		ESP_LOGI(TAG, "LVGL task started on core %d", xPortGetCoreID());

		while (1) {
				// Call LVGL timer handler
				lv_timer_handler();

				// Update touch dot visibility/position if pressed
				if (s_touch_indev && touch_dot) {
						if (s_touch_indev->proc.state == LV_INDEV_STATE_PRESSED) {
								lv_point_t p;
								lv_indev_get_point(s_touch_indev, &p);
								lv_obj_clear_flag(touch_dot, LV_OBJ_FLAG_HIDDEN);
								lv_obj_set_pos(touch_dot, p.x - 5, p.y - 5);
						} else {
								lv_obj_add_flag(touch_dot, LV_OBJ_FLAG_HIDDEN);
						}
				}

				// Run at ~100Hz for responsive UI
				vTaskDelay(pdMS_TO_TICKS(10));
		}
}

void display_start_lvgl_task(void)
{
		if (s_lvgl_task_handle != NULL) {
				ESP_LOGW(TAG, "LVGL task already running");
				return;
		}

		// Create LVGL task with priority 3 (medium priority)
		// Priority: WiFi/Network=20-23, DNS=5, LVGL=3, Display_prov=2
		BaseType_t ret = xTaskCreate(lvgl_task, "lvgl", 8192, NULL, 3, &s_lvgl_task_handle);
		if (ret != pdPASS) {
				ESP_LOGE(TAG, "Failed to create LVGL task");
				s_lvgl_task_handle = NULL;
		} else {
				ESP_LOGI(TAG, "LVGL task created successfully");
		}
}

/**
 * @brief Suspend the LVGL task to free memory during OTA
 *
 * This suspends the LVGL rendering task and can free up to ~100KB
 * from frame buffers and LVGL memory pools. Used during OTA updates
 * when display is not needed.
 */
void display_suspend(void)
{
		if (s_lvgl_task_handle == NULL) {
				ESP_LOGW(TAG, "LVGL task not running, nothing to suspend");
				return;
		}

		ESP_LOGI(TAG, "Suspending display for OTA operation...");

		// Log heap before suspension
		size_t heap_before = esp_get_free_heap_size();
		ESP_LOGI(TAG, "Free heap before suspend: %lu bytes", heap_before);

		// Suspend the LVGL task
		vTaskSuspend(s_lvgl_task_handle);

		// Give system time to release resources
		vTaskDelay(pdMS_TO_TICKS(100));

		// Log heap after suspension
		size_t heap_after = esp_get_free_heap_size();
		ESP_LOGI(TAG, "Free heap after suspend: %lu bytes", heap_after);
		ESP_LOGI(TAG, "Memory freed by suspension: %ld bytes", (long)(heap_after - heap_before));

		ESP_LOGI(TAG, "Display suspended successfully");
}

/**
 * @brief Resume the LVGL task after OTA
 *
 * Restores normal display operation after OTA update is complete.
 */
void display_resume(void)
{
		if (s_lvgl_task_handle == NULL) {
				ESP_LOGW(TAG, "LVGL task not running, nothing to resume");
				return;
		}

		ESP_LOGI(TAG, "Resuming display after OTA operation...");

		// Resume the LVGL task
		vTaskResume(s_lvgl_task_handle);

		ESP_LOGI(TAG, "Display resumed successfully");
}

// ========================================
// OTA Display Management: Full Deinit with RGB LED
// ========================================

// RGB LED control for OTA status feedback
static void ota_led_init(void)
{
		ESP_LOGI(TAG, "Initializing RGB LED on GPIO %d for OTA status", OTA_RGB_LED_GPIO);

		rmt_tx_channel_config_t tx_config = {
				.gpio_num = (gpio_num_t)OTA_RGB_LED_GPIO,
				.clk_src = RMT_CLK_SRC_DEFAULT,
				.resolution_hz = RMT_LED_RESOLUTION_HZ,
				.mem_block_symbols = 64,
				.trans_queue_depth = 4,
			.intr_priority = 0,
			.flags = {}
		};
		ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_config, &s_led_chan));
		ESP_ERROR_CHECK(rmt_enable(s_led_chan));

		rmt_copy_encoder_config_t encoder_config = {};
		ESP_ERROR_CHECK(rmt_new_copy_encoder(&encoder_config, &s_led_encoder));
}

static void ota_led_set_color(uint8_t r, uint8_t g, uint8_t b)
{
		if (!s_led_chan) return;

		// WS2812 timing: T0H=0.4us, T0L=0.85us, T1H=0.8us, T1L=0.45us
		rmt_symbol_word_t bit0 = {.duration0 = 4, .level0 = 1, .duration1 = 8, .level1 = 0};
		rmt_symbol_word_t bit1 = {.duration0 = 8, .level0 = 1, .duration1 = 4, .level1 = 0};
		rmt_symbol_word_t reset = {.duration0 = 500, .level0 = 0, .duration1 = 0, .level1 = 0};

		rmt_symbol_word_t symbols[25];
		uint8_t bytes[3] = {g, r, b};  // WS2812 wants GRB

		for (int byte_idx = 0; byte_idx < 3; byte_idx++) {
				for (int bit = 7; bit >= 0; bit--) {
						symbols[byte_idx * 8 + (7 - bit)] = (bytes[byte_idx] & (1 << bit)) ? bit1 : bit0;
				}
		}
		symbols[24] = reset;

		rmt_transmit_config_t tx_cfg = {
				.loop_count = 0,
				.flags = {
						.eot_level = 0,
						.queue_nonblocking = 0
				}
		};
		rmt_transmit(s_led_chan, s_led_encoder, symbols, sizeof(symbols), &tx_cfg);
		rmt_tx_wait_all_done(s_led_chan, -1);
}

void display_ota_prepare(void)
{
	ESP_LOGI(TAG, "========================================");
	ESP_LOGI(TAG, "Preparing display for OTA (free buffers for memory)");
	ESP_LOGI(TAG, "========================================");

	size_t heap_before = esp_get_free_heap_size();
	ESP_LOGI(TAG, "Free heap before: %lu bytes", heap_before);

	// Suspend LVGL task first
	if (s_lvgl_task_handle) {
		vTaskSuspend(s_lvgl_task_handle);
		vTaskDelay(pdMS_TO_TICKS(100));
	}

	// Free display buffers to make room for OTA
	if (buf1) {
		heap_caps_free(buf1);
		buf1 = NULL;
	}
	if (buf2) {
		heap_caps_free(buf2);
		buf2 = NULL;
	}
	if (s_line_rgb666) {
		heap_caps_free(s_line_rgb666);
		s_line_rgb666 = NULL;
	}

	size_t heap_after = esp_get_free_heap_size();
	ESP_LOGI(TAG, "Free heap after: %lu bytes", heap_after);
	ESP_LOGI(TAG, "Memory freed: %ld bytes (~%ld KB)",
					 (long)(heap_after - heap_before), (long)(heap_after - heap_before) / 1024);

	// Initialize RGB LED for status
	ota_led_init();
	ota_led_set_color(0, 0, 255);  // Blue = preparing
}

void display_ota_restore(bool success)
{
	if (success) {
			ESP_LOGI(TAG, "OTA successful - flashing LED green 3 times");
			// Flash green 3 times before reboot
			for (int i = 0; i < 3; i++) {
					ota_led_set_color(0, 255, 0);  // Green
					vTaskDelay(pdMS_TO_TICKS(200));
					ota_led_set_color(0, 0, 0);    // Off
					vTaskDelay(pdMS_TO_TICKS(200));
			}
			return;  // Device will reboot
	}

	ESP_LOGI(TAG, "========================================");
	ESP_LOGI(TAG, "OTA failed - restoring display buffers and resuming");
	ESP_LOGI(TAG, "========================================");
	ota_led_set_color(255, 0, 0);  // Red = error
	vTaskDelay(pdMS_TO_TICKS(1000));  // Show for 1 second

	// Clean up LED
	if (s_led_chan) {
			ota_led_set_color(0, 0, 0);  // Off
			rmt_disable(s_led_chan);
			rmt_del_channel(s_led_chan);
			s_led_chan = NULL;
	}
	if (s_led_encoder) {
			rmt_del_encoder(s_led_encoder);
			s_led_encoder = NULL;
	}

	// Reallocate display buffers
	const int buf_size = TFT_WIDTH * 40;
	buf1 = (lv_color_t *)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
	buf2 = (lv_color_t *)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
	s_line_rgb666 = (uint8_t *)heap_caps_malloc(TFT_WIDTH * 3, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

	if (!buf1 || !buf2 || !s_line_rgb666) {
			ESP_LOGE(TAG, "Failed to reallocate display buffers after OTA!");
			return;
	}

	// Reinitialize LVGL draw buffer
	lv_disp_draw_buf_init(&disp_buf, buf1, buf2, buf_size);
	disp_drv.draw_buf = &disp_buf;

	ESP_LOGI(TAG, "Display buffers reallocated successfully");

	// Resume LVGL task
	if (s_lvgl_task_handle) {
			vTaskResume(s_lvgl_task_handle);
			ESP_LOGI(TAG, "Display resumed successfully");
	}
}

void display_ota_restore_no_update(void)
{
	ESP_LOGI(TAG, "========================================");
	ESP_LOGI(TAG, "Firmware up to date - restoring display");
	ESP_LOGI(TAG, "========================================");

	// Clean up LED
	if (s_led_chan) {
			ota_led_set_color(0, 0, 0);  // Off
			rmt_disable(s_led_chan);
			rmt_del_channel(s_led_chan);
			s_led_chan = NULL;
	}
	if (s_led_encoder) {
			rmt_del_encoder(s_led_encoder);
			s_led_encoder = NULL;
	}

	// Reallocate display buffers
	const int buf_size = TFT_WIDTH * 40;
	buf1 = (lv_color_t *)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
	buf2 = (lv_color_t *)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
	s_line_rgb666 = (uint8_t *)heap_caps_malloc(TFT_WIDTH * 3, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

	if (!buf1 || !buf2 || !s_line_rgb666) {
			ESP_LOGE(TAG, "Failed to reallocate display buffers!");
			return;
	}

	// Reinitialize LVGL draw buffer
	lv_disp_draw_buf_init(&disp_buf, buf1, buf2, buf_size);
	disp_drv.draw_buf = &disp_buf;

	ESP_LOGI(TAG, "Display buffers reallocated successfully");

	// Resume LVGL task
	if (s_lvgl_task_handle) {
			vTaskResume(s_lvgl_task_handle);
			ESP_LOGI(TAG, "Display resumed successfully");
	}
}

// Turn off OTA status LED (called after successful OTA boot)
void display_ota_led_off(void)
{
		ESP_LOGI(TAG, "Turning off OTA status LED after reboot");

		// After reboot, s_led_chan is NULL but LED hardware is still on
		// Initialize LED just to turn it off
		if (!s_led_chan) {
				ota_led_init();
		}

		// Turn off LED - send multiple times to ensure it takes
		for (int i = 0; i < 3; i++) {
				ota_led_set_color(0, 0, 0);  // Off
				vTaskDelay(pdMS_TO_TICKS(50));
		}

		// Keep channel alive - don't delete it
		// WS2812 needs the signal to persist or it reverts to previous state
		ESP_LOGI(TAG, "OTA LED turned off (channel kept alive)");
}

void display_update(void)
{
		// Legacy function - now deprecated, kept for compatibility during calibration
		uint32_t now = lv_tick_get();
		if (now - s_last_handler_ms >= 40) { // ~25 FPS max
				lv_timer_handler();
				s_last_handler_ms = now;
		}

		// Update touch dot visibility/position if pressed
		if (s_touch_indev && touch_dot) {
				if (s_touch_indev->proc.state == LV_INDEV_STATE_PRESSED) {
						lv_point_t p;
						lv_indev_get_point(s_touch_indev, &p);
						lv_obj_clear_flag(touch_dot, LV_OBJ_FLAG_HIDDEN);
						lv_obj_set_pos(touch_dot, p.x - 5, p.y - 5);
				} else {
						lv_obj_add_flag(touch_dot, LV_OBJ_FLAG_HIDDEN);
				}
		}
}
