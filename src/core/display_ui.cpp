#include "core/display_ui.h"
#include "core/ui/ui_main.h"
#include "core/ui/ui_theme.h"
#include "core/user_settings.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "display_ui";
static bool s_provisioning_skip = false;

static void on_skip_wifi(lv_event_t *e)
{
		ESP_LOGI(TAG, "Skip WiFi button pressed");
		s_provisioning_skip = true;
}

void display_ui_create_provisioning(lv_obj_t *scr)
{
		ESP_LOGI(TAG, "Creating provisioning UI with skip option");
		s_provisioning_skip = false;
		const hestia_theme_t *t = ui_theme_get();

		lv_obj_set_style_bg_color(scr, lv_color_hex(t->bg), 0);

		/* Title */
		lv_obj_t *title = lv_label_create(scr);
		lv_label_set_text(title, "WiFi Setup");
		lv_obj_set_style_text_color(title, lv_color_hex(t->text), 0);
		lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
		lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

		/* Subtitle */
		lv_obj_t *subtitle = lv_label_create(scr);
		lv_label_set_text(subtitle, "Optional - Configure WiFi network");
		lv_obj_set_style_text_color(subtitle, lv_color_hex(t->text_secondary), 0);
		lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_14, 0);
		lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 55);

		/* Instructions */
		lv_obj_t *label = lv_label_create(scr);
		lv_label_set_text(label, "1. Connect to WiFi: HESTIA32\n\n"
											 "2. Open browser: 192.168.4.1\n\n"
											 "3. Enter your WiFi credentials\n\n"
											 "Device will restart after setup");
		lv_obj_set_style_text_color(label, lv_color_hex(t->text), 0);
		lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
		lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
		lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

		/* Skip button */
		lv_obj_t *skip_btn = lv_btn_create(scr);
		lv_obj_set_size(skip_btn, 200, 50);
	lv_obj_set_style_bg_color(skip_btn, lv_color_hex(t->primary), 0);
	lv_obj_set_style_radius(skip_btn, 8, 0);
	lv_obj_set_style_shadow_width(skip_btn, 0, 0);
	lv_obj_align(skip_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
	lv_obj_add_event_cb(skip_btn, on_skip_wifi, LV_EVENT_CLICKED, NULL);

	lv_obj_t *skip_label = lv_label_create(skip_btn);
	lv_label_set_text(skip_label, "Setup WiFi later...");
	lv_obj_set_style_text_color(skip_label, lv_color_hex(t->on_primary), 0);
		lv_obj_set_style_text_font(skip_label, &lv_font_montserrat_16, 0);
	lv_obj_align(skip_label, LV_ALIGN_CENTER, 0, 0);
}

bool display_ui_provisioning_skip_pressed(void)
{
	return s_provisioning_skip;
}

/* ── Diagnostic test pattern ────────────────────────────────────────────
 * Draws 8 solid-colour horizontal bands (no text, no icons, no widgets).
 * Each band has hard top/bottom pixel edges. If ANY band edge shows a
 * 1-pixel staircase the issue is in the driver flush path, not in LVGL
 * widget rendering.  Bands are static — no timers, no redraws.
 * Re-enable ui_main_create(scr) below once the driver is confirmed clean.
 * ----------------------------------------------------------------------- */
#define TEST_PATTERN_ENABLED 0

#if TEST_PATTERN_ENABLED
static void create_test_pattern(lv_obj_t *scr)
{
    static const uint32_t colors[] = {
        0xFF0000, /* red    */
        0xFF8000, /* orange */
        0xFFFF00, /* yellow */
        0x00FF00, /* green  */
        0x00FFFF, /* cyan   */
        0x0000FF, /* blue   */
        0xFF00FF, /* magenta*/
        0xFFFFFF, /* white  */
    };
    const int n = sizeof(colors) / sizeof(colors[0]);
    const int h = 320 / n;   /* exact band height: 40px each */

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < n; i++) {
        lv_obj_t *band = lv_obj_create(scr);
        lv_obj_set_pos(band, 0, i * h);
        lv_obj_set_size(band, 480, h);
        lv_obj_set_style_bg_color(band, lv_color_hex(colors[i]), 0);
        lv_obj_set_style_bg_opa(band, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(band, 0, 0);
        lv_obj_set_style_pad_all(band, 0, 0);
        lv_obj_set_style_radius(band, 0, 0);
        lv_obj_clear_flag(band, LV_OBJ_FLAG_SCROLLABLE);
    }
}
#endif /* TEST_PATTERN_ENABLED */

void display_ui_create_main(lv_obj_t *scr)
{
#if TEST_PATTERN_ENABLED
    ESP_LOGI(TAG, "Creating diagnostic test pattern (TEST_PATTERN_ENABLED=1)");
    create_test_pattern(scr);
#else
    /* Load the saved theme before building any UI widgets so all inline
     * lv_obj_set_style_* calls use the correct colour palette. */
    ui_theme_apply(device_config_get_theme());
    ESP_LOGI(TAG, "Creating main UI (theme=%d)", (int)device_config_get_theme());
    ui_main_create(scr);

    /* Force LVGL to complete all layout calculations immediately to prevent
     * visual glitches during screen transitions (e.g., from provisioning) */
    lv_obj_update_layout(scr);
#endif
}

void display_ui_update_sensor(float temperature, float humidity, bool is_celsius)
{
#if !TEST_PATTERN_ENABLED
    ui_main_update_sensor(temperature, humidity, is_celsius);
#else
    (void)temperature; (void)humidity; (void)is_celsius;
#endif
}
