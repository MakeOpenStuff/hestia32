#include "core/display_config.h"
#include "core/display_manager.h"
#include "core/display_ui.h"
#include "core/core_config.h"
#include "core/user_settings.h"
#include "core/ui/ui_theme.h"
#include "core/ui/ui_settings_system.h"
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
/* RGB666 conversion buffer holds LVGL_BUFFER_HEIGHT rows at max width.
 * Matching chunk lines to LVGL_BUFFER_HEIGHT means every flush region is
 * converted and sent in a SINGLE RAMWR transaction per chunk — eliminating
 * per-row CASET+PASET commands that can race with async DMA and cause a
 * systematic 1-pixel-per-row shear on horizontal lines. */
#define RGB666_CHUNK_LINES LVGL_BUFFER_HEIGHT

static const char *TAG = "display";
//TODO: Check timeouts for calibration and blocking WiFi provisioning screens

#define XIAO_DISPLAY_SAFE_INIT 0

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

/* Called from ISR context when the SPI DMA transfer for tx_color completes.
 * Signalling LVGL here (instead of immediately after tx_color returns) prevents
 * the next flush_cb from overwriting s_line_rgb666 while DMA is still reading it,
 * which manifested as random pixel corruption on horizontal edges. */
static bool IRAM_ATTR lcd_trans_done_cb(esp_lcd_panel_io_handle_t panel_io,
                                        esp_lcd_panel_io_event_data_t *edata,
                                        void *user_ctx)
{
    (void)panel_io; (void)edata; (void)user_ctx;
    lv_disp_flush_ready(&disp_drv);
    return false; /* no higher-priority task woken */
}

// Limit LVGL handler cadence to reduce tearing
static uint32_t s_last_handler_ms = 0;
static volatile uint32_t s_flush_count = 0;
// Touch
static spi_device_handle_t s_touch_dev = NULL;
static TaskHandle_t s_lvgl_task_handle = NULL;
static lv_indev_t *s_touch_indev = NULL;
static lv_obj_t *s_touch_cursor = NULL;  /* Touch pointer visual indicator */
static bool s_display_paused_for_ota = false;
static bool s_backlight_ramp_started = false;
static bool sSwapXY = false;

/* ── Backlight sleep state ──────────────────────────────── */
static volatile uint32_t s_last_touch_ms    = 0;   /* millis of last touch event  */
static volatile bool     s_display_sleeping = false; /* true when dimmed to sleep bri */
static volatile uint32_t s_wake_time_ms     = 0;   /* millis when display woke from sleep */
static uint8_t           s_active_bri       = 80;  /* current target brightness %   */
static uint8_t           s_sleep_bri        = 0;   /* sleep brightness %            */

/* ── Forward declarations ────────────────────────────────── */
static void run_touch_calibration(lv_obj_t *scr);
static uint32_t          s_sleep_timeout_ms = 60000;/* default 60 s                  */
#define WAKE_DEBOUNCE_MS 400  /* ignore touches for 400ms after waking */

