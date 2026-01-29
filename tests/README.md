# Tests Directory

This directory contains test and diagnostic utilities for hardware validation.

## Available Tests

### Power Consumption Test
**Files:** `power_test.c`, `power_test.h`
- Tests 4 SSR relays (GPIO 0, 1, 3, 26)
- Reads BME280 sensor (I2C on GPIO 4, 5)
- Cycles through relay states with measurements

### RGB LED Test
**Files:** `rgb_led_test.c`, `rgb_led_test.h`
- Auto-detects LED type (addressable WS2812 vs separate RGB)
- Tests GPIO 27 for addressable LED
- Falls back to GPIOs 27, 25, 8 for separate R/G/B
- Cycles through colors to identify configuration

### Thermostat Test
**Files:** `test_thermostat.c`
- Unit tests for thermostat control logic

## Enabling Tests

To enable a specific test, follow these steps:

1. **Uncomment the test in `src/CMakeLists.txt`:**
   ```cmake
   set(test_sources
       # ${CMAKE_SOURCE_DIR}/tests/power_test.c
       # ${CMAKE_SOURCE_DIR}/tests/rgb_led_test.c
   )
   ```

2. **Include the test header in `src/main.c`:**
   ```c
   #include "../tests/power_test.h"
   // or
   #include "../tests/rgb_led_test.h"
   ```

3. **Call the test initialization and run functions in `app_main()`:**
   ```c
   // For power test
   power_test_init();
   power_test_run();  // Runs forever

   // For RGB LED test
   rgb_led_test_init();
   rgb_led_test_run();  // Runs forever
   ```

4. **Important:** Most tests run in infinite loops, so place them at the end of `app_main()` after all other initialization.

## Notes
- Tests are designed to run standalone (one at a time)
- Most tests run in infinite loops for continuous validation
- Check serial monitor output for test results
