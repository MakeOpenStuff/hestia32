#include "ui/ui_theme.h"
#include <string.h>

/* ---------- Built-in theme definitions ---------- */

const hestia_theme_t HESTIA_THEME_DARK_DEF = {
    .bg              = 0x0F1923,
    .surface         = 0x1E2D3D,
    .surface_variant = 0x253547,
    .border          = 0x2E4057,
    .text            = 0xE8EEF4,
    .text_secondary  = 0x8AA4BC,
    .text_disabled   = 0x4A6070,
    .primary         = 0x4FC3F7,
    .on_primary      = 0x0F1923,
    .accent          = 0x80DEEA,
    .heating_color   = 0xFF6B35,
    .cooling_color   = 0x4FC3F7,
    .fan_color       = 0xEF5350,  /* Same as danger */
    .humidity_color  = 0x81D4FA,
    .hot_water_color = 0xFFCC02,
    .boost_color     = 0xFF8F00,
    .inactive_color  = 0x37474F,
    .danger          = 0xEF5350,
    .success         = 0xEF5350,  /* Same as danger */
    .warning         = 0xFFA726,
};

const hestia_theme_t HESTIA_THEME_LIGHT_DEF = {
    .bg              = 0xF0F4F8,
    .surface         = 0xFFFFFF,
    .surface_variant = 0xE8EEF4,
    .border          = 0xCDD5DE,
    .text            = 0x1A2733,
    .text_secondary  = 0x4A6070,
    .text_disabled   = 0x8AA4BC,
    .primary         = 0x1565C0,
    .on_primary      = 0xFFFFFF,
    .accent          = 0x0288D1,
    .heating_color   = 0xE64A19,
    .cooling_color   = 0x0288D1,
    .fan_color       = 0xC62828,  /* Same as danger */
    .humidity_color  = 0x0277BD,
    .hot_water_color = 0xF9A825,
    .boost_color     = 0xEF6C00,
    .inactive_color  = 0xB0BEC5,
    .danger          = 0xC62828,
    .success         = 0xC62828,  /* Same as danger */
    .warning         = 0xE65100,
};

/* ---------- Active theme state ---------- */

static hestia_theme_t   s_active_theme;
static hestia_theme_id_t s_active_id = HESTIA_THEME_DARK;
static lv_style_t       s_styles[HESTIA_STYLE_COUNT];
static bool             s_initialized = false;

/* ---------- Internal helpers ---------- */