/* Set LEDC backlight duty from a 0-100 % value */
static void backlight_set_pct(uint8_t pct)
{
    if (TFT_BL < 0) return;
    uint32_t duty = ((uint32_t)pct * 255) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

/* Called from touch callback or any user-interaction event to reset sleep timer */
void display_notify_activity(void)
{
    s_last_touch_ms = lv_tick_get();
    if (s_display_sleeping) {
        s_display_sleeping = false;
        s_wake_time_ms = lv_tick_get();
        
        /* Wake display controller first */
        if (panel_handle) {
            esp_lcd_panel_disp_on_off(panel_handle, true);
        }
        
        /* Restore backlight */
        backlight_set_pct(s_active_bri);
        
        /* Force full screen refresh to clear any retention */
        lv_obj_invalidate(lv_scr_act());
        lv_refr_now(lv_disp_get_default());
    }
}

void display_reload_settings(void)
{
	display_settings_t ds;
	if (display_settings_init() == ESP_OK && display_settings_get(&ds) == ESP_OK) {
		s_active_bri       = ds.brightness;
		s_sleep_bri        = ds.sleep_bri;
		s_sleep_timeout_ms = (uint32_t)ds.sleep_timeout_sec * 1000;
		ESP_LOGI(TAG, "Display settings reloaded: brightness=%d%%, sleep_bri=%d%%, sleep_timeout=%lums",
		         (int)s_active_bri, (int)s_sleep_bri, s_sleep_timeout_ms);
		// Apply active brightness immediately if not sleeping
		if (!s_display_sleeping) {
			backlight_set_pct(s_active_bri);
		}
	} else {
		ESP_LOGE(TAG, "Failed to reload display settings from NVS");
	}
}
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
static esp_err_t backlight_init(void)
{
		if (TFT_BL < 0) {
				ESP_LOGW(TAG, "Backlight GPIO disabled (TFT_BL=%d), skipping PWM backlight init", TFT_BL);
				return ESP_OK;
		}

		esp_err_t ret;
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
		ret = ledc_timer_config(&ledc_timer);
		if (ret != ESP_OK) return ret;
		ret = ledc_channel_config(&ledc_channel);
		if (ret != ESP_OK) return ret;
		// Start dark, then ramp after panel/UI init so fade is visible.
		ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
		if (ret != ESP_OK) return ret;
		ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
		if (ret != ESP_OK) return ret;
		return ESP_OK;
}

static void backlight_ramp_task(void *arg)
{
		(void)arg;
		vTaskDelay(pdMS_TO_TICKS(700));

		/* Load target brightness from NVS (default 80%) */
		uint8_t target_pct = 80;
		display_settings_t ds;
		if (display_settings_init() == ESP_OK && display_settings_get(&ds) == ESP_OK) {
				target_pct = ds.brightness;
				s_active_bri       = ds.brightness;
				s_sleep_bri        = ds.sleep_bri;
				s_sleep_timeout_ms = (uint32_t)ds.sleep_timeout_sec * 1000;
		}
		uint32_t target_duty = ((uint32_t)target_pct * 255) / 100;

		ESP_LOGI(TAG, "Backlight gradual ramp start (target %u%%)", target_pct);
		for (uint32_t duty = 0; duty <= target_duty; duty += 16) {
			esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
			if (ret != ESP_OK) {
					ESP_LOGE(TAG, "Backlight ramp set_duty failed: %s", esp_err_to_name(ret));
					break;
			}
			ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
			if (ret != ESP_OK) {
					ESP_LOGE(TAG, "Backlight ramp update_duty failed: %s", esp_err_to_name(ret));
					break;
			}
			vTaskDelay(pdMS_TO_TICKS(90));
		}
		/* Ensure we land exactly on target */
		ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, target_duty);
		ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

		ESP_LOGI(TAG, "Backlight gradual ramp complete");
		vTaskDelete(NULL);
}

// LVGL flush callback
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
		esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t) drv->user_data;
		const int offsetx1 = area->x1;
		const int offsetx2 = area->x2;
		const int offsety1 = area->y1;
		const int offsety2 = area->y2;

		const int width = (offsetx2 - offsetx1 + 1);
		const int height = (offsety2 - offsety1 + 1);

#if XIAO_DISPLAY_SAFE_INIT
		// In XIAO safe mode use RGB565 directly to avoid any format-conversion ambiguity.
		esp_lcd_panel_draw_bitmap(panel, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
#else

		/* Convert RGB565 → RGB666 and send in the fewest possible RAMWR transactions.
		 * The old dummy UI never had a shear problem because it called draw_bitmap
		 * ONCE for the entire flush area (XIAO safe-init path).  With lines_per_chunk=1
		 * we issued one CASET+PASET+RAMWR per row; if the async DMA for row N was still
		 * clocking bytes when the CASET command for row N+1 arrived, the ILI9488 could
		 * consume those command bytes as pixel data, shifting every subsequent row by
		 * 1–2 pixels — producing the systematic 1-pixel staircase on horizontal edges.
		 * Fix: derive lines_per_chunk from the actual buffer size so the whole flush
		 * area is sent in as few RAMWR calls as possible (ideally one). */
		const uint16_t *src = (const uint16_t *)color_map;
		size_t bytes_per_line = (size_t)width * 3;
		/* How many rows fit in the pre-allocated conversion buffer?
		 * Handles the fallback case (1-line buffer) safely. */
		int lines_per_chunk = (int)(s_line_rgb666_size / bytes_per_line);
		if (lines_per_chunk < 1) lines_per_chunk = 1;

		for (int y0 = 0; y0 < height; y0 += lines_per_chunk) {
				int chunk_h = height - y0;
				if (chunk_h > lines_per_chunk) {
						chunk_h = lines_per_chunk;
				}

				for (int y = 0; y < chunk_h; y++) {
						const uint16_t *row = src + (size_t)(y0 + y) * (size_t)width;
						uint8_t *dst = s_line_rgb666 + (size_t)y * bytes_per_line;
						for (int x = 0; x < width; x++) {
								const uint16_t pixel = row[x];
								const size_t idx = (size_t)x * 3;
								dst[idx + 0] = ((pixel >> 11) & 0x1F) << 3; // R
								dst[idx + 1] = ((pixel >> 5) & 0x3F) << 2;  // G
								dst[idx + 2] = (pixel & 0x1F) << 3;         // B
						}
				}

				esp_lcd_panel_draw_bitmap(panel,
						offsetx1,
						offsety1 + y0,
						offsetx2 + 1,
						offsety1 + y0 + chunk_h,
						s_line_rgb666);
		}
#endif
		s_flush_count = s_flush_count + 1;
		/* lv_disp_flush_ready is called from lcd_trans_done_cb (DMA completion ISR)
		 * to guarantee s_line_rgb666 is not reused before DMA finishes. */
#if XIAO_DISPLAY_SAFE_INIT
		lv_disp_flush_ready(drv); /* safe-init path has no DMA callback */
#endif
}

