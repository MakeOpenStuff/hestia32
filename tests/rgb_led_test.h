#ifndef RGB_LED_TEST_H
#define RGB_LED_TEST_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// WS2812 RGB LED GPIO
#define RGB_LED_GPIO 27

/**
 * @brief Initialize RGB LED test
 * WS2812 addressable LED on single GPIO
 * @return ESP_OK on success
 */
esp_err_t rgb_led_test_init(void);

/**
 * @brief Run RGB LED test
 * Cycles through colors to identify LED type and pins
 */
void rgb_led_test_run(void);

#ifdef __cplusplus
}
#endif

#endif // RGB_LED_TEST_H
