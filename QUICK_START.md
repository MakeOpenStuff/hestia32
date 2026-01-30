# ESP32 OTA Project - Quick Reference

## Project Structure (Simplified)

```
hestia32/
├── src/
│   ├── main.c                   # Main application entry point
│   ├── config.h                 # Provisioning AP & OTA configuration
│   ├── wifi_provisioning.c/h    # WiFi provisioning portal
│   ├── wifi_manager.c/h         # WiFi connection module
│   ├── ota_manager.c/h          # OTA update module
│   └── CMakeLists.txt           # ESP-IDF build config
├── platformio.ini               # PlatformIO configuration
├── partitions_two_ota.csv       # Dual OTA partition table
└── README.md                    # Full documentation

```

## What's New

### WiFi Provisioning Portal (Latest)
- **No hardcoded WiFi credentials** - Configure via web interface
- **Captive portal** - Access http://192.168.4.1 when connected to ESP32-Setup AP
- **NVS storage** - Credentials saved permanently
- **Extra settings** - Configure OTA server URL and device name
- **Factory reset** - Can reset and reconfigure anytime

### Previous Features
- **WiFi Manager Module**: Complete WiFi connection handling
- **OTA Manager Module**: Working HTTPS OTA implementation
- **Configuration Header**: Centralized settings
- **Clean Main Loop**: FreeRTOS-based, no watchdog issues
- **Simplified Documentation**: Focus on what's actually implemented

## Quick Start Commands

```bash
# NO CONFIGURATION NEEDED - Just flash and configure via web!

# Build and upload
pio run -t upload

# Monitor serial output to see provisioning instructions
pio device monitor
```

## First Time Setup (WiFi Provisioning)

1. **Flash the firmware** (see commands above)

2. **Device creates WiFi AP**:
   - SSID: `ESP32-Setup`
   - Password: `12345678`

3. **Connect to the AP** with your phone/laptop

4. **Open browser** and go to: `http://192.168.4.1`

5. **Fill in the form**:
   - WiFi SSID (your network)
   - WiFi Password
   - OTA Server URL (optional)
   - Node Name (optional)

6. **Click Save** - Device restarts and connects to your WiFi!

## Factory Reset / Reconfigure

```bash
# Option 1: Erase and reflash
pio run -t erase
pio run -t upload

# Option 2: Add reset button (future feature)
# Will trigger wifi_prov_reset() + esp_restart()
```

## Next Steps

1. **Flash firmware** - `pio run -t upload`
2. **Connect to ESP32-Setup WiFi** and configure at http://192.168.4.1
3. **Device connects to your WiFi** automatically
4. **Set up an OTA server** (see README.md)
5. **Test OTA updates** by incrementing version and uncommenting OTA check
6. **Add security** for production (HTTPS certificates, firmware signing)

## Key Features

- [x] **WiFi Provisioning Portal** - No hardcoded credentials!
- [x] **NVS Storage** - Settings persist across reboots
- [x] **Clean, modular code** structure
- [x] Proper error handling
- [x] WiFi auto-reconnect
- [x] OTA rollback protection
- [x] No watchdog timer issues
- [x] Ready for production (with security additions)
- [x] **Boost Feature** - Temporarily override heating, cooling, or hot water for a user-defined countdown. Only stage 1 is activated during boost. Each domain has its own adjustable timer.
## Using the Boost Feature

To activate Boost for heating, cooling, or hot water:

1. Use the UI or physical controls to select the domain (heating, cooling, hot water).
2. Set the desired countdown duration. The last-used duration is remembered per domain and can be adjusted.
3. During Boost, only stage 1 is activated for heating/cooling. After the countdown, normal operation resumes.

Refer to the README.md for more details.

## Configuration Structure

```c
// Stored in NVS, configurable via web portal:
typedef struct {
    char ssid[32];           // WiFi network name
    char password[64];       // WiFi password
    char server_url[128];    // OTA server URL (optional)
    char node_name[32];      // Device name (optional)
    bool provisioned;        // Provisioning status flag
} wifi_config_data_t;
```

## Documentation

- **README.md** - Full implementation guide and documentation
- **partitions_two_ota.csv** - Partition table for dual OTA slots

## Build Status

**Last Build**: Success
**RAM Usage**: 9.6% (31,504 / 327,680 bytes)
**Flash Usage**: 72.5% (759,917 / 1,048,576 bytes)

## Notes

- Uses ESP-IDF framework version 5.1.2
- Supports OTA updates via HTTPS
- Dual partition scheme allows safe rollback
- WiFi connection with retry logic
- Periodic OTA checks (configurable interval)
