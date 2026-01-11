# Hestia32 Firmware

ESP32 firmware with WiFi provisioning, OTA updates, and factory reset support.

## Supported Hardware

- **ESP32** (original) - e.g., NodeMCU-32S
- **ESP32-C5** - RISC-V based ESP32 variant

## Features

- WiFi provisioning via web interface (AP mode)
- BT and OTA (Over-The-Air) firmware updates
- Factory reset via BOOT button
- NVS storage for WiFi credentials and configuration
- Advanced thermostat control logic with multi-stage heating/cooling

## Prerequisites

### For ESP32 (PlatformIO)
- [PlatformIO](https://platformio.org/) installed
- USB drivers for your ESP32 board

### For ESP32-C5 (ESP-IDF v5.5)
- ESP-IDF v5.5.1 or later
- Python 3.8+
- CMake 3.16+
- Ninja build system

### For Testing (Native Host)
- GCC or Clang compiler with C11 support
- Make (optional, for convenient build commands)

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

## Testing

### Thermostat Logic Tests

The firmware includes a comprehensive test suite for the thermostat control logic that runs on your development machine (not on the ESP32).

#### Running Tests

```bash
# Quick test (using Makefile)
make test

# Or manually build and run
gcc -Wall -Wextra -std=c11 -I. src/thermostat.c tests/test_thermostat.c -o test_thermostat -lm && ./test_thermostat
```

#### Test Coverage

The test suite validates:

**Thermal Logic Isolation Tests (Non-HVAC):**
1. **Configuration validation** - Domain limits, setpoint sanity checks
2. **Hysteresis protection** - Prevents rapid state changes (300s delay)
3. **Comfort deadband** - Temperature activation thresholds (±0.5°C)
4. **ECO deadband** - Wider temperature band for energy savings (±1.0°C)
5. **Stage 2 heating** - Gradual power increase with delay + deviation logic
6. **Stage 2 cooling** - Gradual power increase with delay + deviation logic
7. **Mutual exclusion** - Heating and cooling never run simultaneously
8. **Sensor failure safety** - Automatic shutdown on sensor faults
9. **Temperature units** - Celsius and Fahrenheit conversion

**HVAC Integration Tests (Fan Coordination):**
10. **Fan control** - Pre-run (30s), post-run (60s), and manual override
11. **Humidity control** - Humidification and dehumidification with deadband
12. **Hot water demand** - On-demand hot water heating
13. **Fan timing** - Comprehensive pre-run and post-run validation
14. **Hysteresis + HVAC** - State change delays with fan coordination
15. **Comfort deadband + HVAC** - Temperature thresholds with fan safety
16. **ECO deadband + HVAC** - Energy-saving mode with fan coordination
17. **Stage 2 heating + HVAC** - Multi-stage heating with fan timing
18. **Stage 2 cooling + HVAC** - Multi-stage cooling with fan timing
19. **Mutual exclusion + HVAC** - No simultaneous operation through all fan phases
20. **Sensor failure + HVAC** - Immediate shutdown including fan (safety critical)

**Scenario Testing:**
- **24-hour simulation** - Complete heating cycle with realistic temperature changes

**Key Features Validated:**
- Fan pre-run ensures airflow before thermal elements activate (prevents dry running)
- Fan post-run distributes residual heat/cool after thermal stops
- Minimum cycle time (180s) protects equipment from short cycling
- Hysteresis period (300s) prevents rapid mode switching
- Stage 2 activation requires both time delay (300s) AND temperature deviation (0.75°C)
- All outputs shut down immediately on sensor failure (safety)
- Manual fan override works independently of thermal state

#### Test Output

Tests produce color-coded output (green = pass, red = fail) with:
- Related configuration for each test
- Step-by-step execution traces
- Final summary table with pass/fail counts

```
=== TEST 1: Configuration Validation ===
Testing: thermostat_config_validate() with various invalid configs
...
✓ PASSED: Configuration Validation

=== SUMMARY ===
Total Tests: 12
Passed: 12
Failed: 0
Success Rate: 100.00%
```

#### Continuous Testing

Run tests before committing changes to ensure thermostat logic correctness:


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


## Project Structure

```
hestia32-firmware/
├── src/
│   ├── main.c              # Main application
│   ├── wifi_manager.c      # WiFi connection management
│   ├── wifi_provisioning.c # Web-based WiFi provisioning
│   ├── ota_manager.c       # OTA update handling
│   ├── thermostat.c        # Thermostat control logic
│   ├── thermostat.h        # Thermostat API definitions
│   └── config.h            # Configuration constants
├── tests/
│   └── test_thermostat.c   # Comprehensive thermostat tests
├── platformio.ini          # PlatformIO configuration (ESP32)
├── CMakeLists.txt          # ESP-IDF build config (ESP32-C5)
├── sdkconfig.defaults      # ESP-IDF default settings (ESP32-C5)
├── Makefile                # Test build automation
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

### Test Failures

If thermostat tests fail:

1. Check that you have a C11-compatible compiler:
   ```bash
   gcc --version  # Should be 4.9 or later
   ```

2. Ensure all source files are present:
   ```bash
   ls src/thermostat.c src/thermostat.h tests/test_thermostat.c
   ```

3. Run with verbose output to see which test failed:
   ```bash
   ./build/test_thermostat
   ```

4. If colors don't display correctly, your terminal may not support ANSI codes

## License

This project is open source. See LICENSE file for details.