esp_err_t display_init(void)
{
		ESP_LOGI(TAG, "Initializing display");
		esp_err_t ret;

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
		ESP_LOGI(TAG, "Display init step: backlight");
#if XIAO_DISPLAY_SAFE_INIT
		ESP_LOGW(TAG, "XIAO safe init: TFT_BL=%d", TFT_BL);
		#if TFT_BL >= 0
				ESP_LOGW(TAG, "XIAO safe init: forcing backlight GPIO ON (no PWM)");
				gpio_config_t bl_conf = {
						.pin_bit_mask = 1ULL << TFT_BL,
						.mode = GPIO_MODE_OUTPUT,
						.pull_up_en = GPIO_PULLUP_DISABLE,
						.pull_down_en = GPIO_PULLDOWN_DISABLE,
						.intr_type = GPIO_INTR_DISABLE,
				};
				ret = gpio_config(&bl_conf);
				if (ret != ESP_OK) {
						ESP_LOGE(TAG, "Display init failed at backlight gpio_config: %s", esp_err_to_name(ret));
						return ret;
				}
				ret = gpio_set_level((gpio_num_t)TFT_BL, 1);
				if (ret != ESP_OK) {
						ESP_LOGE(TAG, "Display init failed at backlight gpio_set_level: %s", esp_err_to_name(ret));
						return ret;
				}
		#else
				ESP_LOGW(TAG, "XIAO safe init: no backlight GPIO configured (TFT_BL=%d)", TFT_BL);
		#endif
#else
		ret = backlight_init();
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Display init failed at backlight: %s", esp_err_to_name(ret));
				return ret;
		}
#endif

		// Allocate RGB666 conversion buffer FIRST (chunked, but memory-bounded).
		s_line_rgb666_size = (size_t)TFT_WIDTH * RGB666_CHUNK_LINES * 3;
		s_line_rgb666 = (uint8_t *)heap_caps_malloc(s_line_rgb666_size, MALLOC_CAP_DMA);
		if (!s_line_rgb666) {
				// Fallback to a single-line conversion buffer if memory is tight.
				s_line_rgb666_size = TFT_WIDTH * 3;
				s_line_rgb666 = (uint8_t *)heap_caps_malloc(s_line_rgb666_size, MALLOC_CAP_DMA);
				if (!s_line_rgb666) {
						ESP_LOGE(TAG, "Failed to allocate RGB666 buffer");
						return ESP_ERR_NO_MEM;
				}
				ESP_LOGW(TAG, "Using single-line RGB666 buffer fallback");
		}
		ESP_LOGI(TAG, "Display init step: RGB666 buffer allocated");

		// Configure SPI bus (skip if already initialized for reinit scenario)
		if (!s_spi_initialized) {
				spi_bus_config_t buscfg = {};
				buscfg.mosi_io_num = TFT_MOSI;
				#if XIAO_DISPLAY_SAFE_INIT
				buscfg.miso_io_num = -1; // Safe mode: no touch, keep display bus write-only
				#else
				buscfg.miso_io_num = TOUCH_MISO; // Dedicated to touch only
				#endif
				buscfg.sclk_io_num = TFT_SCLK;
				buscfg.quadwp_io_num = -1;
				buscfg.quadhd_io_num = -1;
				buscfg.max_transfer_sz = TFT_WIDTH * RGB666_CHUNK_LINES * 3;
				#if XIAO_DISPLAY_SAFE_INIT
				buscfg.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_SCLK | SPICOMMON_BUSFLAG_MOSI;
				#else
				buscfg.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_SCLK | SPICOMMON_BUSFLAG_MOSI | SPICOMMON_BUSFLAG_MISO;
				#endif
				ret = spi_bus_initialize(TFT_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
				if (ret != ESP_OK) {
						ESP_LOGE(TAG, "Display init failed at spi_bus_initialize: %s", esp_err_to_name(ret));
						return ret;
				}
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
		io_config.trans_queue_depth = 1;  // Artifact-safe: keep only one in-flight transfer.
		io_config.lcd_cmd_bits = LCD_CMD_BITS;
		io_config.lcd_param_bits = LCD_PARAM_BITS;
		/* Signal LVGL only when DMA is fully done (not when tx_color returns).
		 * Without this, the next flush_cb converts pixels into s_line_rgb666 while
		 * the previous DMA is still reading from it, causing random corruption. */
		io_config.on_color_trans_done = lcd_trans_done_cb;
		io_config.user_ctx = NULL;
		ESP_LOGI(TAG, "Display init step: panel IO");
		ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)TFT_SPI_HOST, &io_config, &io_handle);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Display init failed at esp_lcd_new_panel_io_spi: %s", esp_err_to_name(ret));
				return ret;
		}

		// Add touch device (XPT2046) on same SPI bus
