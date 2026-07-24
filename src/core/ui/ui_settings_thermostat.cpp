#include "ui/ui_settings_thermostat.h"
#include "ui/ui_common.h"
#include "ui/ui_theme.h"
#include "core/user_settings.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "ui_therm";

/* Spinner widget: label with +/- buttons */
typedef struct {
    lv_obj_t *label;
    float      value;
    float      min, max, step;
    int        decimals;  /* decimal places to show */
} spinner_t;

static void spinner_update(spinner_t *sp)
{
    if (!sp->label) return;
    char buf[16];
    if (sp->decimals == 1)
        snprintf(buf, sizeof(buf), "%.1f", sp->value);
    else
        snprintf(buf, sizeof(buf), "%d", (int)sp->value);
    lv_label_set_text(sp->label, buf);
}

/* Thermostat settings state */
static thermostat_settings_t s_therm;

#define SPINNER_COUNT 4
static spinner_t s_spinners[SPINNER_COUNT];
enum { SP_ECO_H, SP_ECO_C, SP_S2H_MIN, SP_S2C_MIN };

static void save_settings(void)
{
    s_therm.eco_heat_offset    = s_spinners[SP_ECO_H].value;
    s_therm.eco_cool_offset    = s_spinners[SP_ECO_C].value;
    s_therm.s2_heat_delay_min  = (uint16_t)s_spinners[SP_S2H_MIN].value;
    s_therm.s2_cool_delay_min  = (uint16_t)s_spinners[SP_S2C_MIN].value;
    thermostat_settings_save(&s_therm);
}

static void on_minus(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    spinner_t *sp = &s_spinners[idx];
    sp->value -= sp->step;
    if (sp->value < sp->min) sp->value = sp->min;
    spinner_update(sp);
    save_settings();  // Save immediately
}

static void on_plus(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    spinner_t *sp = &s_spinners[idx];
    sp->value += sp->step;
    if (sp->value > sp->max) sp->value = sp->max;
    spinner_update(sp);
    save_settings();  // Save immediately
}

static lv_obj_t *make_spinner_row(lv_obj_t *parent, const char *title,
                                   int sp_idx, float init, float mn, float mx,
                                   float step, int decimals)
{
    const hestia_theme_t *t = ui_theme_get();
    s_spinners[sp_idx].value    = init;
    s_spinners[sp_idx].min      = mn;
    s_spinners[sp_idx].max      = mx;
    s_spinners[sp_idx].step     = step;
    s_spinners[sp_idx].decimals = decimals;

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), 44);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(row, lv_color_hex(t->border), LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, UI_PAD, 0);

    /* +/- buttons on right */
    lv_obj_t *plus_btn = lv_btn_create(row);
    lv_obj_set_size(plus_btn, 28, 28);
    lv_obj_set_style_bg_color(plus_btn, lv_color_hex(t->surface_variant), 0);
    lv_obj_set_style_shadow_width(plus_btn, 0, 0);
    lv_obj_set_style_radius(plus_btn, 4, 0);
    lv_obj_align(plus_btn, LV_ALIGN_RIGHT_MID, -UI_PAD, 0);
    lv_obj_add_event_cb(plus_btn, on_plus, LV_EVENT_CLICKED, (void *)(intptr_t)sp_idx);
    lv_obj_t *pl = lv_label_create(plus_btn);
    lv_label_set_text(pl, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_color(pl, lv_color_hex(t->text), 0);
    lv_obj_align(pl, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *val_lbl = lv_label_create(row);
    lv_obj_set_style_text_color(val_lbl, lv_color_hex(t->accent), 0);
    lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(val_lbl, LV_ALIGN_RIGHT_MID, -44, 0);
    s_spinners[sp_idx].label = val_lbl;
    spinner_update(&s_spinners[sp_idx]);

    lv_obj_t *minus_btn = lv_btn_create(row);
    lv_obj_set_size(minus_btn, 28, 28);
    lv_obj_set_style_bg_color(minus_btn, lv_color_hex(t->surface_variant), 0);
    lv_obj_set_style_shadow_width(minus_btn, 0, 0);
    lv_obj_set_style_radius(minus_btn, 4, 0);
    lv_obj_align(minus_btn, LV_ALIGN_RIGHT_MID, -80, 0);
    lv_obj_add_event_cb(minus_btn, on_minus, LV_EVENT_CLICKED, (void *)(intptr_t)sp_idx);
    lv_obj_t *ml = lv_label_create(minus_btn);
    lv_label_set_text(ml, LV_SYMBOL_MINUS);
    lv_obj_set_style_text_color(ml, lv_color_hex(t->text), 0);
    lv_obj_align(ml, LV_ALIGN_CENTER, 0, 0);

    return row;
}

static void on_back(lv_event_t *e) { (void)e; ui_common_pop_screen(); }

void ui_settings_thermostat_open(void)
{
    thermostat_settings_init();
    thermostat_settings_get(&s_therm);

    const hestia_theme_t *t = ui_theme_get();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(scr, lv_color_hex(t->bg), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    ui_common_header(scr, "Thermostat", on_back, NULL);

    lv_obj_t *body = lv_obj_create(scr);
    lv_obj_set_size(body, UI_SCREEN_W, UI_SCREEN_H - UI_HEADER_H);
    lv_obj_align(body, LV_ALIGN_TOP_LEFT, 0, UI_HEADER_H);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_hor(body, UI_PAD, 0);
    lv_obj_set_style_pad_ver(body, 0, 0);
    lv_obj_set_style_radius(body, 0, 0);
    lv_obj_set_layout(body, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);

    make_spinner_row(body, "Eco heat offset",     SP_ECO_H,   s_therm.eco_heat_offset,   0.5f,  5.0f, 0.5f, 1);
    make_spinner_row(body, "Eco cool offset",     SP_ECO_C,   s_therm.eco_cool_offset,   0.5f,  5.0f, 0.5f, 1);
    make_spinner_row(body, "Stage 2 heat (min)",  SP_S2H_MIN, (float)s_therm.s2_heat_delay_min, 5.0f, 60.0f, 1.0f, 0);
    make_spinner_row(body, "Stage 2 cool (min)",  SP_S2C_MIN, (float)s_therm.s2_cool_delay_min, 5.0f, 60.0f, 1.0f, 0);

    ui_common_push_screen(scr);
}
