#ifndef CONFIG_H
#define CONFIG_H

// WiFi Provisioning AP Configuration
// When device is not provisioned, it will create an AP with these credentials
#define PROV_AP_SSID "HESTIA32"
#define PROV_AP_PASSWORD ""  // Empty for open network

// Reset button configuration (optional)
// Set to -1 to disable, or a GPIO number to enable factory reset on boot
// To reset: hold this GPIO LOW during boot (connect to GND)
#ifdef CONFIG_IDF_TARGET_ESP32C5
#define FACTORY_RESET_GPIO 28  // Use GPIO 28 (BOOT button on ESP32-C5)
#else
#define FACTORY_RESET_GPIO 0  // Use GPIO 0 (BOOT button on most ESP32 boards)
#endif

// WiFi Configuration (fallback - used only if not using provisioning)
// #define WIFI_SSID "BOB"
// #define WIFI_PASSWORD "guest123."
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define WIFI_MAX_RETRY 5

// OTA Configuration (default - can be changed via provisioning)
#define OTA_SERVER_URL "https://your-server.com/firmware.bin"
#define OTA_CHECK_INTERVAL_MS 300000  // Check for updates every 5 minutes

// Application Configuration
#define APP_VERSION "1.0.0"
#define LOOP_DELAY_MS 10000  // 10 seconds between prints

#endif // CONFIG_H
