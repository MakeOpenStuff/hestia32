#include "ui/ui_settings_display.h"
#include "ui/ui_common.h"
#include "ui/ui_theme.h"
#include "core/user_settings.h"
#include "core/display_manager.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "ui_disp_cfg";

static display_settings_t s_disp;
static lv_obj_t *s_bri_label      = NULL;
static lv_obj_t *s_sleep_bri_label = NULL;
static lv_obj_t *s_sleep_label    = NULL;
static lv_obj_t *s_sleep_bri_slider = NULL;
static uint8_t s_original_brightness = 0;  /* For restoring if not saved */

/* Convert brightness (10-100%) to LEDC duty (0-255) */
static void apply_brightness(uint8_t pct)
{
    uint32_t duty = (uint32_t)pct * 255 / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void on_bri_slider(lv_event_t *e)
{
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    int32_t raw = lv_slider_get_value(slider);
    /* Snap to 10% increments for easier resistive touch */
    int32_t val = ((raw + 5) / 10) * 10;
    if (val < 10) val = 10; /* Min brightness 10% */
    lv_slider_set_value(slider, val, LV_ANIM_OFF);
    s_disp.brightness = (uint8_t)val;
    if (s_bri_label) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", (int)val);
        lv_label_set_text(s_bri_label, buf);
    }
    apply_brightness((uint8_t)val);
}

static void on_sleep_bri_slider(lv_event_t *e)
{
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    int32_t raw = lv_slider_get_value(slider);
    /* Snap to 10% increments for easier resistive touch */
    int32_t val = ((raw + 5) / 10) * 10;
    if (val > 100) val = 100;
    lv_slider_set_value(slider, val, LV_ANIM_OFF);
    s_disp.sleep_bri = (uint8_t)val;
    if (s_sleep_bri_label) {
        char buf[20];
        if (val == 0) snprintf(buf, sizeof(buf), "Screen stays off");
        else snprintf(buf, sizeof(buf), "%d%%", (int)val);
        lv_label_set_text(s_sleep_bri_label, buf);
    }
}

