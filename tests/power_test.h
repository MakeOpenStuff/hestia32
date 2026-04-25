#ifndef POWER_TEST_H
#define POWER_TEST_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Relay GPIO pins (adjacent for easy PCB routing on ESP32-C5 30-pin DevKit)
// Avoiding: GPIO 2(touch), 6-10(display), 11-12(UART0), 13(backlight), 14-15(touch), 28(BOOT)
// Available on 30-pin DevKit: GPIO 0, 1, 3, 4, 5 are free
#define RELAY1_GPIO 0
#define RELAY2_GPIO 1
#define RELAY3_GPIO 3
#define RELAY4_GPIO 26

// SHT45 I2C pins (using available GPIOs)
#define SHT45_I2C_SDA 4
#define SHT45_I2C_SCL 5
#define SHT45_I2C_ADDR 0x44  // SHT45 address

/**
 * @brief Initialize power consumption test (relays + SHT45)
 * @return ESP_OK on success
 */
esp_err_t power_test_init(void);

/**
 * @brief Run power consumption test loop
 * Cycles through relay states and reads SHT45 every second
 */
void power_test_run(void);

#ifdef __cplusplus
}
#endif

#endif // POWER_TEST_H