#if !XIAO_DISPLAY_SAFE_INIT
		{
				spi_device_interface_config_t devcfg = {};
				devcfg.clock_speed_hz = 2 * 1000 * 1000; // 2 MHz for touch
				devcfg.mode = 0; // Try mode 0 first (CPOL=0, CPHA=0)
				devcfg.spics_io_num = TOUCH_CS;
				devcfg.queue_size = 3;
				devcfg.flags = 0; // full duplex
				devcfg.input_delay_ns = 0; // Remove delay for now
				ESP_LOGI(TAG, "Display init step: touch device");
				ret = spi_bus_add_device(TFT_SPI_HOST, &devcfg, &s_touch_dev);
				if (ret != ESP_OK) {
						ESP_LOGE(TAG, "Display init failed at spi_bus_add_device: %s", esp_err_to_name(ret));
						return ret;
				}

				// Configure IRQ pin (active low)
				gpio_config_t io_conf = {};
				io_conf.pin_bit_mask = 1ULL << TOUCH_IRQ;
				io_conf.mode = GPIO_MODE_INPUT;
				io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
				io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
				io_conf.intr_type = GPIO_INTR_DISABLE;
				ret = gpio_config(&io_conf);
				if (ret != ESP_OK) {
						ESP_LOGE(TAG, "Display init failed at touch gpio_config: %s", esp_err_to_name(ret));
						return ret;
				}
		}
#else
		ESP_LOGW(TAG, "XIAO safe init: skipping touch controller setup");
