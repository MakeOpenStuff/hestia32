#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <stdbool.h>
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
 * @note This is now deprecated - LVGL runs in its own task after display_start_lvgl_task()
 */
void display_update(void);

/**
 * @brief Start the dedicated LVGL task for continuous UI updates
 * @note Call this after display_init() and display_create_ui() to start background updates
 */
void display_start_lvgl_task(void);

/**
 * @brief Set touch pointer visibility
 * @param visible true to show pointer, false to hide
 */
void display_set_touch_pointer_visible(bool visible);

/**
 * @brief Run touch calibration wizard
 * Clears the screen and runs the 5-point calibration sequence
 */
void display_recalibrate(void);

/**
 * @brief Suspend display task to free memory during OTA
 * Suspends LVGL rendering task to free up ~100KB for OTA operations
 */
void display_suspend(void);

/**
 * @brief Resume display task after OTA
 * Restores normal display operation after OTA is complete
 */
void display_resume(void);

/**
 * @brief Prepare display for OTA update (frees memory for download)
 * Full display deinitialization with RGB LED status feedback
 * Frees ~110KB of memory for OTA operations
 */
void display_ota_prepare(void);

/**
 * @brief Restore display after failed OTA update
 * @param success If true, OTA succeeded (will reboot). If false, restore display.
 * @note Only called on OTA failure - successful OTA reboots device
 */
void display_ota_restore(bool success);

/** * @brief Restore display after OTA check when no update is needed
 * Called when firmware is already up to date (not an error, just restore silently)
 */
void display_ota_restore_no_update(void);

/** * @brief Turn off OTA status LED after successful boot
 * Called by ota_init() on first boot after successful OTA update
 */
void display_ota_led_off(void);

/**
 * @brief Reset the backlight sleep timer (call from any user interaction).
 *        Wakes the display immediately if it was sleeping.
 */
void display_notify_activity(void);

/**
 * @brief Reload display settings from NVS
 * Called after settings are changed to apply them immediately without reboot
 */
void display_reload_settings(void);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_MANAGER_H
