# Tests Directory

This directory contains test and diagnostic utilities for hardware validation.

## Available Tests
These tests can be compiled into the main firmware and called from `app_main()`.

### Power Consumption Test
**Files:** `power_test.c`, `power_test.h`
Needed by power profiler
- Tests all relay outputs
- Reads SHT45 sensor (I2C on GPIO 4 SCL, GPIO 5 SDA)
- Cycles through relay states

### RGB LED Test
**Files:** `rgb_led_test.c`, `rgb_led_test.h`
- Auto-detects LED type (addressable WS2812 vs separate RGB)
- Tests GPIO 27 for addressable LED
- Falls back to GPIOs 27, 25, 8 for separate R/G/B
- Cycles through colors to identify configuration

### Sensor Test
**Files:** `sensor_test.c`, `sensor_test.h`
- Reads SHT45 temperature and humidity
- Respects user temperature unit preference (°C / °F)
- Supports one-shot and periodic task modes

### Thermostat Logic Test
**Files:** `test_thermostat.c`
- Unit tests for thermostat control logic (no hardware required)

### I2C Scanner
**Files:** `i2c_scanner.c`, `i2c_scanner.h`
- Scans I2C bus addresses 0x03–0x77
- Labels known devices (SHT45 at 0x44, TCA9555 at 0x20)
- Probes SHT45 serial number and temperature/humidity measurement with CRC validation
- Probes TCA9555 input port register
- API: `void i2c_scanner_scan(int scl_pin, int sda_pin)`

## Standalone Test Projects

These are self-contained ESP-IDF projects that can be built and flashed independently without touching the main firmware.

### test_blink
**Path:** `test_blink/`
- Blinks the onboard LED on GPIO 27 at 2-second intervals
- Use to verify bare-minimum flash/boot on a fresh board

### test_i2c_scanner
**Path:** `test_i2c_scanner/`
- Standalone app that runs the I2C scanner in a loop every 3 seconds
- Configured for XIAO ESP32-C5 (SCL=GPIO24, SDA=GPIO23)
- Build and flash:
  ```bash
  cd tests/test_i2c_scanner
  source ~/esp/esp-idf-v5.5/export.sh
  idf.py set-target esp32c5
  idf.py build flash monitor
  ```

## Enabling Library Tests in Main Firmware

1. **Uncomment the test in `src/CMakeLists.txt`:**
   ```cmake
   set(test_sources
       # ${CMAKE_SOURCE_DIR}/tests/power_test.c
       # ${CMAKE_SOURCE_DIR}/tests/sensor_test.c
   )
   ```

2. **Include the test header in `src/main.c`:**
   ```c
   #include "../tests/power_test.h"
   #include "../tests/sensor_test.h"
   ```

3. **Call the test functions in `app_main()`:**
   ```c
   power_test_init();
   power_test_run();  // runs forever

   sensor_test_init();
   sensor_test_start_periodic_task(5000);  // every 5 seconds
   ```

4. **Important:** Most tests run in infinite loops or background tasks — place them at the end of `app_main()` after all other initialization.

## Notes
- Library tests are designed to be enabled one at a time
- Standalone test projects have their own `CMakeLists.txt` and `sdkconfig.defaults`
- Check serial monitor output for results
- Build outputs and generated `sdkconfig` files are excluded from version control via `.gitignore`