#endif

		// Configure ILI9488 panel
		esp_lcd_panel_dev_config_t panel_config = {};
		panel_config.reset_gpio_num = TFT_RST;
		panel_config.rgb_endian = LCD_RGB_ENDIAN_RGB;
		#if XIAO_DISPLAY_SAFE_INIT
		panel_config.bits_per_pixel = 16; // Safer baseline path for XIAO diagnostics
		#else
		panel_config.bits_per_pixel = 24; // ILI9488 SPI expects 18-bit (24bpp container)
		#endif
		ESP_LOGI(TAG, "Display init step: panel create");
		ret = esp_lcd_new_panel_ili9488(io_handle, &panel_config, &panel_handle);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Display init failed at esp_lcd_new_panel_ili9488: %s", esp_err_to_name(ret));
				return ret;
		}

		// Reset and initialize panel
		ESP_LOGI(TAG, "Display init step: panel reset/init");
		ret = esp_lcd_panel_reset(panel_handle);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Display init failed at panel_reset: %s", esp_err_to_name(ret));
				return ret;
		}
		ret = esp_lcd_panel_init(panel_handle);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Display init failed at panel_init: %s", esp_err_to_name(ret));
				return ret;
		}

		// Some ILI9488 modules require explicit sleep-out/display-on sequence.
		ret = esp_lcd_panel_io_tx_param(io_handle, 0x11, NULL, 0); // Sleep out
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Display init failed at tx_param(0x11): %s", esp_err_to_name(ret));
				return ret;
		}
		vTaskDelay(pdMS_TO_TICKS(120));
		ret = esp_lcd_panel_io_tx_param(io_handle, 0x29, NULL, 0); // Display on
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Display init failed at tx_param(0x29): %s", esp_err_to_name(ret));
				return ret;
		}
		vTaskDelay(pdMS_TO_TICKS(20));

		// Set interface pixel format explicitly.
		{
				#if XIAO_DISPLAY_SAFE_INIT
				uint8_t pf = 0x55; // 16-bit/pixel RGB565
				#else
				uint8_t pf = 0x66; // 18-bit/pixel RGB666
				#endif
				ret = esp_lcd_panel_io_tx_param(io_handle, 0x3A, &pf, 1);
				if (ret != ESP_OK) {
						ESP_LOGE(TAG, "Display init failed at tx_param(0x3A): %s", esp_err_to_name(ret));
						return ret;
				}
		}

		// Landscape orientation: swap X/Y axes only — no mirroring needed
		ret = esp_lcd_panel_swap_xy(panel_handle, true);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Display init failed at panel_swap_xy: %s", esp_err_to_name(ret));
				return ret;
		}
		ret = esp_lcd_panel_mirror(panel_handle, false, false);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Display init failed at panel_mirror: %s", esp_err_to_name(ret));
				return ret;
		}
		ret = esp_lcd_panel_invert_color(panel_handle, false);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Display init failed at panel_invert_color: %s", esp_err_to_name(ret));
				return ret;
		}
		ret = esp_lcd_panel_disp_on_off(panel_handle, true);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Display init failed at panel_disp_on_off: %s", esp_err_to_name(ret));
				return ret;
		}

		ESP_LOGI(TAG, "Display init step: panel ready (test fill disabled)");

		lv_init();

		size_t buffer_size = TFT_WIDTH * LVGL_BUFFER_HEIGHT;
		size_t lv_buf_bytes = buffer_size * sizeof(lv_color_t);
		ESP_LOGI(TAG, "LVGL buffers: %u bytes each, free heap: %u",
				(unsigned)lv_buf_bytes, (unsigned)esp_get_free_heap_size());
		// LVGL draw buffers do not need DMA in this pipeline.
		buf1 = (lv_color_t *)heap_caps_malloc(lv_buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
		if (!buf1) {
				buf1 = (lv_color_t *)heap_caps_malloc(lv_buf_bytes, MALLOC_CAP_8BIT);
		}
		buf2 = (lv_color_t *)heap_caps_malloc(lv_buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
		if (!buf2) {
				buf2 = (lv_color_t *)heap_caps_malloc(lv_buf_bytes, MALLOC_CAP_8BIT);
		}
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
		disp_drv.full_refresh = 1;
		lv_disp_drv_register(&disp_drv);

		/* Tell LVGL the display background is opaque.
		 * Without this, partial-refresh dirty-region tracking can leave ghost
		 * pixels from the previous screen when switching between screens. */
		{
				lv_disp_t *disp = lv_disp_get_default();
				lv_disp_set_bg_color(disp, lv_color_hex(0x000000));
				lv_disp_set_bg_opa(disp, LV_OPA_COVER);
		}

		// Register LVGL touch input device
#if !XIAO_DISPLAY_SAFE_INIT
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
						// Map raw touch to screen coordinates.
						// IMPORTANT: integrate swap_xy into the scale factors.
						// If we map raw_x→[0,TFT_WIDTH-1] and raw_y→[0,TFT_HEIGHT-1] and THEN swap,
						// the X axis ends up scaled to TFT_HEIGHT (320) but LVGL expects TFT_WIDTH (480),
						// causing 66% horizontal compression and 150% vertical expansion.
						int32_t x, y;
						if (sSwapXY) {
								// physical X → screen Y  (scale to HEIGHT range)
								// physical Y → screen X  (scale to WIDTH range)
								x = (int32_t)(raw_y - s_cal_y_min) * (int32_t)(TFT_WIDTH  - 1) / (int32_t)(s_cal_y_max - s_cal_y_min);
								y = (int32_t)(raw_x - s_cal_x_min) * (int32_t)(TFT_HEIGHT - 1) / (int32_t)(s_cal_x_max - s_cal_x_min);
						} else {
								x = (int32_t)(raw_x - s_cal_x_min) * (int32_t)(TFT_WIDTH  - 1) / (int32_t)(s_cal_x_max - s_cal_x_min);
								y = (int32_t)(raw_y - s_cal_y_min) * (int32_t)(TFT_HEIGHT - 1) / (int32_t)(s_cal_y_max - s_cal_y_min);
						}
						// Flip corrections (swap already integrated above)
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

				/* ── Backlight sleep: wake on first touch, swallow event ── */
				if (s_display_sleeping) {
						display_notify_activity();
						data->state = LV_INDEV_STATE_REL; /* swallow — don't send to UI */
						return;
				}
			/* Debounce: ignore touches for 400ms after waking to prevent same physical touch
			 * from registering button presses (a single human touch generates multiple events) */
			uint32_t now_ms = lv_tick_get();
			if (s_wake_time_ms > 0 && (now_ms - s_wake_time_ms) < WAKE_DEBOUNCE_MS) {
					data->state = LV_INDEV_STATE_REL; /* swallow during debounce period */
					return;
			}
			display_notify_activity();

				data->point.x = (lv_coord_t)x;
				data->point.y = (lv_coord_t)y;
				data->state = LV_INDEV_STATE_PR;

				// Debug: print mapping on touch (reduced frequency)
				static uint32_t s_touch_log_ms = 0;
				if (now_ms - s_touch_log_ms > 1000) {
						ESP_LOGI(TAG, "touch raw:(%u,%u) clamped:(%u,%u) mapped:(%d,%d)",
										 (unsigned)raw_x0, (unsigned)raw_y0,
										 (unsigned)raw_x, (unsigned)raw_y,
										 (int)x, (int)y);
						s_touch_log_ms = now_ms;
				}
		};
		s_touch_indev = lv_indev_drv_register(&indev_drv);

		/* Create touch pointer visual indicator (themed, controlled by show_pointer setting) */
		s_touch_cursor = lv_obj_create(lv_layer_top());
		lv_obj_set_size(s_touch_cursor, 10, 10);

		/* Use themed primary color for visibility */
		const hestia_theme_t *theme = ui_theme_get();
		lv_obj_set_style_bg_color(s_touch_cursor, lv_color_hex(theme->primary), 0);
		lv_obj_set_style_bg_opa(s_touch_cursor, LV_OPA_70, 0);
		lv_obj_set_style_border_color(s_touch_cursor, lv_color_hex(theme->text), 0);
		lv_obj_set_style_border_width(s_touch_cursor, 1, 0);
		lv_obj_set_style_radius(s_touch_cursor, LV_RADIUS_CIRCLE, 0);
		lv_obj_set_style_pad_all(s_touch_cursor, 0, 0);
		lv_obj_clear_flag(s_touch_cursor, LV_OBJ_FLAG_CLICKABLE);
		lv_indev_set_cursor(s_touch_indev, s_touch_cursor);

		/* Initialize visibility based on device config */
		device_config_t cfg;
		if (device_config_get(&cfg) == ESP_OK && cfg.show_pointer) {
			lv_obj_clear_flag(s_touch_cursor, LV_OBJ_FLAG_HIDDEN);
			ESP_LOGI(TAG, "Touch pointer enabled at boot");
		} else {
			lv_obj_add_flag(s_touch_cursor, LV_OBJ_FLAG_HIDDEN);
		}
		}
