#ifndef CORE_CONFIG_H
#define CORE_CONFIG_H

// Application Configuration
#define APP_VERSION "0.8.4"

// Reset button configuration
// Set to -1 to disable, or a GPIO number to enable factory reset on boot
// To reset: hold this GPIO LOW during boot (connect to GND)
#ifdef CONFIG_IDF_TARGET_ESP32C5
#define FACTORY_RESET_GPIO 28  // Use GPIO 28 (BOOT button on ESP32-C5)
#else
#define FACTORY_RESET_GPIO 0  // Use GPIO 0 (BOOT button on most ESP32 boards)
#endif

// OTA Internal RGB LED configuration
// WS2812 RGB LED for OTA status feedback (blue=downloading, green=success, red=error)
#define OTA_RGB_LED_GPIO 27

#endif // CORE_CONFIG_H
