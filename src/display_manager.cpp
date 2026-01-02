#include "display_manager.h"
#include "display_config.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9488.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "lvgl.h"
#include <string.h>

static const char *TAG = "display";

// Display handles
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
// LVGL buffers (disabled in minimal bring-up)
static lv_disp_drv_t disp_drv;
static lv_disp_draw_buf_t disp_buf;
static lv_color_t *buf1 = NULL;
static lv_color_t *buf2 = NULL;

// UI elements
static lv_obj_t *temp_label = NULL;
static lv_obj_t *target_label = NULL;

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
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    size_t len = (offsetx2 - offsetx1 + 1) * (offsety2 - offsety1 + 1);

    // ILI9488 in RGB666 mode: Convert RGB565 to RGB666
    uint8_t *rgb666_buf = (uint8_t *)heap_caps_malloc(len * 3, MALLOC_CAP_DMA);
    if (rgb666_buf) {
        uint16_t *rgb565 = (uint16_t *)color_map;

        for (size_t i = 0; i < len; i++) {
            uint16_t pixel = rgb565[i];

            // Extract RGB565 components
            uint8_t r5 = (pixel >> 11) & 0x1F;
            uint8_t g6 = (pixel >> 5) & 0x3F;
            uint8_t b5 = pixel & 0x1F;

            // Convert to 8-bit values
            uint8_t r8 = (r5 << 3) | (r5 >> 2);
            uint8_t g8 = (g6 << 2) | (g6 >> 4);
            uint8_t b8 = (b5 << 3) | (b5 >> 2);

            // Pack as RGB
            rgb666_buf[i*3 + 0] = r8;
            rgb666_buf[i*3 + 1] = g8;
            rgb666_buf[i*3 + 2] = b8;
        }

        esp_err_t ret = esp_lcd_panel_draw_bitmap(panel, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, rgb666_buf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "draw_bitmap failed: %d", ret);
        }
        free(rgb666_buf);
    } else {
        ESP_LOGE(TAG, "Failed to allocate RGB666 buffer");
    }

    lv_disp_flush_ready(drv);
}

esp_err_t display_init(void)
{
    ESP_LOGI(TAG, "Initializing display");

    // Initialize backlight
    backlight_init();

    // Configure SPI bus
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = TFT_MOSI;
    buscfg.miso_io_num = -1;
    buscfg.sclk_io_num = TFT_SCLK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = TFT_WIDTH * LVGL_BUFFER_HEIGHT * 3;
    ESP_ERROR_CHECK(spi_bus_initialize(TFT_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // Configure panel I/O
    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = TFT_CS;
    io_config.dc_gpio_num = TFT_DC;
    io_config.spi_mode = 0;
    io_config.pclk_hz = 5 * 1000 * 1000;
    io_config.trans_queue_depth = 5;  // Reduced from 10 for smoother transfers
    io_config.lcd_cmd_bits = LCD_CMD_BITS;
    io_config.lcd_param_bits = LCD_PARAM_BITS;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)TFT_SPI_HOST, &io_config, &io_handle));

    // Configure ILI9488 panel
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = TFT_RST;
    panel_config.rgb_endian = LCD_RGB_ENDIAN_RGB;
    panel_config.bits_per_pixel = 24;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9488(io_handle, &panel_config, &panel_handle));

    // Reset and initialize panel
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));  // Mirror X to fix left-right flip
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));  // Enable inversion to fix colors
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // Quick hardware test: draw a small solid rectangle before LVGL
    {
        const int x1 = 10, y1 = 10, x2 = 110, y2 = 110;
        size_t len = (x2 - x1) * (y2 - y1);
        uint8_t *rgb666_buf = (uint8_t *)heap_caps_malloc(len * 3, MALLOC_CAP_DMA);
        if (rgb666_buf) {
            // Solid red test
            for (size_t i = 0; i < len; i++) {
                rgb666_buf[i*3 + 0] = 255;
                rgb666_buf[i*3 + 1] = 0;
                rgb666_buf[i*3 + 2] = 0;
            }
            esp_err_t ret = esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2, y2, rgb666_buf);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Hardware test draw failed: %d", ret);
            } else {
                ESP_LOGI(TAG, "Hardware test rectangle drawn");
            }
            free(rgb666_buf);
        } else {
            ESP_LOGW(TAG, "Hardware test: failed to allocate buffer");
        }
    }

    // Minimal bring-up: skip LVGL for now to avoid old UI
    ESP_LOGI(TAG, "Display initialized successfully (minimal mode, LVGL disabled)");
    return ESP_OK;
}

void display_create_ui(void)
{
    // Minimal mode: skip creating LVGL UI
    ESP_LOGI(TAG, "LVGL UI creation skipped (minimal mode)");
}

void display_update(void)
{
    // Minimal mode: no LVGL timer
}
