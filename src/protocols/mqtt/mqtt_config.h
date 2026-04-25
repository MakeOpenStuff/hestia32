#ifndef MQTT_CONFIG_H
#define MQTT_CONFIG_H

// WiFi Provisioning AP Configuration
// When device is not provisioned, it will create an AP with these credentials
#define PROV_AP_SSID "HESTIA32"
#define PROV_AP_PASSWORD ""  // Empty for open network

// WiFi Configuration (fallback - used only if not using provisioning)
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define WIFI_MAX_RETRY 5

// OTA Configuration (default - can be changed via provisioning)
#define OTA_SERVER_URL "https://your-server.com/firmware.bin"
#define OTA_CHECK_INTERVAL_MS 300000  // Check for updates every 5 minutes

#endif // MQTT_CONFIG_H
