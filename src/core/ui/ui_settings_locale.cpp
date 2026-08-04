#include "ui/ui_settings_locale.h"
#include "ui/ui_common.h"
#include "ui/ui_main.h"
#include "ui/ui_theme.h"
#include "ui/ui_icons.h"
#include "core/user_settings.h"
#include "lvgl.h"
#include "esp_log.h"
#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include <string.h>

static const char *TAG = "ui_locale";

#ifndef HESTIA_ENABLE_MANUAL_TIME_ENTRY
#define HESTIA_ENABLE_MANUAL_TIME_ENTRY 0
#endif

static time_settings_t s_ts;
static lv_obj_t *s_temp_btn_c       = NULL;
static lv_obj_t *s_temp_btn_f       = NULL;
static lv_obj_t *s_temp_lbl_c       = NULL;
static lv_obj_t *s_temp_lbl_f       = NULL;
static lv_obj_t *s_ntp_sw           = NULL;
static lv_obj_t *s_manual_section   = NULL;
static lv_obj_t *s_sb_hour          = NULL;
static lv_obj_t *s_sb_min           = NULL;
static lv_obj_t *s_sb_day           = NULL;
static lv_obj_t *s_sb_mon           = NULL;
static lv_obj_t *s_sb_year          = NULL;
static bool s_manual_controls_created = false;
static struct tm s_manual_seed_tm;

static void on_back(lv_event_t *e)
{
    (void)e;
    ui_common_pop_screen();
}

static void style_temp_unit_button(lv_obj_t *btn, const hestia_theme_t *t, bool selected)
{
    if (!btn || !t) {
        return;
    }

    lv_obj_set_size(btn, 64, 34);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(t->surface_variant), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, selected ? 2 : 1, 0);
    lv_obj_set_style_border_color(btn,
                                  selected ? lv_color_hex(t->primary) : lv_color_hex(t->border),
                                  0);
}

static void set_temp_unit_visual_state(temp_unit_t unit)
{
    const hestia_theme_t *t = ui_theme_get();
    bool c_selected = (unit == TEMP_UNIT_CELSIUS);

    style_temp_unit_button(s_temp_btn_c, t, c_selected);
    style_temp_unit_button(s_temp_btn_f, t, !c_selected);
}

static void on_temp_unit_select(lv_event_t *e)
{
    temp_unit_t new_unit = (temp_unit_t)(intptr_t)lv_event_get_user_data(e);
    esp_err_t err = user_settings_set_temp_unit(new_unit);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Temperature unit changed to %s",
                 new_unit == TEMP_UNIT_FAHRENHEIT ? "Fahrenheit" : "Celsius");
        ui_main_refresh_temp_unit();
        set_temp_unit_visual_state(new_unit);
    } else {
        ESP_LOGE(TAG, "Failed to save temperature unit: %d", err);
    }
}

#if HESTIA_ENABLE_MANUAL_TIME_ENTRY
static void cb_sb_inc(lv_event_t *e)
{
    lv_obj_t *sb = (lv_obj_t *)lv_event_get_user_data(e);
    if (sb) {
        lv_spinbox_increment(sb);
    }
}

static void cb_sb_dec(lv_event_t *e)
{
    lv_obj_t *sb = (lv_obj_t *)lv_event_get_user_data(e);
    if (sb) {
        lv_spinbox_decrement(sb);
    }
}