static void on_sleep_slider(lv_event_t *e)
{
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    int32_t raw = lv_slider_get_value(slider);
    int32_t val = raw;

    /* Position 0 = Never sleeps, Positions 1-6 = 10s-60s in 10s increments */
    if (val == 0) {
        s_disp.sleep_timeout_sec = 0; /* 0 means never sleep */
    } else {
        s_disp.sleep_timeout_sec = (uint16_t)(val * 10); /* 1=10s, 2=20s, ..., 6=60s */
    }

    lv_slider_set_value(slider, val, LV_ANIM_OFF);

    if (s_sleep_label) {
        char buf[20];
        if (val == 0) snprintf(buf, sizeof(buf), "Screen never sleeps");
        else snprintf(buf, sizeof(buf), "%ds", (int)s_disp.sleep_timeout_sec);
        lv_label_set_text(s_sleep_label, buf);
    }

    /* Disable sleep brightness slider when screen never sleeps */
    if (s_sleep_bri_slider) {
        if (val == 0) {
            lv_obj_add_state(s_sleep_bri_slider, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(s_sleep_bri_slider, LV_STATE_DISABLED);
        }
    }
}

static void on_save(lv_event_t *e)
{
    (void)e;
    display_settings_save(&s_disp);
    display_reload_settings();  // Apply settings immediately
    ESP_LOGI(TAG, "Display settings saved and applied");
    ui_common_pop_screen();
}

static void on_back(lv_event_t *e)
{
    (void)e;
    /* Restore original brightness if user canceled without saving */
    apply_brightness(s_original_brightness);
    ui_common_pop_screen();
}

static lv_obj_t *make_slider_row(lv_obj_t *parent, const char *title,
                                  lv_obj_t **val_lbl_out,
                                  int32_t min, int32_t max, int32_t init,
                                  lv_event_cb_t cb)
{
    const hestia_theme_t *t = ui_theme_get();
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), 60);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(row, lv_color_hex(t->border), LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, UI_PAD, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *val = lv_label_create(row);
    lv_obj_set_style_text_color(val, lv_color_hex(t->accent), 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_14, 0);
    lv_obj_align(val, LV_ALIGN_TOP_RIGHT, 0, 0);
    if (val_lbl_out) *val_lbl_out = val;

    lv_obj_t *slider = lv_slider_create(row);
    lv_obj_set_size(slider, LV_PCT(100), 12);
    lv_obj_align(slider, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    /* Add horizontal padding to slider for easier edge selection */
    lv_obj_set_style_pad_left(slider, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_right(slider, 12, LV_PART_MAIN);
    lv_slider_set_range(slider, min, max);
    lv_slider_set_value(slider, init, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, lv_color_hex(t->surface_variant), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(t->primary), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(t->primary), LV_PART_KNOB);
    lv_obj_add_event_cb(slider, cb, LV_EVENT_VALUE_CHANGED, NULL);

    return row;
}

void ui_settings_display_open(void)
{
    display_settings_init();
    display_settings_get(&s_disp);

    /* Save original brightness for restoring if user cancels */
    s_original_brightness = s_disp.brightness;

    const hestia_theme_t *t = ui_theme_get();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(scr, lv_color_hex(t->bg), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    ui_common_header(scr, "Display", on_back, NULL);

    lv_obj_t *body = lv_obj_create(scr);
    lv_obj_set_size(body, UI_SCREEN_W, UI_SCREEN_H - UI_HEADER_H - 60);
    lv_obj_align(body, LV_ALIGN_TOP_LEFT, 0, UI_HEADER_H);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_hor(body, UI_PAD, 0);
    lv_obj_set_style_pad_ver(body, 0, 0);
    lv_obj_set_style_radius(body, 0, 0);
    lv_obj_set_layout(body, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);

    char val_buf[16];
    snprintf(val_buf, sizeof(val_buf), "%d%%", (int)s_disp.brightness);

    lv_obj_t *row1 = make_slider_row(body, "Active brightness",
                                      &s_bri_label,
                                      10, 100, s_disp.brightness,
                                      on_bri_slider);
    (void)row1;
    if (s_bri_label) lv_label_set_text(s_bri_label, val_buf);

    /* Timeout slider: 0 = never sleep, 1-6 = 10s-60s */
    uint16_t sleep_val;
    if (s_disp.sleep_timeout_sec == 0) {
        sleep_val = 0; /* Never sleep */
    } else {
        sleep_val = s_disp.sleep_timeout_sec / 10; /* Convert seconds to slider position */
        if (sleep_val > 6) sleep_val = 6; /* Max 60s */
        if (sleep_val < 1) sleep_val = 1; /* Min 10s */
    }

    lv_obj_t *row2 = make_slider_row(body, "Sleep timeout",
                                      &s_sleep_label,
                                      0, 6, (int32_t)sleep_val,
                                      on_sleep_slider);
    (void)row2;
    if (s_sleep_label) {
        if (s_disp.sleep_timeout_sec == 0) lv_label_set_text(s_sleep_label, "Screen never sleeps");
        else {
            snprintf(val_buf, sizeof(val_buf), "%ds", (int)s_disp.sleep_timeout_sec);
            lv_label_set_text(s_sleep_label, val_buf);
        }
    }

    lv_obj_t *row3 = make_slider_row(body, "Sleep brightness",
                                      &s_sleep_bri_label,
                                      0, 100, s_disp.sleep_bri,
                                      on_sleep_bri_slider);
    (void)row3;
    /* Store reference to slider's child (the actual slider object) */
    s_sleep_bri_slider = lv_obj_get_child(row3, -1);  /* Last child is the slider */

    /* Add disabled state styling to make it visually greyed out */
    if (s_sleep_bri_slider) {
        lv_obj_set_style_bg_color(s_sleep_bri_slider, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(s_sleep_bri_slider, lv_color_hex(0x606060), LV_PART_INDICATOR | LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(s_sleep_bri_slider, lv_color_hex(0x606060), LV_PART_KNOB | LV_STATE_DISABLED);
        lv_obj_set_style_bg_opa(s_sleep_bri_slider, LV_OPA_50, LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_bg_opa(s_sleep_bri_slider, LV_OPA_50, LV_PART_INDICATOR | LV_STATE_DISABLED);
        lv_obj_set_style_bg_opa(s_sleep_bri_slider, LV_OPA_50, LV_PART_KNOB | LV_STATE_DISABLED);
    }

    if (s_sleep_bri_label) {
        if (s_disp.sleep_bri == 0) lv_label_set_text(s_sleep_bri_label, "Screen stays off");
        else {
            snprintf(val_buf, sizeof(val_buf), "%d%%", (int)s_disp.sleep_bri);
            lv_label_set_text(s_sleep_bri_label, val_buf);
        }
    }

    /* Disable sleep brightness slider initially if timeout is 0 */
    if (s_sleep_bri_slider && s_disp.sleep_timeout_sec == 0) {
        lv_obj_add_state(s_sleep_bri_slider, LV_STATE_DISABLED);
    }

    lv_obj_t *save_area = lv_obj_create(scr);
    lv_obj_set_size(save_area, UI_SCREEN_W, 52);
    lv_obj_align(save_area, LV_ALIGN_BOTTOM_LEFT, 0, -10);
    lv_obj_set_style_bg_opa(save_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(save_area, 0, 0);
    lv_obj_set_style_pad_all(save_area, UI_PAD, 0);
    lv_obj_clear_flag(save_area, LV_OBJ_FLAG_SCROLLABLE);
    ui_common_btn(save_area, "SAVE", on_save, NULL);

    ui_common_push_screen(scr);
}
