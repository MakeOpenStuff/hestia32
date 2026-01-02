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
 * @brief Check if touch calibration exists in NVS
 *
 * @return true if calibration exists, false otherwise
 */
bool display_has_calibration(void);

/**
 * @brief Clear the display to black screen
 */
void display_clear_screen(void);

/**
 * @brief Create the UI using LVGL widgets
 * @param skip_calibration If true, skip calibration wizard even if not calibrated
 * @param provisioning_mode If true, show minimal provisioning UI instead of full UI
 */
void display_create_ui(bool skip_calibration, bool provisioning_mode);

/**
 * @brief Update LVGL (call periodically from main loop)
 */
void display_update(void);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_MANAGER_H