static lv_obj_t *make_spinbox_group(lv_obj_t *row_parent, int min, int max, int init, int digits)
{
    if (!row_parent) {
        ESP_LOGE(TAG, "make_spinbox_group: row_parent is NULL");
        return NULL;
    }

    const hestia_theme_t *t = ui_theme_get();

    const int sb_w   = (digits >= 4) ? 56 : 44;
    const int btn_w  = 26;
    const int cont_w = btn_w + sb_w + btn_w;

    lv_obj_t *cont = lv_obj_create(row_parent);
    if (!cont) {
        ESP_LOGE(TAG, "make_spinbox_group: failed to create container");
        return NULL;
    }
    lv_obj_set_size(cont, cont_w, 36);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *btn_dec = lv_btn_create(cont);
    if (!btn_dec) {
        ESP_LOGE(TAG, "make_spinbox_group: failed to create decrement button");
        lv_obj_del(cont);
        return NULL;
    }
    lv_obj_set_size(btn_dec, btn_w, 36);
    lv_obj_align(btn_dec, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_dec, lv_color_hex(t->surface_variant), 0);
    lv_obj_set_style_shadow_width(btn_dec, 0, 0);
    lv_obj_set_style_radius(btn_dec, 4, 0);
    lv_obj_set_style_pad_all(btn_dec, 0, 0);

    lv_obj_t *dl = lv_label_create(btn_dec);
    if (!dl) {
        ESP_LOGE(TAG, "make_spinbox_group: failed to create decrement label");
        lv_obj_del(cont);
        return NULL;
    }
    lv_label_set_text(dl, LV_SYMBOL_MINUS);
    lv_obj_center(dl);

    lv_obj_t *sb = lv_spinbox_create(cont);
    if (!sb) {
        ESP_LOGE(TAG, "make_spinbox_group: failed to create spinbox");
        lv_obj_del(cont);
        return NULL;
    }
    lv_spinbox_set_digit_format(sb, digits, 0);
    lv_spinbox_set_range(sb, min, max);
    lv_spinbox_set_value(sb, init);
    lv_spinbox_set_step(sb, 1);
    lv_obj_set_size(sb, sb_w, 36);
    lv_obj_align(sb, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(sb, lv_color_hex(t->surface), 0);
    lv_obj_set_style_text_color(sb, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(sb, &lv_font_montserrat_14, 0);
    lv_obj_set_style_border_color(sb, lv_color_hex(t->border), 0);
    lv_obj_set_style_border_width(sb, 1, 0);
    lv_obj_set_style_radius(sb, 4, 0);
    lv_obj_set_style_pad_all(sb, 2, 0);

    lv_obj_t *btn_inc = lv_btn_create(cont);
    if (!btn_inc) {
        ESP_LOGE(TAG, "make_spinbox_group: failed to create increment button");
        lv_obj_del(cont);
        return NULL;
    }
    lv_obj_set_size(btn_inc, btn_w, 36);
    lv_obj_align(btn_inc, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_inc, lv_color_hex(t->surface_variant), 0);
    lv_obj_set_style_shadow_width(btn_inc, 0, 0);
    lv_obj_set_style_radius(btn_inc, 4, 0);
    lv_obj_set_style_pad_all(btn_inc, 0, 0);

    lv_obj_t *pl = lv_label_create(btn_inc);
    if (!pl) {
        ESP_LOGE(TAG, "make_spinbox_group: failed to create increment label");
        lv_obj_del(cont);
        return NULL;
    }
    lv_label_set_text(pl, LV_SYMBOL_PLUS);
    lv_obj_center(pl);

    lv_obj_add_event_cb(btn_dec, cb_sb_dec, LV_EVENT_CLICKED, sb);
    lv_obj_add_event_cb(btn_inc, cb_sb_inc, LV_EVENT_CLICKED, sb);

    return sb;
}
#endif

#if HESTIA_ENABLE_MANUAL_TIME_ENTRY
static bool create_manual_controls(lv_obj_t *manual_parent, const struct tm *seed_tm, const hestia_theme_t *t)
{
    if (!manual_parent || !seed_tm || !t) {
        ESP_LOGE(TAG, "create_manual_controls: invalid args");
        return false;
    }

    if (s_manual_controls_created) {
        return true;
    }

    ui_common_section_label(manual_parent, "Set Date & Time");

    lv_obj_t *time_row = lv_obj_create(manual_parent);
    if (!time_row) {
        ESP_LOGE(TAG, "Failed to create time row");
        return false;
    }
    lv_obj_set_width(time_row, LV_PCT(100));
    lv_obj_set_height(time_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(time_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(time_row, 0, 0);
    lv_obj_set_style_pad_all(time_row, 0, 0);
    lv_obj_set_flex_flow(time_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(time_row, 6, 0);

    s_sb_hour = make_spinbox_group(time_row, 0, 23, seed_tm->tm_hour, 2);

    lv_obj_t *colon = lv_label_create(time_row);
    if (colon) {
        lv_label_set_text(colon, ":");
        lv_obj_set_style_text_font(colon, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(colon, lv_color_hex(t->text), 0);
    }

    s_sb_min = make_spinbox_group(time_row, 0, 59, seed_tm->tm_min, 2);

    lv_obj_t *date_row = lv_obj_create(manual_parent);
    if (!date_row) {
        ESP_LOGE(TAG, "Failed to create date row");
        return false;
    }
    lv_obj_set_width(date_row, LV_PCT(100));
    lv_obj_set_height(date_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(date_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(date_row, 0, 0);
    lv_obj_set_style_pad_all(date_row, 0, 0);
    lv_obj_set_flex_flow(date_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(date_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(date_row, 6, 0);

    s_sb_day = make_spinbox_group(date_row, 1, 31, seed_tm->tm_mday, 2);

    lv_obj_t *slash1 = lv_label_create(date_row);
    if (slash1) {
        lv_label_set_text(slash1, "/");
        lv_obj_set_style_text_font(slash1, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(slash1, lv_color_hex(t->text), 0);
    }

    s_sb_mon = make_spinbox_group(date_row, 1, 12, seed_tm->tm_mon + 1, 2);

    lv_obj_t *slash2 = lv_label_create(date_row);
    if (slash2) {
        lv_label_set_text(slash2, "/");
        lv_obj_set_style_text_font(slash2, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(slash2, lv_color_hex(t->text), 0);
    }

    s_sb_year = make_spinbox_group(date_row, 2024, 2099, seed_tm->tm_year + 1900, 4);

    if (!s_sb_hour || !s_sb_min || !s_sb_day || !s_sb_mon || !s_sb_year) {
        ESP_LOGE(TAG, "Locale manual time controls incomplete");
        return false;
    }

    s_manual_controls_created = true;
    return true;
}
#endif

#if HESTIA_ENABLE_MANUAL_TIME_ENTRY
static void on_ntp_toggle(lv_event_t *e)
{
    (void)e;
    if (!s_manual_section || !s_ntp_sw) {
        return;
    }

#if !HESTIA_ENABLE_MANUAL_TIME_ENTRY
    lv_obj_add_flag(s_manual_section, LV_OBJ_FLAG_HIDDEN);
    return;
#endif

    bool ntp_on = lv_obj_has_state(s_ntp_sw, LV_STATE_CHECKED);
    if (ntp_on) {
        lv_obj_add_flag(s_manual_section, LV_OBJ_FLAG_HIDDEN);
    } else {
        if (!s_manual_controls_created) {
            const hestia_theme_t *t = ui_theme_get();
            if (!create_manual_controls(s_manual_section, &s_manual_seed_tm, t)) {
                ESP_LOGE(TAG, "Failed to create manual controls on-demand; forcing NTP ON");
                lv_obj_add_state(s_ntp_sw, LV_STATE_CHECKED);
                lv_obj_add_flag(s_manual_section, LV_OBJ_FLAG_HIDDEN);
                s_ts.use_ntp = 1;
                return;
            }
        }
        lv_obj_clear_flag(s_manual_section, LV_OBJ_FLAG_HIDDEN);
    }
}
#endif

void ui_settings_locale_open(void)
{
    s_temp_btn_c = NULL;
    s_temp_btn_f = NULL;
    s_temp_lbl_c = NULL;
    s_temp_lbl_f = NULL;
    s_ntp_sw = NULL;
    s_manual_section = NULL;
    s_sb_hour = NULL;
    s_sb_min = NULL;
    s_sb_day = NULL;
    s_sb_mon = NULL;
    s_sb_year = NULL;
    s_manual_controls_created = false;

    time_settings_init();
    time_settings_get(&s_ts);

    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    if (tm_now.tm_year < 100) {
        tm_now.tm_hour = 12;
        tm_now.tm_min = 0;
        tm_now.tm_mday = 1;
        tm_now.tm_mon = 0;
        tm_now.tm_year = 126;
    }
    s_manual_seed_tm = tm_now;

    const hestia_theme_t *t = ui_theme_get();

    lv_mem_monitor_t mon_before;
    lv_mem_monitor(&mon_before);
    ESP_LOGI(TAG, "Locale open: LVGL mem free=%u largest=%u frag=%u%%",
             (unsigned)mon_before.free_size,
             (unsigned)mon_before.free_biggest_size,
             (unsigned)mon_before.frag_pct);

    lv_obj_t *scr = lv_obj_create(NULL);
    if (!scr) {
        ESP_LOGE(TAG, "Failed to create Locale screen");
        return;
    }
    lv_obj_set_size(scr, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(scr, lv_color_hex(t->bg), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    ui_common_header(scr, "Locale", on_back, NULL);

    lv_obj_t *body = lv_obj_create(scr);
    if (!body) {
        ESP_LOGE(TAG, "Failed to create Locale body");
        lv_obj_del(scr);
        return;
    }
    lv_obj_set_size(body, UI_SCREEN_W, UI_SCREEN_H - UI_HEADER_H);
    lv_obj_align(body, LV_ALIGN_TOP_LEFT, 0, UI_HEADER_H);
    lv_obj_set_style_bg_color(body, lv_color_hex(t->bg), 0);
    lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_all(body, 16, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(body, 10, 0);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);

    lv_obj_t *temp_row = lv_obj_create(body);
    if (!temp_row) {
        ESP_LOGE(TAG, "Failed to create temperature unit row");
        lv_obj_del(scr);
        return;
    }
    lv_obj_set_size(temp_row, LV_PCT(100), 50);
    lv_obj_set_style_bg_color(temp_row, lv_color_hex(t->surface), 0);
    lv_obj_set_style_border_width(temp_row, 0, 0);
    lv_obj_set_style_radius(temp_row, 8, 0);
    lv_obj_clear_flag(temp_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *temp_title = lv_label_create(temp_row);
    lv_label_set_text(temp_title, "Temperature Unit");
    lv_obj_set_style_text_color(temp_title, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(temp_title, &lv_font_montserrat_16, 0);
    lv_obj_align(temp_title, LV_ALIGN_LEFT_MID, 16, 0);

    lv_obj_t *temp_controls = lv_obj_create(temp_row);
    if (!temp_controls) {
        ESP_LOGE(TAG, "Failed to create temperature control container");
        lv_obj_del(scr);
        return;
    }
    lv_obj_set_size(temp_controls, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(temp_controls, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(temp_controls, 0, 0);
    lv_obj_set_style_pad_all(temp_controls, 0, 0);
    lv_obj_set_style_pad_column(temp_controls, 8, 0);
    lv_obj_set_flex_flow(temp_controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(temp_controls, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(temp_controls, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(temp_controls, LV_ALIGN_RIGHT_MID, -12, 0);

    s_temp_btn_c = lv_btn_create(temp_controls);
    s_temp_btn_f = lv_btn_create(temp_controls);
    if (!s_temp_btn_c || !s_temp_btn_f) {
        ESP_LOGE(TAG, "Failed to create temperature unit buttons");
        lv_obj_del(scr);
        return;
    }

    s_temp_lbl_c = lv_label_create(s_temp_btn_c);
    s_temp_lbl_f = lv_label_create(s_temp_btn_f);
    if (!s_temp_lbl_c || !s_temp_lbl_f) {
        ESP_LOGE(TAG, "Failed to create temperature unit labels");
        lv_obj_del(scr);
        return;
    }
    lv_label_set_text(s_temp_lbl_c, "°C");
    lv_label_set_text(s_temp_lbl_f, "°F");
    lv_obj_set_style_text_color(s_temp_lbl_c, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_color(s_temp_lbl_f, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(s_temp_lbl_c, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_font(s_temp_lbl_f, &lv_font_montserrat_14, 0);
    lv_obj_center(s_temp_lbl_c);
    lv_obj_center(s_temp_lbl_f);

    lv_obj_add_event_cb(s_temp_btn_c, on_temp_unit_select, LV_EVENT_CLICKED,
                        (void *)(intptr_t)TEMP_UNIT_CELSIUS);
    lv_obj_add_event_cb(s_temp_btn_f, on_temp_unit_select, LV_EVENT_CLICKED,
                        (void *)(intptr_t)TEMP_UNIT_FAHRENHEIT);

    temp_unit_t current_unit = user_settings_get_temp_unit();
    set_temp_unit_visual_state(current_unit);

#if HESTIA_ENABLE_MANUAL_TIME_ENTRY
    s_manual_section = lv_obj_create(body);
    if (!s_manual_section) {
        ESP_LOGE(TAG, "Failed to create manual section");
        lv_obj_del(scr);
        return;
    }
    lv_obj_set_width(s_manual_section, LV_PCT(100));
    lv_obj_set_height(s_manual_section, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s_manual_section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_manual_section, 0, 0);
    lv_obj_set_style_pad_all(s_manual_section, 0, 0);
    lv_obj_set_flex_flow(s_manual_section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_manual_section, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(s_manual_section, 8, 0);

    s_ntp_sw = ui_common_toggle_row(body, "Internet Time (NTP)", NULL, NULL);
    if (!s_ntp_sw) {
        ESP_LOGE(TAG, "Failed to create NTP toggle row");
        lv_obj_del(scr);
        return;
    }

    if (s_ts.use_ntp) {
        lv_obj_add_state(s_ntp_sw, LV_STATE_CHECKED);
        lv_obj_add_flag(s_manual_section, LV_OBJ_FLAG_HIDDEN);
    } else {
        if (!create_manual_controls(s_manual_section, &s_manual_seed_tm, t)) {
            ESP_LOGE(TAG, "Locale open: failed to create manual controls; forcing NTP ON");
            lv_obj_add_state(s_ntp_sw, LV_STATE_CHECKED);
            lv_obj_add_flag(s_manual_section, LV_OBJ_FLAG_HIDDEN);
            s_ts.use_ntp = 1;
        } else {
            lv_obj_clear_flag(s_manual_section, LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_obj_add_event_cb(s_ntp_sw, on_ntp_toggle, LV_EVENT_VALUE_CHANGED, NULL);
#else
    s_manual_section = NULL;
    s_ntp_sw = NULL;
#endif

    lv_mem_monitor_t mon_after;
    lv_mem_monitor(&mon_after);
    ESP_LOGI(TAG, "Locale built: LVGL mem free=%u largest=%u frag=%u%%",
             (unsigned)mon_after.free_size,
             (unsigned)mon_after.free_biggest_size,
             (unsigned)mon_after.frag_pct);

    ui_common_push_screen(scr);
}
