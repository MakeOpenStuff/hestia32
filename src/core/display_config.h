#ifndef DISPLAY_CONFIG_H
#define DISPLAY_CONFIG_H

#include "sdkconfig.h"

// Display dimensions (landscape 480×320)
#define TFT_WIDTH  480
#define TFT_HEIGHT 320

// SPI and touch pin mapping
#ifdef CONFIG_BOARD_TYPE_XIAO
// XIAO ESP32-C5 pin map (Seeed wiki):
// D8=GPIO8 (SCK), D9=GPIO9 (MISO), D10=GPIO10 (MOSI)
// Control pins chosen from available XIAO pins and do NOT match DevKit wiring.
#define TFT_MOSI   10
#define TFT_SCLK   8
#define TFT_CS     7
#define TFT_DC     1
// Use a dedicated GPIO for panel reset on XIAO.
// GPIO25 (D2) is a plain GPIO and avoids BOOT/strap-adjacent GPIO0.
#define TFT_RST    25
// Default: control BL from GPIO0 (D1).
#define TFT_BL     0

#define TOUCH_CS   12
#define TOUCH_IRQ  11
#define TOUCH_MOSI TFT_MOSI
#define TOUCH_MISO 9
#define TOUCH_SCLK TFT_SCLK
#else
// ESP32-C5 DevKit wiring
#define TFT_MOSI   7
#define TFT_SCLK   6
#define TFT_CS     10
#define TFT_DC     8
#define TFT_RST    9
#define TFT_BL     13

#define TOUCH_CS   14
#define TOUCH_IRQ  15
#define TOUCH_MOSI TFT_MOSI
#define TOUCH_MISO 2  // Dedicated to touch, display won't use it
#define TOUCH_SCLK TFT_SCLK
#endif

// SPI Configuration
#define TFT_SPI_HOST    SPI2_HOST
#ifdef CONFIG_BOARD_TYPE_XIAO
#define LCD_PIXEL_CLOCK_HZ (8 * 1000 * 1000)
#else
#define LCD_PIXEL_CLOCK_HZ (20 * 1000 * 1000)
#endif
#define LCD_CMD_BITS    8
#define LCD_PARAM_BITS  8

// LVGL Configuration
#define LVGL_TICK_PERIOD_MS 2
#ifdef CONFIG_BOARD_TYPE_XIAO
#define LVGL_BUFFER_HEIGHT  20  // Reduced: WiFi provisioning leaves <40KB free; 20 rows keeps total display buffers under 68KB
#else
#define LVGL_BUFFER_HEIGHT  80  // Higher throughput on DevKit
#endif

#endif // DISPLAY_CONFIG_H