#endif

		ESP_LOGI(TAG, "Display initialized successfully (LVGL enabled)");

		if (TFT_BL >= 0 && !s_backlight_ramp_started) {
				BaseType_t tr = xTaskCreate(backlight_ramp_task, "bl_ramp", 2048, NULL, 1, NULL);
				if (tr == pdPASS) {
						s_backlight_ramp_started = true;
				} else {
						ESP_LOGE(TAG, "Failed to create backlight ramp task");
				}
		}

		return ESP_OK;
}

bool display_has_calibration(void)
{
#if XIAO_DISPLAY_SAFE_INIT
		ESP_LOGW(TAG, "XIAO safe init: bypassing touch calibration check");
		return true;
#endif

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

// State to restore pointer visibility after recalibration
static bool s_pointer_was_visible = false;

// Timer callback to return to system settings after recalibration
static void recalibration_return_cb(lv_timer_t *timer)
{
	ESP_LOGI(TAG, "Returning to System Settings after recalibration");

	// Restore pointer visibility to state before calibration
	if (s_pointer_was_visible) {
		display_set_touch_pointer_visible(true);
	}

	ui_settings_system_open();
}

void display_recalibrate(void)
{
	ESP_LOGI(TAG, "Recalibration requested from UI");

	// Save current pointer visibility state and hide it during calibration
	s_pointer_was_visible = (s_touch_cursor != NULL && !lv_obj_has_flag(s_touch_cursor, LV_OBJ_FLAG_HIDDEN));
	if (s_pointer_was_visible) {
		display_set_touch_pointer_visible(false);
		ESP_LOGI(TAG, "Pointer hidden for calibration (will restore after)");
	}

	run_touch_calibration(lv_scr_act());

	// Show brief completion message
	lv_obj_clean(lv_scr_act());
	lv_obj_t *msg = lv_label_create(lv_scr_act());
	lv_label_set_text(msg, "Calibration complete!");
	lv_obj_set_style_text_font(msg, &lv_font_montserrat_20, 0);
	lv_obj_center(msg);
	lv_refr_now(NULL);

	// Brief delay, then return to system settings screen (will restore pointer)
	lv_timer_t *timer = lv_timer_create(recalibration_return_cb, 500, NULL);
	lv_timer_set_repeat_count(timer, 1);

	ESP_LOGI(TAG, "Return to settings scheduled");
}

		static void run_touch_calibration(lv_obj_t *scr)
		{
				ESP_LOGI(TAG, "Starting touch calibration (60 second timeout)");
				const hestia_theme_t *t = ui_theme_get();

				// Clear screen and set theme background
				lv_obj_clean(scr);
				lv_obj_set_style_bg_color(scr, lv_color_hex(t->bg), 0);

				// Force screen update to show background
				lv_refr_now(NULL);

				// Title
				lv_obj_t *title = lv_label_create(scr);
				lv_label_set_text(title, "Touch Calibration");
				lv_obj_set_style_text_color(title, lv_color_hex(t->text), 0);
				lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
				lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

				// Instruction message
				lv_obj_t *msg = lv_label_create(scr);
				lv_label_set_text(msg, "Touch target 1/5 (60s remaining)");
				lv_obj_set_style_text_color(msg, lv_color_hex(t->text_secondary), 0);
				lv_obj_set_style_text_font(msg, &lv_font_montserrat_16, 0);
				lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 60);

				// Target marker objects (crosshair with primary color)
				lv_obj_t *marker_h = lv_obj_create(scr);
				lv_obj_t *marker_v = lv_obj_create(scr);
				lv_obj_set_style_bg_color(marker_h, lv_color_hex(t->primary), 0);
				lv_obj_set_style_bg_color(marker_v, lv_color_hex(t->primary), 0);
				lv_obj_set_style_border_width(marker_h, 0, 0);
				lv_obj_set_style_border_width(marker_v, 0, 0);
				lv_obj_set_size(marker_h, 20, 2);
				lv_obj_set_size(marker_v, 2, 20);

				// Force immediate render of calibration UI
				lv_refr_now(NULL);

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

						// Force immediate refresh to show marker at new position
						lv_refr_now(NULL);

						ESP_LOGI(TAG, "Waiting for touch on target %d at (%d, %d)", i + 1, targets[i].x, targets[i].y);

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
														lv_refr_now(NULL);  // Force update of countdown
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
				/* Create main UI on a new hidden screen for smooth transition */
				lv_obj_t *new_scr = lv_obj_create(NULL);
				display_ui_create_main(new_scr);
				/* Atomically switch to the fully-prepared screen */
				lv_scr_load(new_scr);
		}
}

