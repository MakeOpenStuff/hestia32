#ifndef DISPLAY_CONFIG_H
#define DISPLAY_CONFIG_H

// Display dimensions
#define TFT_WIDTH  320
#define TFT_HEIGHT 480

// SPI Pin Configuration (avoiding GPIO 11/12 - UART0)
#define TFT_MOSI   7
#define TFT_SCLK   6
#define TFT_CS     10
#define TFT_DC     8
#define TFT_RST    9
#define TFT_BL     13

// Touch Pin Configuration
#define TOUCH_CS   14
#define TOUCH_IRQ  15
#define TOUCH_MOSI TFT_MOSI
#define TOUCH_MISO 2
#define TOUCH_SCLK TFT_SCLK

// SPI Configuration
#define TFT_SPI_HOST    SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ (20 * 1000 * 1000)
#define LCD_CMD_BITS    8
#define LCD_PARAM_BITS  8

// LVGL Configuration
#define LVGL_TICK_PERIOD_MS 2
#define LVGL_BUFFER_HEIGHT  80  // Larger buffers to improve throughput

#endif // DISPLAY_CONFIG_H
