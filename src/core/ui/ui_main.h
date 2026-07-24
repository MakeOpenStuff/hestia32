#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ui_main.h - Main 3-pane screen for Hestia32
 *
 * Layout (landscape 480×320):
 *   [Sidebar 60px] | [Current conditions 175px] | [Controls 245px]
 *
 * The main screen is created once and never deleted.
 * Settings screens are layered on top and deleted on back.
 */

/* Domain indices (must match order in sidebar buttons) */
typedef enum {
    UI_DOMAIN_HEATING  = 0,
    UI_DOMAIN_COOLING  = 1,
    UI_DOMAIN_FAN      = 2,
    UI_DOMAIN_HUMIDITY = 3,
    UI_DOMAIN_HOTWATER = 4,
    UI_DOMAIN_COUNT    = 5,
} ui_domain_t;

/* Status dot state for sidebar indicator */
typedef enum {
    UI_STATUS_DISABLED = 0,  /* domain not configured (dot hidden) */
    UI_STATUS_IDLE,          /* domain configured, not running (grey dot) */
    UI_STATUS_RUNNING,       /* domain actively running (green pulsing dot) */
    UI_STATUS_BOOST,         /* boost active (orange dot) */
} ui_domain_status_t;

/*
 * Create the main screen on the given LVGL screen object.
 * Must be called from the LVGL task context.
 */
void ui_main_create(lv_obj_t *scr);

/*
 * Update sensor data shown in the current-conditions pane.
 * Thread-safe: stores into a cached struct; LVGL timer reads it.
 */
void ui_main_update_sensor(float temperature, float humidity, bool is_celsius);

/*
 * Refresh all temperature displays after temp unit preference change.
 * Converts cached sensor data and setpoints to the new unit and updates UI.
 * Must be called from LVGL task context.
 */
void ui_main_refresh_temp_unit(void);

/*
 * Update the status dot for a domain.
 * Must be called from LVGL task context (e.g. from a timer callback).
 */
void ui_main_set_domain_status(ui_domain_t domain, ui_domain_status_t status);

/*
 * Force-switch the active controls pane to the given domain.
 * Used when thermostat activates a domain while user views another.
 */
void ui_main_activate_domain(ui_domain_t domain);

/*
 * Return the LVGL screen object for the main screen (for navigation).
 */
lv_obj_t *ui_main_get_screen(void);

/*
 * Reload the sidebar to reflect updated domain configuration.
 * Must be called from LVGL task context after domain_mask changes.
 */
void ui_main_reload_sidebar(void);

#ifdef __cplusplus
}
#endif
