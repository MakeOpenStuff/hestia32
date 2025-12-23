# Hestia32 Firmware

ESP32 firmware with WiFi provisioning, OTA updates, and factory reset support.

## Supported Hardware

- **ESP32** (original) - e.g., NodeMCU-32S
- **ESP32-C5** - RISC-V based ESP32 variant

## Features

- WiFi provisioning via web interface (AP mode)
- OTA (Over-The-Air) firmware updates
- Factory reset via BOOT button
- NVS storage for WiFi credentials and configuration
- Configurable node name and OTA server URL

## Prerequisites

### For ESP32 (PlatformIO)
- [PlatformIO](https://platformio.org/) installed
- USB drivers for your ESP32 board

### For ESP32-C5 (ESP-IDF v5.5)
- ESP-IDF v5.5.1 or later
- Python 3.8+
- CMake 3.16+
- Ninja build system

## Building and Flashing

### ESP32 (NodeMCU-32S) - PlatformIO

```bash
# Build
pio run -e nodemcu-32s

# Upload
pio run -e nodemcu-32s -t upload

# Monitor
pio device monitor
```

### ESP32-C5 - Native ESP-IDF

#### First Time Setup

```bash
# Clone ESP-IDF v5.5.1 (one time only)
cd ~/esp
git clone --recursive --branch v5.5.1 https://github.com/espressif/esp-idf.git esp-idf-v5.5

# Install ESP-IDF tools for ESP32-C5 (one time only)
cd ~/esp/esp-idf-v5.5
./install.sh esp32c5
```

#### Build Commands

```bash
# Set up environment (required in each new terminal session)
source ~/esp/esp-idf-v5.5/export.sh

# Or use the provided script
./setup-c5.sh

# Configure target (first time only)
idf.py set-target esp32c5

# Build firmware
idf.py --preview build

# Erase flash (removes all data including WiFi credentials)
idf.py --preview -p /dev/ttyACM0 erase-flash

# Flash firmware
idf.py --preview -p /dev/ttyACM0 flash

# Monitor serial output
idf.py --preview -p /dev/ttyACM0 monitor

# Flash and monitor in one command
idf.py --preview -p /dev/ttyACM0 flash monitor
```

**Note:** Replace `/dev/ttyACM0` with your actual serial port (e.g., `/dev/ttyUSB0`, `COM3` on Windows, `/dev/cu.usbserial-*` on macOS).

## Usage

### Initial Setup (Provisioning)

1. Flash the firmware to your device
2. On first boot, the device creates a WiFi access point:
   - **SSID:** `HESTIA32`
   - **Password:** None (open network)
3. Connect to the `HESTIA32` WiFi network
4. Open a web browser and navigate to `http://192.168.4.1`
5. Enter your WiFi credentials and configuration:
   - WiFi SSID
   - WiFi Password
   - Node Name (optional)
   - OTA Server URL (optional)
6. Click Save - the device will restart and connect to your WiFi

### Factory Reset

To erase WiFi credentials and return to provisioning mode:

1. Power off the device
2. Press and hold the **BOOT** button
3. Power on the device (or press RESET while holding BOOT)
4. Keep holding the BOOT button for **3 seconds**
5. Release when you see "Factory reset confirmed!" in the serial monitor
6. The device will erase credentials and reboot into provisioning mode

**BOOT Button GPIO:**
- ESP32 (original): GPIO 0
- ESP32-C5: GPIO 28

## Configuration

Edit `src/config.h` to customize:

```c
#define PROV_AP_SSID "HESTIA32"          // Provisioning AP name
#define PROV_AP_PASSWORD ""               // AP password (empty = open)
#define OTA_SERVER_URL "https://..."      // Default OTA server
#define OTA_CHECK_INTERVAL_MS 300000      // OTA check interval (5 min)
#define APP_VERSION "1.0.0"               // Firmware version
```

## Project Structure

```
hestia32-firmware/
├── src/
│   ├── main.c              # Main application
│   ├── wifi_manager.c      # WiFi connection management
│   ├── wifi_provisioning.c # Web-based WiFi provisioning
│   ├── ota_manager.c       # OTA update handling
│   └── config.h            # Configuration constants
├── platformio.ini          # PlatformIO configuration (ESP32)
├── CMakeLists.txt          # ESP-IDF build config (ESP32-C5)
├── sdkconfig.defaults      # ESP-IDF default settings (ESP32-C5)
└── setup-c5.sh             # ESP32-C5 environment setup script
```

## Troubleshooting

### ESP32-C5 Build Issues

**Error: `idf.py: command not found`**
```bash
source ~/esp/esp-idf-v5.5/export.sh
```

**Error: WiFi/PHY undefined references**
- Make sure you're using ESP-IDF v5.5.1 or later
- ESP32-C5 support is incomplete in earlier versions

**Error: `--preview` flag required**
- ESP32-C5 is still in preview, always use `--preview` flag with `idf.py` commands

### Serial Port Not Found

```bash
# Linux - check available ports
ls /dev/tty*

# Add user to dialout group (Linux)
sudo usermod -a -G dialout $USER
# Log out and back in for changes to take effect

# macOS - check available ports
ls /dev/cu.*
```

### Device Stuck in Provisioning Mode

If the device won't connect to WiFi after provisioning:

1. Check serial monitor for error messages
2. Verify WiFi credentials are correct
3. Perform factory reset and re-provision
4. Check that your WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)

## License

This project is open source. See LICENSE file for details.
