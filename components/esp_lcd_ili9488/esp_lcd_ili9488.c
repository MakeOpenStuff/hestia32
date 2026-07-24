#include <stdlib.h>
#include <sys/cdefs.h>
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lcd_ili9488.h"

static const char *TAG = "ili9488";

// ILI9488 specific commands
#define ILI9488_CMD_NOP                  0x00
#define ILI9488_CMD_SWRESET              0x01
#define ILI9488_CMD_SLPIN                0x10
#define ILI9488_CMD_SLPOUT               0x11
#define ILI9488_CMD_INVOFF               0x20
#define ILI9488_CMD_INVON                0x21
#define ILI9488_CMD_DISPOFF              0x28
#define ILI9488_CMD_DISPON               0x29
#define ILI9488_CMD_CASET                0x2A
#define ILI9488_CMD_PASET                0x2B
#define ILI9488_CMD_RAMWR                0x2C
#define ILI9488_CMD_MADCTL               0x36
#define ILI9488_CMD_COLMOD               0x3A

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    uint8_t fb_bits_per_pixel;
    uint8_t madctl_val;
    uint8_t colmod_val;
} ili9488_panel_t;

static esp_err_t panel_ili9488_del(esp_lcd_panel_t *panel);
static esp_err_t panel_ili9488_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_ili9488_init(esp_lcd_panel_t *panel);
static esp_err_t panel_ili9488_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t panel_ili9488_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_ili9488_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_ili9488_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_ili9488_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_ili9488_disp_on_off(esp_lcd_panel_t *panel, bool off);

esp_err_t esp_lcd_new_panel_ili9488(const esp_lcd_panel_io_handle_t io,
                                      const esp_lcd_panel_dev_config_t *panel_dev_config,
                                      esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = ESP_OK;
    ili9488_panel_t *ili9488 = NULL;

    ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");

    ili9488 = calloc(1, sizeof(ili9488_panel_t));
    ESP_GOTO_ON_FALSE(ili9488, ESP_ERR_NO_MEM, err, TAG, "no mem for ili9488 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    ili9488->io = io;
    ili9488->fb_bits_per_pixel = panel_dev_config->bits_per_pixel;
    ili9488->reset_gpio_num = panel_dev_config->reset_gpio_num;
    ili9488->reset_level = panel_dev_config->flags.reset_active_high;
    ili9488->madctl_val = 0;

    // Set color format: 16-bit RGB565
    ili9488->colmod_val = 0x55;

    ili9488->base.del = panel_ili9488_del;
    ili9488->base.reset = panel_ili9488_reset;
    ili9488->base.init = panel_ili9488_init;
    ili9488->base.draw_bitmap = panel_ili9488_draw_bitmap;
    ili9488->base.invert_color = panel_ili9488_invert_color;
    ili9488->base.set_gap = panel_ili9488_set_gap;
    ili9488->base.mirror = panel_ili9488_mirror;
    ili9488->base.swap_xy = panel_ili9488_swap_xy;
    ili9488->base.disp_on_off = panel_ili9488_disp_on_off;

    *ret_panel = &(ili9488->base);
    ESP_LOGD(TAG, "new ili9488 panel @%p", ili9488);

    return ESP_OK;

err:
    if (ili9488) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(ili9488);
    }
    return ret;
}

