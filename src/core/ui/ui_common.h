#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ui_common.h - Shared UI helpers used across all screens
 */

/* Screen width/height constants matching landscape config */
#define UI_SCREEN_W  480
#define UI_SCREEN_H  320

/* Sidebar / pane widths */
#define UI_SIDEBAR_W    80
#define UI_CURRENT_W   175
#define UI_CONTROLS_W  (UI_SCREEN_W - UI_SIDEBAR_W - UI_CURRENT_W)  /* 225 */

/* Standard padding */
#define UI_PAD          8
#define UI_PAD_SM       4

/* Standard header height for settings screens */
#define UI_HEADER_H    40

/*
 * Create a styled settings screen header bar.
 * Returns the header container.
 * Adds a back button that calls `back_cb` on click.
 * Adds a title label.
 */
lv_obj_t *ui_common_header(lv_obj_t *parent, const char *title, lv_event_cb_t back_cb, void *back_user_data);

/*
 * Create a full-width styled button.
 */
lv_obj_t *ui_common_btn(lv_obj_t *parent, const char *label, lv_event_cb_t cb, void *user_data);

/*
 * Create a full-width styled "danger" button (red).
 */
lv_obj_t *ui_common_btn_danger(lv_obj_t *parent, const char *label, lv_event_cb_t cb, void *user_data);

/*
 * Create a section label (grey, 14pt, uppercase).
 */
lv_obj_t *ui_common_section_label(lv_obj_t *parent, const char *text);

/*
 * Create a horizontal divider line.
 */
lv_obj_t *ui_common_divider(lv_obj_t *parent);

/*
 * Create a row container with label on left, widget slot on right.
 * Returns the container (parent for adding right-side widget).
 */
lv_obj_t *ui_common_row(lv_obj_t *parent, const char *label_text);

/*
 * Create a badge label (small coloured pill) for domain status.
 * color: background hex colour.
 */
lv_obj_t *ui_common_badge(lv_obj_t *parent, const char *text, uint32_t color);

/*
 * Update badge text and colour.
 */
void ui_common_badge_update(lv_obj_t *badge, const char *text, uint32_t color);

/*
 * Create a toggle (lv_switch) with a label in a row.
 * Returns the lv_switch object.
 */
lv_obj_t *ui_common_toggle_row(lv_obj_t *parent, const char *label_text,
                                lv_event_cb_t cb, void *user_data);

/* Number of visual segments used by WiFi indicator widgets. */
#define UI_WIFI_SEGMENT_COUNT 3

/*
 * Create a smartphone-style WiFi indicator ("pizza" arcs + dot).
 * The returned object is a container; call ui_common_wifi_indicator_update()
 * to set connection state and RSSI.
 */
lv_obj_t *ui_common_wifi_indicator_create(lv_obj_t *parent, int width, int height);

/*
 * Update WiFi indicator segments/colors from connection state and RSSI.
 */
void ui_common_wifi_indicator_update(lv_obj_t *indicator, bool connected, int rssi);

/*
 * Push a screen (creates transition).
 * Stores previous screen for back navigation.
 */
void ui_common_push_screen(lv_obj_t *new_scr);

/*
 * Pop back to previous screen (deletes current).
 */
void ui_common_pop_screen(void);

/*
 * Return the main application screen (never deleted).
 */
lv_obj_t *ui_common_get_main_screen(void);

/*
 * Register the main screen (called once by ui_main).
 */
void ui_common_set_main_screen(lv_obj_t *scr);

#ifdef __cplusplus
}
#endif
