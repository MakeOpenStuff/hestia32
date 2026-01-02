#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the display with esp_lcd and LVGL
 *
 * @return ESP_OK on success
 */
esp_err_t display_init(void);

/**
 * @brief Create the UI using LVGL widgets
 */
void display_create_ui(void);

/**
 * @brief Update LVGL (call periodically from main loop)
 */
void display_update(void);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_MANAGER_H
