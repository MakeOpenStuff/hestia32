#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the main application UI with tiles and animations
 *
 * @param scr The LVGL screen object to create UI on
 * @param touch_dot Pointer to touch dot object (will be created and assigned)
 * @param flush_count Pointer to flush counter for FPS display
 */
void display_ui_create_main(lv_obj_t *scr, lv_obj_t **touch_dot, volatile uint32_t *flush_count);

/**
 * @brief Create minimal provisioning UI showing WiFi setup instructions
 *
 * @param scr The LVGL screen object to create UI on
 */
void display_ui_create_provisioning(lv_obj_t *scr);

/**
 * @brief Update sensor data display on the UI
 *
 * @param temperature Temperature value
 * @param humidity Humidity value
 * @param is_celsius True for Celsius, false for Fahrenheit
 */
void display_ui_update_sensor(float temperature, float humidity, bool is_celsius);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_UI_H
