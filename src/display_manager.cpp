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
    io_config.pclk_hz = LCD_PIXEL_CLOCK_HZ;
    io_config.trans_queue_depth = 7;  // Slightly deeper queue for better pipelining
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

    lv_init();

    size_t buffer_size = TFT_WIDTH * LVGL_BUFFER_HEIGHT;
    buf1 = (lv_color_t *)heap_caps_malloc(buffer_size * sizeof(lv_color_t), MALLOC_CAP_DMA);
    buf2 = (lv_color_t *)heap_caps_malloc(buffer_size * sizeof(lv_color_t), MALLOC_CAP_DMA);
    if (!buf1 || !buf2) {
        ESP_LOGE(TAG, "Failed to allocate LVGL buffers");
        return ESP_ERR_NO_MEM;
    }

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, buffer_size);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = TFT_WIDTH;
    disp_drv.ver_res = TFT_HEIGHT;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Display initialized successfully (LVGL enabled)");
    return ESP_OK;
}

void display_create_ui(void)
{
    ESP_LOGI(TAG, "Creating heavy UI");

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);

    int cols = 4, rows = 6;
    int tile_w = TFT_WIDTH / cols;
    int tile_h = TFT_HEIGHT / rows;
    uint32_t colors[8] = {0x2364AA,0x3DA5D9,0x73BFB8,0xFEC601,0xEA7317,0x6C2E2F,0x8A2BE2,0x2ECC71};
    int ci = 0;
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            lv_obj_t *tile = lv_obj_create(scr);
            lv_obj_set_size(tile, tile_w-2, tile_h-2);
            lv_obj_set_style_bg_color(tile, lv_color_hex(colors[ci%8]), 0);
            lv_obj_set_style_radius(tile, 6, 0);
            lv_obj_set_pos(tile, x*tile_w+1, y*tile_h+1);
            ci++;
        }
    }

    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_size(bar, 20, TFT_HEIGHT-40);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(bar, 10, 20);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, bar);
    lv_anim_set_values(&a, 10, TFT_WIDTH-30);
    lv_anim_set_time(&a, 3000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_start(&a);

    lv_obj_t *fps = lv_label_create(scr);
    lv_label_set_text(fps, "FPS:");
    lv_obj_set_style_text_color(fps, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(fps, LV_ALIGN_TOP_LEFT, 8, 8);

    static uint32_t last_t = 0;
    static uint32_t frames = 0;
    lv_timer_t *tmr = lv_timer_create([](lv_timer_t *t){
        frames++;
        uint32_t now = lv_tick_get();
        if (now - last_t >= 1000) {
            char buf[32];
            lv_snprintf(buf, sizeof(buf), "FPS:%lu", (unsigned long)frames);
            lv_label_set_text((lv_obj_t*)t->user_data, buf);
            frames = 0;
            last_t = now;
        }
    }, 16, fps);
    (void)tmr;

    ESP_LOGI(TAG, "Heavy UI created");
}

void display_update(void)
{
    lv_timer_handler();
}
