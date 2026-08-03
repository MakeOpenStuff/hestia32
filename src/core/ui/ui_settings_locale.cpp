#include "ui/ui_settings_locale.h"
#include "ui/ui_common.h"
#include "ui/ui_main.h"
#include "ui/ui_theme.h"
#include "ui/ui_icons.h"
#include "core/user_settings.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "ui_locale";

static time_settings_t s_ts;
static lv_obj_t *s_temp_unit_switch = NULL;

static void on_back(lv_event_t *e)
{
    (void)e;
    ui_common_pop_screen();
}

static void on_temp_unit_toggle(lv_event_t *e)
{
    lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
    bool is_fahrenheit = lv_obj_has_state(sw, LV_STATE_CHECKED);

    temp_unit_t new_unit = is_fahrenheit ? TEMP_UNIT_FAHRENHEIT : TEMP_UNIT_CELSIUS;
    esp_err_t err = user_settings_set_temp_unit(new_unit);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Temperature unit changed to %s (switch checked=%d)",
                 is_fahrenheit ? "Fahrenheit" : "Celsius", is_fahrenheit);
        /* Trigger UI refresh for main screen temperature displays */
        ui_main_refresh_temp_unit();
        ESP_LOGI(TAG, "UI refresh callback completed");
    } else {
        ESP_LOGE(TAG, "Failed to save temperature unit: %d", err);
    }
}

static void on_save(lv_event_t *e)
{
    (void)e;
    time_settings_save(&s_ts);
    ui_common_pop_screen();
}

void ui_settings_locale_open(void)
{
    time_settings_init();
    time_settings_get(&s_ts);

    const hestia_theme_t *t = ui_theme_get();

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(scr, lv_color_hex(t->bg), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Header with title and back button */
    ui_common_header(scr, "Locale", on_back, NULL);

    /* Content area below header */
    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_set_size(content, UI_SCREEN_W, UI_SCREEN_H - UI_HEADER_H);
    lv_obj_align(content, LV_ALIGN_TOP_LEFT, 0, UI_HEADER_H);
    lv_obj_set_style_bg_color(content, lv_color_hex(t->bg), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 16, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(content, 12, 0);

    /* Temperature Unit Row - all in one line */
    lv_obj_t *temp_row = lv_obj_create(content);
    lv_obj_set_size(temp_row, LV_PCT(100), 50);
    lv_obj_set_style_bg_color(temp_row, lv_color_hex(t->surface), 0);
    lv_obj_set_style_border_width(temp_row, 0, 0);
    lv_obj_set_style_radius(temp_row, 8, 0);
    lv_obj_clear_flag(temp_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *temp_label = lv_label_create(temp_row);
    lv_label_set_text(temp_label, "Temperature Unit");
    lv_obj_set_style_text_color(temp_label, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_16, 0);
    lv_obj_align(temp_label, LV_ALIGN_LEFT_MID, 16, 0);

    /* F label - right of switch */
    lv_obj_t *f_label = lv_label_create(temp_row);
    lv_label_set_text(f_label, "°F");
    lv_obj_set_style_text_color(f_label, lv_color_hex(t->text), 0);
    lv_obj_align(f_label, LV_ALIGN_RIGHT_MID, -16, 0);

    /* Temperature toggle switch */
    s_temp_unit_switch = lv_switch_create(temp_row);
		lv_obj_remove_style(s_temp_unit_switch, NULL,
                    LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_align(s_temp_unit_switch, LV_ALIGN_RIGHT_MID, -56, 0);

    /* Style switch main background and indicator for visibility in both themes */
    /* Main background (track): use surface variant so it's visible */
    lv_obj_set_style_bg_color(s_temp_unit_switch, lv_color_hex(t->surface_variant), LV_PART_MAIN);
    /* Indicator (checked/F): primary color */
    //lv_obj_set_style_bg_color(s_temp_unit_switch, lv_color_hex(t->primary), LV_PART_INDICATOR | LV_STATE_CHECKED);
		lv_obj_set_style_bg_color(s_temp_unit_switch, lv_color_hex(t->primary), LV_PART_INDICATOR | LV_STATE_CHECKED);
    /* Indicator (unchecked/C): text secondary for dark theme visibility */
    //lv_obj_set_style_bg_color(s_temp_unit_switch, lv_color_hex(t->text_secondary), LV_PART_INDICATOR | LV_STATE_DEFAULT);
		lv_obj_set_style_bg_color(s_temp_unit_switch, lv_color_hex(t->primary), LV_PART_INDICATOR | LV_STATE_DEFAULT);
		lv_obj_set_style_bg_opa(s_temp_unit_switch, LV_OPA_COVER, LV_PART_INDICATOR);
    /* Knob: contrasting dark grey */
    lv_obj_set_style_bg_color(s_temp_unit_switch, lv_color_hex(0x404040), LV_PART_KNOB);

    /* C label - left of switch with more spacing */
    lv_obj_t *c_label = lv_label_create(temp_row);
    lv_label_set_text(c_label, "°C");
    lv_obj_set_style_text_color(c_label, lv_color_hex(t->text), 0);
    lv_obj_align(c_label, LV_ALIGN_RIGHT_MID, -120, 0);

    temp_unit_t current_unit = user_settings_get_temp_unit();
    if (current_unit == TEMP_UNIT_FAHRENHEIT) {
        lv_obj_add_state(s_temp_unit_switch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(s_temp_unit_switch, on_temp_unit_toggle, LV_EVENT_VALUE_CHANGED, NULL);

    /* NTP toggle row */
    lv_obj_t *ntp_row = lv_obj_create(content);
    lv_obj_set_size(ntp_row, LV_PCT(100), 50);
    lv_obj_set_style_bg_color(ntp_row, lv_color_hex(t->surface), 0);
    lv_obj_set_style_border_width(ntp_row, 0, 0);
    lv_obj_set_style_radius(ntp_row, 8, 0);
    lv_obj_clear_flag(ntp_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ntp_label = lv_label_create(ntp_row);
    lv_label_set_text(ntp_label, "Use NTP (Network Time)");
    lv_obj_set_style_text_color(ntp_label, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(ntp_label, &lv_font_montserrat_16, 0);
    lv_obj_align(ntp_label, LV_ALIGN_LEFT_MID, 16, 0);

    lv_obj_t *ntp_switch = lv_switch_create(ntp_row);
    lv_obj_align(ntp_switch, LV_ALIGN_RIGHT_MID, -16, 0);
    if (s_ts.use_ntp) {
        lv_obj_add_state(ntp_switch, LV_STATE_CHECKED);
    }

    /* Save button */
    lv_obj_t *save_btn = lv_btn_create(content);
    lv_obj_set_size(save_btn, 120, 45);
    lv_obj_align(save_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x2196F3), 0);
    lv_obj_add_event_cb(save_btn, on_save, LV_EVENT_CLICKED, NULL);

    lv_obj_t *save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Save");
    lv_obj_center(save_label);

    ui_common_push_screen(scr);
}
