#pragma once

#include "lvgl.h"
#include "user_settings.h"   /* canonical hestia_theme_id_t, device_model_t */
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Theme colour palette ---------- */
typedef struct {
    /* Backgrounds */
    uint32_t bg;              /* main screen background             */
    uint32_t surface;         /* card / panel background            */
    uint32_t surface_variant; /* slightly lighter surface           */
    uint32_t border;          /* widget borders, dividers           */
    /* Text */
    uint32_t text;            /* primary text                       */
    uint32_t text_secondary;  /* secondary / dimmed text            */
    uint32_t text_disabled;   /* disabled widget text               */
    /* Accent */
    uint32_t primary;         /* active selection, accent           */
    uint32_t on_primary;      /* text on primary background         */
    uint32_t accent;          /* highlighted values (temp, etc.)    */
    /* Domain status colours */
    uint32_t heating_color;   /* heating active indicator           */
    uint32_t cooling_color;   /* cooling active indicator           */
    uint32_t fan_color;       /* fan active indicator               */
    uint32_t humidity_color;  /* humidity active indicator          */
    uint32_t hot_water_color; /* hot water active indicator         */
    uint32_t boost_color;     /* boost active indicator             */
    uint32_t inactive_color;  /* domain enabled but idle            */
    /* Semantic */
    uint32_t danger;          /* error / destructive actions        */
    uint32_t success;         /* confirmed / running state          */
    uint32_t warning;         /* cautionary state                   */
} hestia_theme_t;

/* Style keys used by ui_theme_style() */
typedef enum {
    HESTIA_STYLE_SCREEN = 0,
    HESTIA_STYLE_CARD,
    HESTIA_STYLE_BTN_PRIMARY,
    HESTIA_STYLE_BTN_SECONDARY,
    HESTIA_STYLE_BTN_DANGER,
    HESTIA_STYLE_LABEL_PRIMARY,
    HESTIA_STYLE_LABEL_SECONDARY,
    HESTIA_STYLE_LABEL_LARGE,
    HESTIA_STYLE_LABEL_XLARGE,
    HESTIA_STYLE_SIDEBAR,
    HESTIA_STYLE_DIVIDER,
    HESTIA_STYLE_COUNT,
} hestia_style_id_t;

/* Return pointer to the active theme */
const hestia_theme_t *ui_theme_get(void);

/* Apply a built-in theme and refresh all LVGL styles */
void ui_theme_apply(hestia_theme_id_t id);

/* Apply a custom theme struct */
void ui_theme_apply_custom(const hestia_theme_t *theme);

/* Return a pre-built lv_style_t for common widget types */
lv_style_t *ui_theme_style(hestia_style_id_t id);

/* Convenience: set object bg + border from theme colours */
void ui_theme_apply_card_style(lv_obj_t *obj);
void ui_theme_apply_screen_style(lv_obj_t *obj);

/* Built-in theme definitions (read-only) */
extern const hestia_theme_t HESTIA_THEME_DARK_DEF;
extern const hestia_theme_t HESTIA_THEME_LIGHT_DEF;

#ifdef __cplusplus
}
#endif