static esp_err_t panel_ili9488_del(esp_lcd_panel_t *panel)
{
    ili9488_panel_t *ili9488 = __containerof(panel, ili9488_panel_t, base);

    if (ili9488->reset_gpio_num >= 0) {
        gpio_reset_pin(ili9488->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del ili9488 panel @%p", ili9488);
    free(ili9488);
    return ESP_OK;
}

static esp_err_t panel_ili9488_reset(esp_lcd_panel_t *panel)
{
    ili9488_panel_t *ili9488 = __containerof(panel, ili9488_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9488->io;

    if (ili9488->reset_gpio_num >= 0) {
        gpio_set_level(ili9488->reset_gpio_num, ili9488->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(ili9488->reset_gpio_num, !ili9488->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
    } else {
        esp_lcd_panel_io_tx_param(io, ILI9488_CMD_SWRESET, NULL, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    return ESP_OK;
}

static esp_err_t panel_ili9488_init(esp_lcd_panel_t *panel)
{
    ili9488_panel_t *ili9488 = __containerof(panel, ili9488_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9488->io;

    // Exit sleep mode
    esp_lcd_panel_io_tx_param(io, ILI9488_CMD_SLPOUT, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120));

    // Pixel Format Set: 0x66 = 18-bit/pixel (RGB666)
    esp_lcd_panel_io_tx_param(io, ILI9488_CMD_COLMOD, (uint8_t[]){0x66}, 1);

    // Memory Access Control:
    // MY=0, MX=0, MV=0, ML=0, BGR=0, MH=0
    // RGB mode (BGR bit disabled) - display panel expects RGB order
    // Mirror will be set via esp_lcd_panel_mirror() call
    esp_lcd_panel_io_tx_param(io, ILI9488_CMD_MADCTL, (uint8_t[]){0x00}, 1);    // Positive Gamma Control
    esp_lcd_panel_io_tx_param(io, 0xE0, (uint8_t[]){
        0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78,
        0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F
    }, 15);

    // Negative Gamma Control
    esp_lcd_panel_io_tx_param(io, 0xE1, (uint8_t[]){
        0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45,
        0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F
    }, 15);

    // Power Control 1
    esp_lcd_panel_io_tx_param(io, 0xC0, (uint8_t[]){0x17, 0x15}, 2);

    // Power Control 2
    esp_lcd_panel_io_tx_param(io, 0xC1, (uint8_t[]){0x41}, 1);

    // VCOM Control
    esp_lcd_panel_io_tx_param(io, 0xC5, (uint8_t[]){0x00, 0x12, 0x80}, 3);

    // Display Inversion OFF
    esp_lcd_panel_io_tx_param(io, ILI9488_CMD_INVOFF, NULL, 0);

    // Normal Display Mode ON
    esp_lcd_panel_io_tx_param(io, 0x13, NULL, 0);

    // Display ON
    esp_lcd_panel_io_tx_param(io, ILI9488_CMD_DISPON, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "ILI9488 initialized: RGB565, MADCTL=0x48");
    return ESP_OK;
}static esp_err_t panel_ili9488_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    ili9488_panel_t *ili9488 = __containerof(panel, ili9488_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9488->io;

    x_start += ili9488->x_gap;
    x_end   += ili9488->x_gap;
    y_start += ili9488->y_gap;
    y_end   += ili9488->y_gap;

    // Column Address Set
    esp_lcd_panel_io_tx_param(io, ILI9488_CMD_CASET, (uint8_t[]) {
        (x_start >> 8) & 0xFF,
        x_start & 0xFF,
        ((x_end - 1) >> 8) & 0xFF,
        (x_end - 1) & 0xFF,
    }, 4);

    // Page Address Set
    esp_lcd_panel_io_tx_param(io, ILI9488_CMD_PASET, (uint8_t[]) {
        (y_start >> 8) & 0xFF,
        y_start & 0xFF,
        ((y_end - 1) >> 8) & 0xFF,
        (y_end - 1) & 0xFF,
    }, 4);

    // Memory Write
    size_t len = (size_t)(x_end - x_start) * (size_t)(y_end - y_start) * ili9488->fb_bits_per_pixel / 8;
    esp_lcd_panel_io_tx_color(io, ILI9488_CMD_RAMWR, color_data, len);

    return ESP_OK;
}

static esp_err_t panel_ili9488_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    ili9488_panel_t *ili9488 = __containerof(panel, ili9488_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9488->io;
    int command = invert_color_data ? ILI9488_CMD_INVON : ILI9488_CMD_INVOFF;
    esp_lcd_panel_io_tx_param(io, command, NULL, 0);
    return ESP_OK;
}

static esp_err_t panel_ili9488_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    ili9488_panel_t *ili9488 = __containerof(panel, ili9488_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9488->io;

    if (mirror_x) {
        ili9488->madctl_val |= 0x40;
    } else {
        ili9488->madctl_val &= ~0x40;
    }
    if (mirror_y) {
        ili9488->madctl_val |= 0x80;
    } else {
        ili9488->madctl_val &= ~0x80;
    }

    esp_lcd_panel_io_tx_param(io, ILI9488_CMD_MADCTL, (uint8_t[]){ili9488->madctl_val}, 1);
    return ESP_OK;
}

static esp_err_t panel_ili9488_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    ili9488_panel_t *ili9488 = __containerof(panel, ili9488_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9488->io;

    if (swap_axes) {
        ili9488->madctl_val |= 0x20;
    } else {
        ili9488->madctl_val &= ~0x20;
    }

    esp_lcd_panel_io_tx_param(io, ILI9488_CMD_MADCTL, (uint8_t[]){ili9488->madctl_val}, 1);
    return ESP_OK;
}

static esp_err_t panel_ili9488_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    ili9488_panel_t *ili9488 = __containerof(panel, ili9488_panel_t, base);
    ili9488->x_gap = x_gap;
    ili9488->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_ili9488_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    ili9488_panel_t *ili9488 = __containerof(panel, ili9488_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9488->io;
    int command = on_off ? ILI9488_CMD_DISPON : ILI9488_CMD_DISPOFF;
    esp_lcd_panel_io_tx_param(io, command, NULL, 0);
    return ESP_OK;
}