static void rebuild_styles(void)
{
    const hestia_theme_t *t = &s_active_theme;

    /* SCREEN */
    lv_style_reset(&s_styles[HESTIA_STYLE_SCREEN]);
    lv_style_set_bg_color(&s_styles[HESTIA_STYLE_SCREEN], lv_color_hex(t->bg));
    lv_style_set_bg_opa(&s_styles[HESTIA_STYLE_SCREEN], LV_OPA_COVER);

    /* CARD */
    lv_style_reset(&s_styles[HESTIA_STYLE_CARD]);
    lv_style_set_bg_color(&s_styles[HESTIA_STYLE_CARD], lv_color_hex(t->surface));
    lv_style_set_bg_opa(&s_styles[HESTIA_STYLE_CARD], LV_OPA_COVER);
    lv_style_set_border_color(&s_styles[HESTIA_STYLE_CARD], lv_color_hex(t->border));
    lv_style_set_border_width(&s_styles[HESTIA_STYLE_CARD], 1);
    lv_style_set_radius(&s_styles[HESTIA_STYLE_CARD], 8);
    lv_style_set_pad_all(&s_styles[HESTIA_STYLE_CARD], 8);

    /* BTN_PRIMARY */
    lv_style_reset(&s_styles[HESTIA_STYLE_BTN_PRIMARY]);
    lv_style_set_bg_color(&s_styles[HESTIA_STYLE_BTN_PRIMARY], lv_color_hex(t->primary));
    lv_style_set_bg_opa(&s_styles[HESTIA_STYLE_BTN_PRIMARY], LV_OPA_COVER);
    lv_style_set_text_color(&s_styles[HESTIA_STYLE_BTN_PRIMARY], lv_color_hex(t->on_primary));
    lv_style_set_radius(&s_styles[HESTIA_STYLE_BTN_PRIMARY], 6);
    lv_style_set_pad_ver(&s_styles[HESTIA_STYLE_BTN_PRIMARY], 8);
    lv_style_set_pad_hor(&s_styles[HESTIA_STYLE_BTN_PRIMARY], 12);

    /* BTN_SECONDARY */
    lv_style_reset(&s_styles[HESTIA_STYLE_BTN_SECONDARY]);
    lv_style_set_bg_color(&s_styles[HESTIA_STYLE_BTN_SECONDARY], lv_color_hex(t->surface_variant));
    lv_style_set_bg_opa(&s_styles[HESTIA_STYLE_BTN_SECONDARY], LV_OPA_COVER);
    lv_style_set_text_color(&s_styles[HESTIA_STYLE_BTN_SECONDARY], lv_color_hex(t->text));
    lv_style_set_border_color(&s_styles[HESTIA_STYLE_BTN_SECONDARY], lv_color_hex(t->border));
    lv_style_set_border_width(&s_styles[HESTIA_STYLE_BTN_SECONDARY], 1);
    lv_style_set_radius(&s_styles[HESTIA_STYLE_BTN_SECONDARY], 6);
    lv_style_set_pad_ver(&s_styles[HESTIA_STYLE_BTN_SECONDARY], 8);
    lv_style_set_pad_hor(&s_styles[HESTIA_STYLE_BTN_SECONDARY], 12);

    /* BTN_DANGER */
    lv_style_reset(&s_styles[HESTIA_STYLE_BTN_DANGER]);
    lv_style_set_bg_color(&s_styles[HESTIA_STYLE_BTN_DANGER], lv_color_hex(t->danger));
    lv_style_set_bg_opa(&s_styles[HESTIA_STYLE_BTN_DANGER], LV_OPA_COVER);
    lv_style_set_text_color(&s_styles[HESTIA_STYLE_BTN_DANGER], lv_color_hex(0xFFFFFF));
    lv_style_set_radius(&s_styles[HESTIA_STYLE_BTN_DANGER], 6);
    lv_style_set_pad_ver(&s_styles[HESTIA_STYLE_BTN_DANGER], 8);
    lv_style_set_pad_hor(&s_styles[HESTIA_STYLE_BTN_DANGER], 12);

    /* LABEL_PRIMARY */
    lv_style_reset(&s_styles[HESTIA_STYLE_LABEL_PRIMARY]);
    lv_style_set_text_color(&s_styles[HESTIA_STYLE_LABEL_PRIMARY], lv_color_hex(t->text));
    lv_style_set_text_font(&s_styles[HESTIA_STYLE_LABEL_PRIMARY], &lv_font_montserrat_14);

    /* LABEL_SECONDARY */
    lv_style_reset(&s_styles[HESTIA_STYLE_LABEL_SECONDARY]);
    lv_style_set_text_color(&s_styles[HESTIA_STYLE_LABEL_SECONDARY], lv_color_hex(t->text_secondary));
    lv_style_set_text_font(&s_styles[HESTIA_STYLE_LABEL_SECONDARY], &lv_font_montserrat_14);

    /* LABEL_LARGE */
    lv_style_reset(&s_styles[HESTIA_STYLE_LABEL_LARGE]);
    lv_style_set_text_color(&s_styles[HESTIA_STYLE_LABEL_LARGE], lv_color_hex(t->text));
    lv_style_set_text_font(&s_styles[HESTIA_STYLE_LABEL_LARGE], &lv_font_montserrat_24);

    /* LABEL_XLARGE */
    lv_style_reset(&s_styles[HESTIA_STYLE_LABEL_XLARGE]);
    lv_style_set_text_color(&s_styles[HESTIA_STYLE_LABEL_XLARGE], lv_color_hex(t->accent));
    lv_style_set_text_font(&s_styles[HESTIA_STYLE_LABEL_XLARGE], &lv_font_montserrat_36);

    /* SIDEBAR */
    lv_style_reset(&s_styles[HESTIA_STYLE_SIDEBAR]);
    lv_style_set_bg_color(&s_styles[HESTIA_STYLE_SIDEBAR], lv_color_hex(t->surface));
    lv_style_set_bg_opa(&s_styles[HESTIA_STYLE_SIDEBAR], LV_OPA_COVER);
    lv_style_set_border_color(&s_styles[HESTIA_STYLE_SIDEBAR], lv_color_hex(t->border));
    lv_style_set_border_width(&s_styles[HESTIA_STYLE_SIDEBAR], 0);
    lv_style_set_border_side(&s_styles[HESTIA_STYLE_SIDEBAR], LV_BORDER_SIDE_RIGHT);
    lv_style_set_radius(&s_styles[HESTIA_STYLE_SIDEBAR], 0);
    lv_style_set_pad_all(&s_styles[HESTIA_STYLE_SIDEBAR], 0);

    /* DIVIDER */
    lv_style_reset(&s_styles[HESTIA_STYLE_DIVIDER]);
    lv_style_set_bg_color(&s_styles[HESTIA_STYLE_DIVIDER], lv_color_hex(t->border));
    lv_style_set_bg_opa(&s_styles[HESTIA_STYLE_DIVIDER], LV_OPA_COVER);
    lv_style_set_radius(&s_styles[HESTIA_STYLE_DIVIDER], 0);
}

/* ---------- Public API ---------- */

const hestia_theme_t *ui_theme_get(void)
{
    if (!s_initialized) {
        ui_theme_apply(HESTIA_THEME_DARK);
    }
    return &s_active_theme;
}

void ui_theme_apply(hestia_theme_id_t id)
{
    s_active_id = id;
    switch (id) {
        case HESTIA_THEME_LIGHT:
            s_active_theme = HESTIA_THEME_LIGHT_DEF;
            break;
        case HESTIA_THEME_DARK:
        default:
            s_active_theme = HESTIA_THEME_DARK_DEF;
            break;
    }
    if (!s_initialized) {
        for (int i = 0; i < HESTIA_STYLE_COUNT; i++) {
            lv_style_init(&s_styles[i]);
        }
        s_initialized = true;
    }
    rebuild_styles();
    lv_obj_report_style_change(NULL);
}

void ui_theme_apply_custom(const hestia_theme_t *theme)
{
    if (!theme) return;
    s_active_id = HESTIA_THEME_CUSTOM;
    s_active_theme = *theme;
    if (!s_initialized) {
        for (int i = 0; i < HESTIA_STYLE_COUNT; i++) {
            lv_style_init(&s_styles[i]);
        }
        s_initialized = true;
    }
    rebuild_styles();
    lv_obj_report_style_change(NULL);
}

lv_style_t *ui_theme_style(hestia_style_id_t id)
{
    if (!s_initialized) {
        ui_theme_apply(HESTIA_THEME_DARK);
    }
    if (id >= HESTIA_STYLE_COUNT) return &s_styles[HESTIA_STYLE_CARD];
    return &s_styles[id];
}

void ui_theme_apply_card_style(lv_obj_t *obj)
{
    lv_obj_add_style(obj, ui_theme_style(HESTIA_STYLE_CARD), 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void ui_theme_apply_screen_style(lv_obj_t *obj)
{
    lv_obj_add_style(obj, ui_theme_style(HESTIA_STYLE_SCREEN), 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}
