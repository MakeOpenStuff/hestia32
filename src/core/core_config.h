#ifndef CORE_CONFIG_H
#define CORE_CONFIG_H

#include "sdkconfig.h"

// Application Configuration
#define APP_VERSION "0.8.4"

// Reset button configuration
// Set to -1 to disable, or a GPIO number to enable factory reset on boot
// To reset: hold this GPIO LOW for 5 seconds while firmware is running.
#ifdef CONFIG_BOARD_TYPE_XIAO
#define FACTORY_RESET_GPIO 28  // XIAO BOOT button (runtime long-press detection)
#elif CONFIG_IDF_TARGET_ESP32C5
#define FACTORY_RESET_GPIO 28  // Use GPIO 28 (BOOT button on ESP32-C5 DevKit)
#else
#define FACTORY_RESET_GPIO 0  // Use GPIO 0 (BOOT button on most ESP32 boards)
#endif

// I2C Configuration (for XIAO with TCA9555)
#ifdef CONFIG_BOARD_TYPE_XIAO
#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_SDA_IO       23  // GPIO 23 (D4 on XIAO)
#define I2C_MASTER_SCL_IO       24  // GPIO 24 (D5 on XIAO)
#define TCA9555_I2C_ADDR        0x20
#endif

// Relay Configuration
#ifdef CONFIG_BOARD_TYPE_XIAO
// XIAO: Relays controlled via TCA9555 I2C GPIO expander
#define RELAY_USE_TCA9555       1
#define RELAY_TCA9555_PIN_BASE  0  // Relays on TCA9555 pins 0-3
#else
// DevKit: Relays controlled via direct GPIO
#define RELAY_USE_TCA9555       0
#define RELAY_GPIO_1            0   // Relay 1
#define RELAY_GPIO_2            1   // Relay 2
#define RELAY_GPIO_3            3   // Relay 3
#define RELAY_GPIO_4            26  // Relay 4
#endif

#define RELAY_COUNT             4

// OTA Internal RGB LED configuration
// WS2812 RGB LED for OTA status feedback (blue=downloading, green=success, red=error)
#define OTA_RGB_LED_GPIO 27

#endif // CORE_CONFIG_H