// LVGL task that runs continuously
static void lvgl_task(void *arg)
{
		ESP_LOGI(TAG, "LVGL task started on core %d", xPortGetCoreID());

		/* Load display sleep settings from NVS */
		{
				display_settings_t ds;
				if (display_settings_init() == ESP_OK && display_settings_get(&ds) == ESP_OK) {
						s_active_bri       = ds.brightness;
						s_sleep_bri        = ds.sleep_bri;
						s_sleep_timeout_ms = (uint32_t)ds.sleep_timeout_sec * 1000;
				}
		}
		s_last_touch_ms = lv_tick_get();

		while (1) {
				if (s_display_paused_for_ota) {
						vTaskDelay(pdMS_TO_TICKS(50));
						continue;
				}

				// Call LVGL timer handler
				lv_timer_handler();

				/* ── Display sleep check ──────── */
				if (!s_display_sleeping && s_sleep_timeout_ms > 0) {
						uint32_t idle = lv_tick_get() - s_last_touch_ms;
						if (idle >= s_sleep_timeout_ms) {
								s_display_sleeping = true;
								
								/* Dim backlight first */
								backlight_set_pct(s_sleep_bri);
								
								/* Turn off LCD controller to prevent image retention */
								if (panel_handle) {
										esp_lcd_panel_disp_on_off(panel_handle, false);
								}
						}
				}

// Touch indicator is now managed by ui_main internally

				// Run near ~20 FPS target; actual FPS depends on flush cost.
				vTaskDelay(pdMS_TO_TICKS(30));
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

void display_set_touch_pointer_visible(bool visible)
{
	if (s_touch_cursor == NULL) {
		ESP_LOGW(TAG, "Touch cursor not initialized");
		return;
	}

	if (visible) {
		lv_obj_clear_flag(s_touch_cursor, LV_OBJ_FLAG_HIDDEN);
		ESP_LOGI(TAG, "Touch pointer shown");
	} else {
		lv_obj_add_flag(s_touch_cursor, LV_OBJ_FLAG_HIDDEN);
		ESP_LOGI(TAG, "Touch pointer hidden");
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
		if (s_led_chan && s_led_encoder) {
				ESP_LOGI(TAG, "OTA RGB LED already initialized, reusing existing RMT channel");
				return;
		}

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
		esp_err_t ret = rmt_new_tx_channel(&tx_config, &s_led_chan);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "OTA RGB LED init failed at rmt_new_tx_channel: %s", esp_err_to_name(ret));
				s_led_chan = NULL;
				return;
		}

		ret = rmt_enable(s_led_chan);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "OTA RGB LED init failed at rmt_enable: %s", esp_err_to_name(ret));
				rmt_del_channel(s_led_chan);
				s_led_chan = NULL;
				return;
		}

		rmt_copy_encoder_config_t encoder_config = {};
		ret = rmt_new_copy_encoder(&encoder_config, &s_led_encoder);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "OTA RGB LED init failed at rmt_new_copy_encoder: %s", esp_err_to_name(ret));
				rmt_disable(s_led_chan);
				rmt_del_channel(s_led_chan);
				s_led_chan = NULL;
				s_led_encoder = NULL;
				return;
		}
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
	s_display_paused_for_ota = true;
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
	// Keep RGB conversion buffer allocated. Freeing it here can race with pending
	// panel transactions and corrupt flush callback state.

	size_t heap_after = esp_get_free_heap_size();
	ESP_LOGI(TAG, "Free heap after: %lu bytes", heap_after);
	ESP_LOGI(TAG, "Memory freed: %ld bytes (~%ld KB)",
					 (long)(heap_after - heap_before), (long)(heap_after - heap_before) / 1024);

	// Initialize RGB LED for status (best effort, non-fatal)
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
	const int buf_size = TFT_WIDTH * LVGL_BUFFER_HEIGHT;
	const size_t lv_buf_bytes = buf_size * sizeof(lv_color_t);
	buf1 = (lv_color_t *)heap_caps_malloc(lv_buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (!buf1) {
			buf1 = (lv_color_t *)heap_caps_malloc(lv_buf_bytes, MALLOC_CAP_8BIT);
	}
	buf2 = (lv_color_t *)heap_caps_malloc(lv_buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (!buf2) {
			buf2 = (lv_color_t *)heap_caps_malloc(lv_buf_bytes, MALLOC_CAP_8BIT);
	}
	if (!s_line_rgb666) {
		s_line_rgb666_size = TFT_WIDTH * 3;
		s_line_rgb666 = (uint8_t *)heap_caps_malloc(s_line_rgb666_size, MALLOC_CAP_DMA);
	}

	if (!buf1 || !s_line_rgb666) {
			ESP_LOGE(TAG, "Failed to reallocate display buffers after OTA!");
			return;
	}
	if (!buf2) {
			ESP_LOGW(TAG, "OTA restore: LVGL buffer 2 unavailable, using single-buffer mode");
	}

	// Reinitialize LVGL draw buffer
	lv_disp_draw_buf_init(&disp_buf, buf1, buf2, buf_size);
	disp_drv.draw_buf = &disp_buf;

	ESP_LOGI(TAG, "Display buffers reallocated successfully");

	// Resume LVGL task
	if (s_lvgl_task_handle) {
			s_display_paused_for_ota = false;
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
	const int buf_size = TFT_WIDTH * LVGL_BUFFER_HEIGHT;
	const size_t lv_buf_bytes = buf_size * sizeof(lv_color_t);
	buf1 = (lv_color_t *)heap_caps_malloc(lv_buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (!buf1) {
			buf1 = (lv_color_t *)heap_caps_malloc(lv_buf_bytes, MALLOC_CAP_8BIT);
	}
	buf2 = (lv_color_t *)heap_caps_malloc(lv_buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (!buf2) {
			buf2 = (lv_color_t *)heap_caps_malloc(lv_buf_bytes, MALLOC_CAP_8BIT);
	}
	if (!s_line_rgb666) {
		s_line_rgb666_size = TFT_WIDTH * 3;
		s_line_rgb666 = (uint8_t *)heap_caps_malloc(s_line_rgb666_size, MALLOC_CAP_DMA);
	}

	if (!buf1 || !s_line_rgb666) {
			ESP_LOGE(TAG, "Failed to reallocate display buffers!");
			return;
	}
	if (!buf2) {
			ESP_LOGW(TAG, "OTA no-update restore: LVGL buffer 2 unavailable, using single-buffer mode");
	}

	// Reinitialize LVGL draw buffer
	lv_disp_draw_buf_init(&disp_buf, buf1, buf2, buf_size);
	disp_drv.draw_buf = &disp_buf;

	ESP_LOGI(TAG, "Display buffers reallocated successfully");

	// Resume LVGL task
	if (s_lvgl_task_handle) {
			s_display_paused_for_ota = false;
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
		// Provisioning mode depends on this path; use real time instead of LVGL ticks
		// because lv_tick_inc is not driven in this project.
		uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
		if (now_ms - s_last_handler_ms >= 40) { // ~25 FPS max
				if (s_display_paused_for_ota) {
						return;
				}
				lv_timer_handler();
				s_last_handler_ms = now_ms;
		}

		// Touch indicator is managed internally by ui_main
}
