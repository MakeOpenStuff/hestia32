#include "ui/ui_main.h"
#include "ui/ui_theme.h"
#include "ui/ui_common.h"
#include "ui/ui_icons.h"
#include "ui/ui_settings.h"
#include "core/display_config.h"
#include "core/user_settings.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <cmath>

static const char *TAG = "ui_main";

/* ─── Sensor data (thread-safe cache) ─────────────────────────── */
static struct {
    float temperature;
    float humidity;
    bool  is_celsius;
    bool  has_data;
} s_sensor = {.temperature = 0, .humidity = 0, .is_celsius = true, .has_data = false};

/* ─── Screen / pane references ─────────────────────────────────── */
static lv_obj_t *s_scr          = NULL;  /* root screen             */
static lv_obj_t *s_time_label   = NULL;  /* "12:34"                 */
static lv_obj_t *s_date_label   = NULL;  /* "Tue, 8 Jul"            */
static lv_obj_t *s_temp_label   = NULL;  /* "21.5"                  */
static lv_obj_t *s_temp_unit    = NULL;  /* "°C"                    */
static lv_obj_t *s_humi_label   = NULL;  /* "48"                    */
static lv_obj_t *s_wifi_dot     = NULL;  /* 8px status dot          */
static lv_obj_t *s_eco_btn      = NULL;  /* eco/comfort toggle      */

/* ─── Sidebar domain widgets ────────────────────────────────────── */
#define MAX_SIDEBAR_DOMAINS 7  /* Maximum possible domains */
static int s_sidebar_domain_count = 0;  /* Actual enabled domain count */
static int s_domain_to_idx[7];          /* Map domain bit (0-6) to sidebar index */
static int s_idx_to_domain[7];          /* Map sidebar index to domain bit */
static lv_obj_t *s_domain_btn[MAX_SIDEBAR_DOMAINS];   /* icon buttons   */
static lv_obj_t *s_domain_dot[MAX_SIDEBAR_DOMAINS];   /* status dots    */
static lv_obj_t *s_domain_lbl[MAX_SIDEBAR_DOMAINS];   /* text labels    */
static lv_obj_t *s_domain_bar[MAX_SIDEBAR_DOMAINS];   /* active left-edge indicator bars */

/* ─── Controls pane ─────────────────────────────────────────────── */
static lv_obj_t *s_ctrl_panels[7]; /* one per domain bit (0-6)  */
static lv_obj_t *s_ctrl_title  = NULL;
static lv_obj_t *s_ctrl_badge  = NULL;
static ui_domain_t s_active_domain = UI_DOMAIN_HEATING;

/* Per-heating/cooling panel setpoint labels */
static lv_obj_t *s_heat_sp_label = NULL;
static lv_obj_t *s_cool_sp_label = NULL;
static float     s_heat_sp       = 20.0f;  /* Always stored in Celsius - heat when below this */
static float     s_cool_sp       = 25.0f;  /* Always stored in Celsius - cool when above this */

/* ─── Domain metadata (all 7 possible domains) ────────────────────── */
static const char *ALL_DOMAIN_DISPLAY_NAMES[7] = {
    "Heating", "Heating", "Cooling", "Cooling", "Fan", "Humidity", "Hot Water",
};
static const char *ALL_DOMAIN_ICONS[7] = {
    ICON_HEATING, ICON_HEATING, ICON_COOLING, ICON_COOLING, ICON_FAN, ICON_HUMIDITY, ICON_HOT_WATER,
};
/* Helper to get domain color (uses theme for fan) */
static uint32_t get_domain_color(int domain_bit) {
    const hestia_theme_t *t = ui_theme_get();
    switch (domain_bit) {
        case 0: return 0xFF6B35;  /* heating stage 1 - orange */
        case 1: return 0xFF4500;  /* heating stage 2 (darker orange) */
        case 2: return 0x4FC3F7;  /* cooling stage 1 - blue */
        case 3: return 0x00BFFF;  /* cooling stage 2 (darker blue) */
        case 4: return t->danger; /* fan - uses danger (displays green due to inversion) */
        case 5: return 0x81D4FA;  /* humidity - light blue */
        case 6: return 0xFFCC02;  /* hot water - yellow */
        default: return t->text_secondary;
    }
}

/* ─── Forward declarations ──────────────────────────────────────── */
static void create_sidebar(lv_obj_t *parent);
static void create_current_pane(lv_obj_t *parent);
static void create_controls_pane(lv_obj_t *parent);
static void create_heating_panel(lv_obj_t *parent);
static void create_cooling_panel(lv_obj_t *parent);
static void create_fan_panel(lv_obj_t *parent);
static void create_humidity_panel(lv_obj_t *parent);
static void create_hotwater_panel(lv_obj_t *parent);
static void select_domain(ui_domain_t domain);
static void update_clock(void);
static void on_settings_btn(lv_event_t *e);
static void on_domain_btn(lv_event_t *e);
static void on_heat_minus(lv_event_t *e);
static void on_heat_plus(lv_event_t *e);
static void on_cool_minus(lv_event_t *e);
static void on_cool_plus(lv_event_t *e);
static void on_eco_toggle(lv_event_t *e);
static void timer_cb(lv_timer_t *t);

/* ─── Helper: small lv_obj styled button ───────────────────────── */
static lv_obj_t *make_sp_btn(lv_obj_t *parent, const char *sym,
                               lv_event_cb_t cb)
{
    const hestia_theme_t *t = ui_theme_get();
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 40, 40);
    lv_obj_set_style_bg_color(btn, lv_color_hex(t->surface_variant), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(t->border), 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, sym);
    lv_obj_set_style_text_color(lbl, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    return btn;
}

/* ─── Sidebar ───────────────────────────────────────────────────── */

static void create_sidebar(lv_obj_t *parent)
{
    const hestia_theme_t *t = ui_theme_get();

    /* Load device config to determine which domains are enabled */
    device_config_t cfg;
    device_config_get(&cfg);
    uint8_t domain_mask = cfg.domain_mask;

    /* Build mapping of enabled domains - collapse S1/S2 into single sidebar entries */
    s_sidebar_domain_count = 0;
    for (int i = 0; i < 7; i++) {
        s_domain_to_idx[i] = -1;  /* Not enabled */
    }

    bool heat_s1_enabled = (domain_mask & (1<<0));
    bool cool_s1_enabled = (domain_mask & (1<<2));

    for (int d = 0; d < 7; d++) {
        if (domain_mask & (1 << d)) {
            /* Skip S2 entries if corresponding S1 is also enabled (collapse to single sidebar entry) */
            if (d == 1 && heat_s1_enabled) continue;  /* Skip Heat S2 if Heat S1 enabled */
            if (d == 3 && cool_s1_enabled) continue;  /* Skip Cool S2 if Cool S1 enabled */
            /* Note: Domain bit 7 (reversing valve) is never shown in sidebar */

            s_domain_to_idx[d] = s_sidebar_domain_count;
            s_idx_to_domain[s_sidebar_domain_count] = d;
            s_sidebar_domain_count++;
        }
    }

    /* If no domains enabled, show at least one (default to heating) */
    if (s_sidebar_domain_count == 0) {
        ESP_LOGW(TAG, "No domains enabled, showing Heating S1 as default");
        s_domain_to_idx[0] = 0;
        s_idx_to_domain[0] = 0;
        s_sidebar_domain_count = 1;
    }

    lv_obj_t *sb = lv_obj_create(parent);
    lv_obj_set_size(sb, UI_SIDEBAR_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(sb, lv_color_hex(t->surface), 0);
    lv_obj_set_style_bg_opa(sb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sb, 0, 0);
    lv_obj_set_style_border_side(sb, LV_BORDER_SIDE_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_border_color(sb, lv_color_hex(t->border), LV_PART_MAIN);
    lv_obj_set_style_border_width(sb, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(sb, 0, 0);
    lv_obj_set_style_pad_all(sb, 0, 0);
    lv_obj_clear_flag(sb, LV_OBJ_FLAG_SCROLLABLE);

    /* Create all MAX_SIDEBAR_DOMAINS buttons upfront (hide unused ones) */
    /* Status dots — absolute screen coordinates */
    lv_obj_t *scr_overlay = lv_obj_get_parent(parent);

    for (int i = 0; i < MAX_SIDEBAR_DOMAINS; i++) {
        lv_obj_t *btn = lv_obj_create(sb);
        lv_obj_set_size(btn, UI_SIDEBAR_W, UI_SCREEN_H);  /* Will be resized on reload */
        lv_obj_set_style_bg_color(btn, lv_color_hex(t->surface), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(t->border), 0);
        lv_obj_set_style_radius(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_add_event_cb(btn, on_domain_btn, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        s_domain_btn[i] = btn;

        /* Active indicator bar */
        lv_obj_t *bar = lv_obj_create(btn);
        lv_obj_set_size(bar, 3, UI_SCREEN_H);  /* Will be resized on reload */
        lv_obj_set_style_bg_color(bar, lv_color_hex(t->primary), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_radius(bar, 0, 0);
        lv_obj_set_style_pad_all(bar, 0, 0);
        lv_obj_align(bar, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
        s_domain_bar[i] = bar;

        /* Icon */
        lv_obj_t *icon = lv_label_create(btn);
        lv_label_set_text(icon, ICON_HEATING);  /* Will be updated on reload */
        lv_obj_set_style_text_color(icon, lv_color_hex(t->text_secondary), 0);
        lv_obj_set_style_text_font(icon, ICON_FONT_28 ? ICON_FONT_28 : &lv_font_montserrat_24, 0);
        lv_obj_align(icon, LV_ALIGN_CENTER, 0, 0);
        s_domain_lbl[i] = icon;

        /* Status dot */
        lv_obj_t *dot = lv_obj_create(scr_overlay);
        lv_obj_set_size(dot, 10, 10);
        lv_obj_set_style_bg_color(dot, lv_color_hex(t->inactive_color), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_set_style_pad_all(dot, 0, 0);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(dot, UI_SIDEBAR_W - 10 - 6, 0);
        s_domain_dot[i] = dot;

        /* Initially hide all buttons - reload will show the enabled ones */
        if (i >= s_sidebar_domain_count) {
            lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* Now position and show the enabled domains */
    const int btn_h = UI_SCREEN_H / s_sidebar_domain_count;
    for (int i = 0; i < s_sidebar_domain_count; i++) {
        int domain_bit = s_idx_to_domain[i];

        lv_obj_set_size(s_domain_btn[i], UI_SIDEBAR_W, btn_h);
        lv_obj_set_pos(s_domain_btn[i], 0, i * btn_h);
        lv_obj_clear_flag(s_domain_btn[i], LV_OBJ_FLAG_HIDDEN);

        lv_label_set_text(s_domain_lbl[i], ALL_DOMAIN_ICONS[domain_bit]);
        lv_obj_set_size(s_domain_bar[i], 3, btn_h);
        lv_obj_set_style_bg_color(s_domain_bar[i], lv_color_hex(get_domain_color(domain_bit)), 0);

        lv_obj_set_pos(s_domain_dot[i], UI_SIDEBAR_W - 10 - 6, i * btn_h + 4);
        lv_obj_clear_flag(s_domain_dot[i], LV_OBJ_FLAG_HIDDEN);
    }
}

/* ─── Reload sidebar (called when domains change) ──────────────── */
void ui_main_reload_sidebar(void)
{
    if (!s_scr) return;  /* Main UI not created yet */

    /* Load device config to determine which domains are enabled */
    device_config_t cfg;
    device_config_get(&cfg);
    uint8_t domain_mask = cfg.domain_mask;

    /* Hide all domain buttons and dots first */
    for (int i = 0; i < MAX_SIDEBAR_DOMAINS; i++) {
        if (s_domain_btn[i]) lv_obj_add_flag(s_domain_btn[i], LV_OBJ_FLAG_HIDDEN);
        if (s_domain_dot[i]) lv_obj_add_flag(s_domain_dot[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* Rebuild mapping of enabled domains - collapse S1/S2 into single sidebar entries */
    s_sidebar_domain_count = 0;
    for (int i = 0; i < 7; i++) {
        s_domain_to_idx[i] = -1;
    }

    bool heat_s1_enabled = (domain_mask & (1<<0));
    bool cool_s1_enabled = (domain_mask & (1<<2));

    for (int d = 0; d < 7; d++) {
        if (domain_mask & (1 << d)) {
            /* Skip S2 entries if corresponding S1 is also enabled (collapse to single sidebar entry) */
            if (d == 1 && heat_s1_enabled) continue;  /* Skip Heat S2 if Heat S1 enabled */
            if (d == 3 && cool_s1_enabled) continue;  /* Skip Cool S2 if Cool S1 enabled */
            /* Note: Domain bit 7 (reversing valve) is never shown in sidebar */

            s_domain_to_idx[d] = s_sidebar_domain_count;
            s_idx_to_domain[s_sidebar_domain_count] = d;
            s_sidebar_domain_count++;
        }
    }

    if (s_sidebar_domain_count == 0) {
        ESP_LOGW(TAG, "No domains enabled after reload, showing Heating S1 as default");
        s_domain_to_idx[0] = 0;
        s_idx_to_domain[0] = 0;
        s_sidebar_domain_count = 1;
    }

    /* Recalculate button heights */
    const int btn_h = UI_SCREEN_H / s_sidebar_domain_count;

    /* Show and reposition active domain buttons */
    for (int i = 0; i < s_sidebar_domain_count; i++) {
        int domain_bit = s_idx_to_domain[i];

        if (s_domain_btn[i]) {
            lv_obj_set_size(s_domain_btn[i], UI_SIDEBAR_W, btn_h);
            lv_obj_set_pos(s_domain_btn[i], 0, i * btn_h);
            lv_obj_clear_flag(s_domain_btn[i], LV_OBJ_FLAG_HIDDEN);

            /* Update icon */
            if (s_domain_lbl[i]) {
                lv_label_set_text(s_domain_lbl[i], ALL_DOMAIN_ICONS[domain_bit]);
            }

            /* Update bar color and size */
            if (s_domain_bar[i]) {
                lv_obj_set_size(s_domain_bar[i], 3, btn_h);
                lv_obj_set_style_bg_color(s_domain_bar[i], lv_color_hex(get_domain_color(domain_bit)), 0);
            }
        }

        if (s_domain_dot[i]) {
            lv_obj_set_pos(s_domain_dot[i], UI_SIDEBAR_W - 10 - 6, i * btn_h + 4);
            lv_obj_clear_flag(s_domain_dot[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* Reset to first domain if current is now invalid */
    if (s_active_domain >= s_sidebar_domain_count) {
        select_domain((ui_domain_t)0);  /* Select first domain */
    } else {
        /* Re-select current to update title */
        select_domain(s_active_domain);
    }
}

/* ─── Current conditions pane ──────────────────────────────────── */

static void create_current_pane(lv_obj_t *parent)
{
    const hestia_theme_t *t = ui_theme_get();

    lv_obj_t *pane = lv_obj_create(parent);
    lv_obj_set_size(pane, UI_CURRENT_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(pane, lv_color_hex(t->bg), 0);
    lv_obj_set_style_bg_opa(pane, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pane, 0, 0);
    lv_obj_set_style_border_side(pane, LV_BORDER_SIDE_LEFT, LV_PART_MAIN);
    lv_obj_set_style_border_color(pane, lv_color_hex(t->border), LV_PART_MAIN);
    lv_obj_set_style_border_width(pane, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(pane, 0, 0);
    lv_obj_set_style_pad_all(pane, UI_PAD, 0);
    lv_obj_clear_flag(pane, LV_OBJ_FLAG_SCROLLABLE);

    /* Time label */
    s_time_label = lv_label_create(pane);
    lv_label_set_text(s_time_label, "--:--");
    lv_obj_set_style_text_color(s_time_label, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_36, 0);
    lv_obj_align(s_time_label, LV_ALIGN_TOP_LEFT, 0, 4);

    /* Date label */
    s_date_label = lv_label_create(pane);
    lv_label_set_text(s_date_label, "---");
    lv_obj_set_style_text_color(s_date_label, lv_color_hex(t->text_secondary), 0);
    lv_obj_set_style_text_font(s_date_label, &lv_font_montserrat_14, 0);
    lv_obj_align(s_date_label, LV_ALIGN_TOP_LEFT, 0, 48);

    /* Temperature row: flex container so unit label always tracks value width */
    lv_obj_t *temp_row = lv_obj_create(pane);
    lv_obj_set_size(temp_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(temp_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(temp_row, 0, 0);
    lv_obj_set_style_pad_all(temp_row, 0, 0);
    lv_obj_set_style_pad_column(temp_row, 2, 0);
    lv_obj_clear_flag(temp_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(temp_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(temp_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(temp_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);
    lv_obj_align(temp_row, LV_ALIGN_TOP_MID, 0, 80);  /* Centered horizontally for photoshoot */

    s_temp_label = lv_label_create(temp_row);
    lv_label_set_text(s_temp_label, "--.-");
    lv_obj_set_style_text_color(s_temp_label, lv_color_hex(t->accent), 0);
    lv_obj_set_style_text_font(s_temp_label, &lv_font_montserrat_48, 0);  /* Larger for prominence */

    s_temp_unit = lv_label_create(temp_row);
    lv_label_set_text(s_temp_unit, "\xC2\xB0""C");
    lv_obj_set_style_text_color(s_temp_unit, lv_color_hex(t->text_secondary), 0);
    lv_obj_set_style_text_font(s_temp_unit, &lv_font_montserrat_24, 0);  /* Proportionally larger */
    lv_obj_set_style_pad_bottom(s_temp_unit, 6, 0); /* raise unit to visual baseline of larger font */

    /* Humidity row: same flex pattern */
    lv_obj_t *humi_row = lv_obj_create(pane);
    lv_obj_set_size(humi_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(humi_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(humi_row, 0, 0);
    lv_obj_set_style_pad_all(humi_row, 0, 0);
    lv_obj_set_style_pad_column(humi_row, 2, 0);
    lv_obj_clear_flag(humi_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(humi_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(humi_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(humi_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);
    lv_obj_align(humi_row, LV_ALIGN_TOP_MID, 0, 150);  /* Centered + adjusted Y for larger temp */

    s_humi_label = lv_label_create(humi_row);
    lv_label_set_text(s_humi_label, "--%");
    lv_obj_set_style_text_color(s_humi_label, lv_color_hex(t->humidity_color), 0);
    lv_obj_set_style_text_font(s_humi_label, &lv_font_montserrat_36, 0);  /* Larger but smaller than temp */

    lv_obj_t *rh_lbl = lv_label_create(humi_row);
    lv_label_set_text(rh_lbl, "RH");
    lv_obj_set_style_text_color(rh_lbl, lv_color_hex(t->text_secondary), 0);
    lv_obj_set_style_text_font(rh_lbl, &lv_font_montserrat_16, 0);  /* Proportionally larger */
    lv_obj_set_style_pad_bottom(rh_lbl, 3, 0); /* visual baseline alignment */

    /* Eco/Comfort toggle button - just colored icon, no border/background (blends with pane) */
    thermostat_settings_t therm_settings;
    thermostat_settings_get(&therm_settings);
    bool is_eco = (therm_settings.therm_mode == 1);  /* 0=comfort, 1=eco, 2=off */

    s_eco_btn = lv_btn_create(pane);
    lv_obj_set_size(s_eco_btn, 52, 52);
    lv_obj_set_style_bg_opa(s_eco_btn, LV_OPA_TRANSP, 0);  /* Transparent background */
    lv_obj_set_style_shadow_width(s_eco_btn, 0, 0);
    lv_obj_set_style_border_width(s_eco_btn, 0, 0);  /* No border */
    lv_obj_align(s_eco_btn, LV_ALIGN_TOP_MID, 0, 210);
    lv_obj_add_event_cb(s_eco_btn, on_eco_toggle, LV_EVENT_CLICKED, NULL);

    lv_obj_t *eco_icon = lv_label_create(s_eco_btn);
    lv_label_set_text(eco_icon, ICON_ECO);
    lv_obj_set_style_text_color(eco_icon,
        lv_color_hex(is_eco ? t->success : t->text_secondary), 0);  /* Green leaf for eco, grey for comfort */
    lv_obj_set_style_text_font(eco_icon, ICON_FONT_28 ? ICON_FONT_28 : &lv_font_montserrat_20, 0);  /* Larger icon */
    lv_obj_align(eco_icon, LV_ALIGN_CENTER, 0, 0);

    /* WiFi status dot */
    s_wifi_dot = lv_obj_create(pane);
    lv_obj_set_size(s_wifi_dot, 8, 8);
    lv_obj_set_style_bg_color(s_wifi_dot, lv_color_hex(t->inactive_color), 0);
    lv_obj_set_style_bg_opa(s_wifi_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_wifi_dot, 0, 0);
    lv_obj_set_style_radius(s_wifi_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(s_wifi_dot, LV_ALIGN_BOTTOM_LEFT, 0, -4);

    lv_obj_t *wifi_lbl = lv_label_create(pane);
    lv_label_set_text(wifi_lbl, ICON_WIFI);
    lv_obj_set_style_text_color(wifi_lbl, lv_color_hex(t->text_secondary), 0);
    lv_obj_set_style_text_font(wifi_lbl, ICON_FONT_20 ? ICON_FONT_20 : &lv_font_montserrat_14, 0);
    lv_obj_align(wifi_lbl, LV_ALIGN_BOTTOM_LEFT, 14, -4);

    /* Settings gear — placed in this pane (not the controls header) because the
     * controls header sits at y=0 in the top-right area of the display, which is
     * a double edge dead-zone for resistive touch.  This pane occupies x=80–255
     * so the gear at BOTTOM_RIGHT is at ~x=211–247, y=272–308 — comfortably
     * within the calibrated touch area on all sides. */
    lv_obj_t *gear_btn = lv_obj_create(pane);
    lv_obj_set_size(gear_btn, 36, 36);
    lv_obj_set_style_bg_color(gear_btn, lv_color_hex(t->surface_variant), 0);
    lv_obj_set_style_bg_opa(gear_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(gear_btn, lv_color_hex(t->border), 0);
    lv_obj_set_style_border_width(gear_btn, 1, 0);
    lv_obj_set_style_radius(gear_btn, 6, 0);
    lv_obj_set_style_pad_all(gear_btn, 0, 0);
    lv_obj_clear_flag(gear_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(gear_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(gear_btn, LV_ALIGN_BOTTOM_RIGHT, 0, -4);
    lv_obj_add_event_cb(gear_btn, on_settings_btn, LV_EVENT_CLICKED, NULL);

    lv_obj_t *gear_lbl = lv_label_create(gear_btn);
    lv_label_set_text(gear_lbl, ICON_SETTINGS);
    lv_obj_set_style_text_color(gear_lbl, lv_color_hex(t->text_secondary), 0);
    lv_obj_set_style_text_font(gear_lbl, ICON_FONT_20 ? ICON_FONT_20 : &lv_font_montserrat_14, 0);
    lv_obj_align(gear_lbl, LV_ALIGN_CENTER, 0, 0);
}

/* ─── Controls pane ─────────────────────────────────────────────── */

static void create_heating_panel(lv_obj_t *parent)
{
    const hestia_theme_t *t = ui_theme_get();
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_size(p, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(p, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_pad_all(p, UI_PAD, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    s_ctrl_panels[UI_DOMAIN_HEATING] = p;

    /* Setpoint row */
    lv_obj_t *row = lv_obj_create(p);
    lv_obj_set_size(row, LV_PCT(100), 52);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, 4);

    make_sp_btn(row, LV_SYMBOL_MINUS, on_heat_minus);
    lv_obj_t *minus = lv_obj_get_child(row, 0);
    lv_obj_align(minus, LV_ALIGN_LEFT_MID, 0, 0);

    s_heat_sp_label = lv_label_create(row);
    temp_unit_t unit = user_settings_get_temp_unit();
    bool is_celsius = (unit == TEMP_UNIT_CELSIUS);
    float display_temp = is_celsius ? s_heat_sp : temp_c_to_f(s_heat_sp);
    char buf[16]; snprintf(buf, sizeof(buf), "%.1f", display_temp);
    lv_label_set_text(s_heat_sp_label, buf);
    lv_obj_set_style_text_color(s_heat_sp_label, lv_color_hex(t->heating_color), 0);
    lv_obj_set_style_text_font(s_heat_sp_label, &lv_font_montserrat_28, 0);
    lv_obj_align(s_heat_sp_label, LV_ALIGN_CENTER, 0, 0);

    make_sp_btn(row, LV_SYMBOL_PLUS, on_heat_plus);
    lv_obj_t *plus = lv_obj_get_child(row, 2);
    lv_obj_align(plus, LV_ALIGN_RIGHT_MID, 0, 0);

    /* AUTO / ON / OFF mode buttons */
    const int bw = (UI_CONTROLS_W - UI_PAD * 2 - 4) / 3;
    const char *modes[3] = {"AUTO", "ON", "OFF"};
    const int   y        = 64;

    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_btn_create(p);
        lv_obj_set_size(btn, bw, 40);
        bool active = (i == 0); /* AUTO default */
        lv_obj_set_style_bg_color(btn,
            lv_color_hex(active ? t->primary : t->surface_variant), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(t->border), 0);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, i * (bw + 2), y);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, modes[i]);
        lv_obj_set_style_text_color(lbl,
            lv_color_hex(active ? t->on_primary : t->text), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    }

    /* BOOST button */
    lv_obj_t *boost = lv_btn_create(p);
    lv_obj_set_size(boost, LV_PCT(100), 44);
    lv_obj_set_style_bg_color(boost, lv_color_hex(t->boost_color), 0);
    lv_obj_set_style_bg_opa(boost, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(boost, 0, 0);
    lv_obj_set_style_radius(boost, 6, 0);
    lv_obj_align(boost, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *boost_ico_h = lv_label_create(boost);
    lv_label_set_text(boost_ico_h, ICON_BOOST);
    lv_obj_set_style_text_color(boost_ico_h, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(boost_ico_h, ICON_FONT_20 ? ICON_FONT_20 : &lv_font_montserrat_14, 0);
    lv_obj_align(boost_ico_h, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_t *blbl = lv_label_create(boost);
    lv_label_set_text(blbl, "BOOST  30 min");
    lv_obj_set_style_text_color(blbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(blbl, &lv_font_montserrat_14, 0);
    lv_obj_align(blbl, LV_ALIGN_CENTER, 10, 0);
}

static void create_cooling_panel(lv_obj_t *parent)
{
    const hestia_theme_t *t = ui_theme_get();
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_size(p, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(p, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_pad_all(p, UI_PAD, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    s_ctrl_panels[2] = p;  /* Cooling S1 - domain bit 2 */

    /* Setpoint row */
    lv_obj_t *row = lv_obj_create(p);
    lv_obj_set_size(row, LV_PCT(100), 52);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, 4);

    make_sp_btn(row, LV_SYMBOL_MINUS, on_cool_minus);
    lv_obj_t *minus = lv_obj_get_child(row, 0);
    lv_obj_align(minus, LV_ALIGN_LEFT_MID, 0, 0);

    s_cool_sp_label = lv_label_create(row);
    temp_unit_t unit_c = user_settings_get_temp_unit();
    bool is_celsius_c = (unit_c == TEMP_UNIT_CELSIUS);
    float display_temp_c = is_celsius_c ? s_cool_sp : temp_c_to_f(s_cool_sp);
    char buf[16]; snprintf(buf, sizeof(buf), "%.1f", display_temp_c);
    lv_label_set_text(s_cool_sp_label, buf);
    lv_obj_set_style_text_color(s_cool_sp_label, lv_color_hex(t->cooling_color), 0);
    lv_obj_set_style_text_font(s_cool_sp_label, &lv_font_montserrat_28, 0);
    lv_obj_align(s_cool_sp_label, LV_ALIGN_CENTER, 0, 0);

    make_sp_btn(row, LV_SYMBOL_PLUS, on_cool_plus);
    lv_obj_t *plus = lv_obj_get_child(row, 2);
    lv_obj_align(plus, LV_ALIGN_RIGHT_MID, 0, 0);

    /* AUTO / ON / OFF mode buttons */
    const int bw_c = (UI_CONTROLS_W - UI_PAD * 2 - 4) / 3;
    const char *modes_c[3] = {"AUTO", "ON", "OFF"};
    const int   y_c        = 64;

    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_btn_create(p);
        lv_obj_set_size(btn, bw_c, 40);
        bool active = (i == 0); /* AUTO default */
        lv_obj_set_style_bg_color(btn,
            lv_color_hex(active ? t->primary : t->surface_variant), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(t->border), 0);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, i * (bw_c + 2), y_c);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, modes_c[i]);
        lv_obj_set_style_text_color(lbl,
            lv_color_hex(active ? t->on_primary : t->text), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    }

    /* BOOST button */
    lv_obj_t *boost = lv_btn_create(p);
    lv_obj_set_size(boost, LV_PCT(100), 44);
    lv_obj_set_style_bg_color(boost, lv_color_hex(t->boost_color), 0);
    lv_obj_set_style_bg_opa(boost, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(boost, 0, 0);
    lv_obj_set_style_radius(boost, 6, 0);
    lv_obj_align(boost, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *boost_ico_c = lv_label_create(boost);
    lv_label_set_text(boost_ico_c, ICON_BOOST);
    lv_obj_set_style_text_color(boost_ico_c, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(boost_ico_c, ICON_FONT_20 ? ICON_FONT_20 : &lv_font_montserrat_14, 0);
    lv_obj_align(boost_ico_c, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_t *blbl = lv_label_create(boost);
    lv_label_set_text(blbl, "BOOST  30 min");
    lv_obj_set_style_text_color(blbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(blbl, &lv_font_montserrat_14, 0);
    lv_obj_align(blbl, LV_ALIGN_CENTER, 10, 0);
}

static void create_fan_panel(lv_obj_t *parent)
{
    const hestia_theme_t *t = ui_theme_get();
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_size(p, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(p, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_pad_all(p, UI_PAD, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    s_ctrl_panels[4] = p;  /* Fan - domain bit 4 */

    /* AUTO / ON / OFF buttons */
    const int bw = (UI_CONTROLS_W - UI_PAD * 2 - 4) / 3;
    const char *modes[3] = {"AUTO", "ON", "OFF"};
    const int   y        = 40;

    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_btn_create(p);
        lv_obj_set_size(btn, bw, 40);
        bool active = (i == 0); /* AUTO default */
        lv_obj_set_style_bg_color(btn,
            lv_color_hex(active ? t->primary : t->surface_variant), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(t->border), 0);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, i * (bw + 2), y);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, modes[i]);
        lv_obj_set_style_text_color(lbl,
            lv_color_hex(active ? t->on_primary : t->text), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    }

    lv_obj_t *status = lv_label_create(p);
    lv_label_set_text(status, "Currently: Auto (off)");
    lv_obj_set_style_text_color(status, lv_color_hex(t->text_secondary), 0);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_14, 0);
    lv_obj_align(status, LV_ALIGN_TOP_LEFT, 0, y + 50);
}

static void create_humidity_panel(lv_obj_t *parent)
{
    const hestia_theme_t *t = ui_theme_get();
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_size(p, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(p, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_pad_all(p, UI_PAD, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    s_ctrl_panels[5] = p;  /* Humidity - domain bit 5 */

    /* HUMIDIFY / DEHUMIDIFY */
    const int half = (UI_CONTROLS_W - UI_PAD * 2) / 2;
    lv_obj_t *hum = lv_btn_create(p);
    lv_obj_set_size(hum, half - 2, 36);
    lv_obj_set_style_bg_color(hum, lv_color_hex(t->primary), 0);
    lv_obj_set_style_bg_opa(hum, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(hum, 0, 0);
    lv_obj_set_style_radius(hum, 6, 0);
    lv_obj_align(hum, LV_ALIGN_TOP_LEFT, 0, 4);
    lv_obj_t *hl = lv_label_create(hum);
    lv_label_set_text(hl, "HUMIDIFY");
    lv_obj_set_style_text_color(hl, lv_color_hex(t->on_primary), 0);
    lv_obj_set_style_text_font(hl, &lv_font_montserrat_14, 0);
    lv_obj_align(hl, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *deh = lv_btn_create(p);
    lv_obj_set_size(deh, half - 2, 36);
    lv_obj_set_style_bg_color(deh, lv_color_hex(t->surface_variant), 0);
    lv_obj_set_style_bg_opa(deh, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(deh, 0, 0);
    lv_obj_set_style_border_color(deh, lv_color_hex(t->border), 0);
    lv_obj_set_style_border_width(deh, 1, 0);
    lv_obj_set_style_radius(deh, 6, 0);
    lv_obj_align(deh, LV_ALIGN_TOP_RIGHT, 0, 4);
    lv_obj_t *dl = lv_label_create(deh);
    lv_label_set_text(dl, "DEHUMIDIFY");
    lv_obj_set_style_text_color(dl, lv_color_hex(t->text_secondary), 0);
    lv_obj_set_style_text_font(dl, &lv_font_montserrat_14, 0);
    lv_obj_align(dl, LV_ALIGN_CENTER, 0, 0);

    /* Setpoint controls */
    lv_obj_t *sp_row = lv_obj_create(p);
    lv_obj_set_size(sp_row, LV_PCT(100), 52);
    lv_obj_set_style_bg_opa(sp_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sp_row, 0, 0);
    lv_obj_set_style_pad_all(sp_row, 0, 0);
    lv_obj_clear_flag(sp_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(sp_row, LV_ALIGN_TOP_LEFT, 0, 48);

    make_sp_btn(sp_row, LV_SYMBOL_MINUS, NULL);  /* TODO: Add humidity minus callback */
    lv_obj_t *sp_minus = lv_obj_get_child(sp_row, 0);
    lv_obj_align(sp_minus, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *sp_lbl = lv_label_create(sp_row);
    lv_label_set_text(sp_lbl, "50%");
    lv_obj_set_style_text_color(sp_lbl, lv_color_hex(t->humidity_color), 0);
    lv_obj_set_style_text_font(sp_lbl, &lv_font_montserrat_28, 0);
    lv_obj_align(sp_lbl, LV_ALIGN_CENTER, 0, 0);

    make_sp_btn(sp_row, LV_SYMBOL_PLUS, NULL);  /* TODO: Add humidity plus callback */
    lv_obj_t *sp_plus = lv_obj_get_child(sp_row, 2);
    lv_obj_align(sp_plus, LV_ALIGN_RIGHT_MID, 0, 0);

    /* AUTO / ON / OFF mode buttons */
    const int bw_h = (UI_CONTROLS_W - UI_PAD * 2 - 4) / 3;
    const char *modes_h[3] = {"AUTO", "ON", "OFF"};
    const int   y_h        = 108;

    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_btn_create(p);
        lv_obj_set_size(btn, bw_h, 40);
        bool active = (i == 0); /* AUTO default */
        lv_obj_set_style_bg_color(btn,
            lv_color_hex(active ? t->primary : t->surface_variant), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(t->border), 0);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, i * (bw_h + 2), y_h);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, modes_h[i]);
        lv_obj_set_style_text_color(lbl,
            lv_color_hex(active ? t->on_primary : t->text), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    }

    /* BOOST button */
    lv_obj_t *boost = lv_btn_create(p);
    lv_obj_set_size(boost, LV_PCT(100), 44);
    lv_obj_set_style_bg_color(boost, lv_color_hex(t->boost_color), 0);
    lv_obj_set_style_bg_opa(boost, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(boost, 0, 0);
    lv_obj_set_style_radius(boost, 6, 0);
    lv_obj_align(boost, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *boost_ico_h = lv_label_create(boost);
    lv_label_set_text(boost_ico_h, ICON_BOOST);
    lv_obj_set_style_text_color(boost_ico_h, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(boost_ico_h, ICON_FONT_20 ? ICON_FONT_20 : &lv_font_montserrat_14, 0);
    lv_obj_align(boost_ico_h, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_t *blbl_h = lv_label_create(boost);
    lv_label_set_text(blbl_h, "BOOST  30 min");
    lv_obj_set_style_text_color(blbl_h, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(blbl_h, &lv_font_montserrat_14, 0);
    lv_obj_align(blbl_h, LV_ALIGN_CENTER, 10, 0);
}

static void create_hotwater_panel(lv_obj_t *parent)
{
    const hestia_theme_t *t = ui_theme_get();
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_size(p, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(p, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_pad_all(p, UI_PAD, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    s_ctrl_panels[6] = p;  /* Hot Water - domain bit 6 */

    /* Demand ON/OFF toggle */
    lv_obj_t *demand = lv_btn_create(p);
    lv_obj_set_size(demand, LV_PCT(100), 52);
    lv_obj_set_style_bg_color(demand, lv_color_hex(t->surface_variant), 0);
    lv_obj_set_style_bg_opa(demand, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(demand, 0, 0);
    lv_obj_set_style_border_color(demand, lv_color_hex(t->border), 0);
    lv_obj_set_style_border_width(demand, 1, 0);
    lv_obj_set_style_radius(demand, 8, 0);
    lv_obj_align(demand, LV_ALIGN_TOP_LEFT, 0, 4);
    lv_obj_t *dl = lv_label_create(demand);
    lv_label_set_text(dl, "DEMAND ON");
    lv_obj_set_style_text_color(dl, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(dl, &lv_font_montserrat_16, 0);
    lv_obj_align(dl, LV_ALIGN_CENTER, 0, 0);

    /* BOOST button */
    lv_obj_t *boost = lv_btn_create(p);
    lv_obj_set_size(boost, LV_PCT(100), 44);
    lv_obj_set_style_bg_color(boost, lv_color_hex(t->boost_color), 0);
    lv_obj_set_style_bg_opa(boost, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(boost, 0, 0);
    lv_obj_set_style_radius(boost, 6, 0);
    lv_obj_align(boost, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *boost_ico_hw = lv_label_create(boost);
    lv_label_set_text(boost_ico_hw, ICON_BOOST);
    lv_obj_set_style_text_color(boost_ico_hw, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(boost_ico_hw, ICON_FONT_20 ? ICON_FONT_20 : &lv_font_montserrat_14, 0);
    lv_obj_align(boost_ico_hw, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_t *blbl = lv_label_create(boost);
    lv_label_set_text(blbl, "BOOST  30 min");
    lv_obj_set_style_text_color(blbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(blbl, &lv_font_montserrat_14, 0);
    lv_obj_align(blbl, LV_ALIGN_CENTER, 10, 0);
}

static void create_controls_pane(lv_obj_t *parent)
{
    const hestia_theme_t *t = ui_theme_get();

    lv_obj_t *pane = lv_obj_create(parent);
    lv_obj_set_size(pane, UI_CONTROLS_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(pane, lv_color_hex(t->bg), 0);
    lv_obj_set_style_bg_opa(pane, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pane, 0, 0);
    lv_obj_set_style_radius(pane, 0, 0);
    lv_obj_set_style_pad_left(pane, 0, 0);
    lv_obj_set_style_pad_right(pane, 0, 0);
    lv_obj_set_style_pad_bottom(pane, 0, 0);
    lv_obj_set_style_pad_top(pane, 0, 0);
    lv_obj_clear_flag(pane, LV_OBJ_FLAG_SCROLLABLE);

    /* Domain title header */
    lv_obj_t *hdr = lv_obj_create(pane);
    lv_obj_set_size(hdr, LV_PCT(100), 36);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(t->surface), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(hdr, lv_color_hex(t->border), LV_PART_MAIN);
    lv_obj_set_style_border_width(hdr, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, UI_PAD, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    s_ctrl_title = lv_label_create(hdr);
    int active_domain_bit = (s_active_domain < s_sidebar_domain_count) ?
        s_idx_to_domain[s_active_domain] : 0;
    lv_label_set_text(s_ctrl_title, ALL_DOMAIN_DISPLAY_NAMES[active_domain_bit]);
    lv_obj_set_style_text_color(s_ctrl_title, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(s_ctrl_title, &lv_font_montserrat_16, 0);
    lv_obj_align(s_ctrl_title, LV_ALIGN_LEFT_MID, 0, 0);

    s_ctrl_badge = ui_common_badge(hdr, "IDLE", t->inactive_color);
    lv_obj_align(s_ctrl_badge, LV_ALIGN_RIGHT_MID, 0, 0);  /* gear removed from here — see current pane */

    /* Content area (below header) */
    lv_obj_t *content = lv_obj_create(pane);
    lv_obj_set_size(content, LV_PCT(100), UI_SCREEN_H - 36);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(content, LV_ALIGN_TOP_LEFT, 0, 36);

    /* Create all domain panels in the content area */
    create_heating_panel(content);
    create_cooling_panel(content);
    create_fan_panel(content);
    create_humidity_panel(content);
    create_hotwater_panel(content);

    /* Hide all except the default active domain */
    for (int i = 0; i < s_sidebar_domain_count; i++) {
        if (i != (int)s_active_domain) {
            int domain_bit = s_idx_to_domain[i];
            if (domain_bit < MAX_SIDEBAR_DOMAINS && s_ctrl_panels[domain_bit]) {
                lv_obj_add_flag(s_ctrl_panels[domain_bit], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

/* ─── Domain selection ──────────────────────────────────────────── */

static void select_domain(ui_domain_t domain)
{
    const hestia_theme_t *t = ui_theme_get();

    /* Update active domain button highlight — flat style:
     *   active:   surface_variant bg + coloured left indicator bar shown
     *   inactive: surface bg + bar hidden */
    for (int i = 0; i < s_sidebar_domain_count; i++) {
        bool active = (i == (int)domain);
        if (s_domain_btn[i]) {
            lv_obj_set_style_bg_color(s_domain_btn[i],
                lv_color_hex(active ? t->surface_variant : t->surface), 0);
        }
        if (s_domain_bar[i]) {
            if (active) lv_obj_clear_flag(s_domain_bar[i], LV_OBJ_FLAG_HIDDEN);
            else        lv_obj_add_flag(s_domain_bar[i],   LV_OBJ_FLAG_HIDDEN);
        }
        if (s_domain_lbl[i]) {
            int domain_bit = s_idx_to_domain[i];
            lv_obj_set_style_text_color(s_domain_lbl[i],
                lv_color_hex(active ? get_domain_color(domain_bit) : t->text_secondary), 0);
        }
    }

    /* Swap content panels - domain is sidebar index, need to get domain bit */
    int selected_domain_bit = (domain < s_sidebar_domain_count) ? s_idx_to_domain[domain] : 0;
    for (int i = 0; i < 7; i++) {
        if (!s_ctrl_panels[i]) continue;
        if (i == selected_domain_bit) {
            lv_obj_clear_flag(s_ctrl_panels[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_ctrl_panels[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* Update title */
    if (s_ctrl_title) {
        lv_label_set_text(s_ctrl_title, ALL_DOMAIN_DISPLAY_NAMES[selected_domain_bit]);
    }

    s_active_domain = domain;
}

/* ─── Clock update ──────────────────────────────────────────────── */

static void update_clock(void)
{
    time_t now;
    struct tm tm_info;
    time(&now);
    localtime_r(&now, &tm_info);

    if (s_time_label) {
        char tbuf[16];
        snprintf(tbuf, sizeof(tbuf), "%02d:%02d", tm_info.tm_hour, tm_info.tm_min);
        lv_label_set_text(s_time_label, tbuf);
    }

    if (s_date_label) {
        static const char *DAYS[]   = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        static const char *MONTHS[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                        "Jul","Aug","Sep","Oct","Nov","Dec"};
        char dbuf[32];
        snprintf(dbuf, sizeof(dbuf), "%s, %d %s",
                 DAYS[tm_info.tm_wday],
                 tm_info.tm_mday,
                 MONTHS[tm_info.tm_mon]);
        lv_label_set_text(s_date_label, dbuf);
    }
}

/* ─── Event callbacks ───────────────────────────────────────────── */

static void on_domain_btn(lv_event_t *e)
{
    ui_domain_t domain = (ui_domain_t)(intptr_t)lv_event_get_user_data(e);
    select_domain(domain);
}

static void on_settings_btn(lv_event_t *e)
{
    (void)e;
    ui_settings_open();
}

static void on_heat_minus(lv_event_t *e)
{
    (void)e;
    temp_unit_t unit = user_settings_get_temp_unit();
    bool is_celsius = (unit == TEMP_UNIT_CELSIUS);

    if (is_celsius) {
        /* Celsius: apply 0.1°C step directly */
        s_heat_sp -= 0.1f;
        if (s_heat_sp < 5.0f) s_heat_sp = 5.0f;
    } else {
        /* Fahrenheit: convert to F, apply 0.5°F step, round, convert back */
        float temp_f = temp_c_to_f(s_heat_sp);
        temp_f -= 0.5f;
        if (temp_f < 41.0f) temp_f = 41.0f;  /* 5°C = 41°F */
        /* Round to nearest 0.5 */
        temp_f = roundf(temp_f * 2.0f) / 2.0f;
        s_heat_sp = temp_f_to_c(temp_f);
    }

    if (s_heat_sp_label) {
        float display_temp = is_celsius ? s_heat_sp : temp_c_to_f(s_heat_sp);
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", display_temp);
        lv_label_set_text(s_heat_sp_label, buf);
    }
}

static void on_heat_plus(lv_event_t *e)
{
    (void)e;
    temp_unit_t unit = user_settings_get_temp_unit();
    bool is_celsius = (unit == TEMP_UNIT_CELSIUS);

    if (is_celsius) {
        /* Celsius: apply 0.1°C step directly */
        s_heat_sp += 0.1f;
        if (s_heat_sp > 35.0f) s_heat_sp = 35.0f;
    } else {
        /* Fahrenheit: convert to F, apply 0.5°F step, round, convert back */
        float temp_f = temp_c_to_f(s_heat_sp);
        temp_f += 0.5f;
        if (temp_f > 95.0f) temp_f = 95.0f;  /* 35°C = 95°F */
        /* Round to nearest 0.5 */
        temp_f = roundf(temp_f * 2.0f) / 2.0f;
        s_heat_sp = temp_f_to_c(temp_f);
    }

    if (s_heat_sp_label) {
        float display_temp = is_celsius ? s_heat_sp : temp_c_to_f(s_heat_sp);
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", display_temp);
        lv_label_set_text(s_heat_sp_label, buf);
    }
}

static void on_cool_minus(lv_event_t *e)
{
    (void)e;
    temp_unit_t unit = user_settings_get_temp_unit();
    bool is_celsius = (unit == TEMP_UNIT_CELSIUS);

    if (is_celsius) {
        /* Celsius: apply 0.1°C step directly */
        s_cool_sp -= 0.1f;
        if (s_cool_sp < 15.0f) s_cool_sp = 15.0f;
    } else {
        /* Fahrenheit: convert to F, apply 0.5°F step, round, convert back */
        float temp_f = temp_c_to_f(s_cool_sp);
        temp_f -= 0.5f;
        if (temp_f < 59.0f) temp_f = 59.0f;  /* 15°C = 59°F */
        /* Round to nearest 0.5 */
        temp_f = roundf(temp_f * 2.0f) / 2.0f;
        s_cool_sp = temp_f_to_c(temp_f);
    }

    if (s_cool_sp_label) {
        float display_temp = is_celsius ? s_cool_sp : temp_c_to_f(s_cool_sp);
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", display_temp);
        lv_label_set_text(s_cool_sp_label, buf);
    }
}

static void on_cool_plus(lv_event_t *e)
{
    (void)e;
    temp_unit_t unit = user_settings_get_temp_unit();
    bool is_celsius = (unit == TEMP_UNIT_CELSIUS);

    if (is_celsius) {
        /* Celsius: apply 0.1°C step directly */
        s_cool_sp += 0.1f;
        if (s_cool_sp > 40.0f) s_cool_sp = 40.0f;
    } else {
        /* Fahrenheit: convert to F, apply 0.5°F step, round, convert back */
        float temp_f = temp_c_to_f(s_cool_sp);
        temp_f += 0.5f;
        if (temp_f > 104.0f) temp_f = 104.0f;  /* 40°C = 104°F */
        /* Round to nearest 0.5 */
        temp_f = roundf(temp_f * 2.0f) / 2.0f;
        s_cool_sp = temp_f_to_c(temp_f);
    }

    if (s_cool_sp_label) {
        float display_temp = is_celsius ? s_cool_sp : temp_c_to_f(s_cool_sp);
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", display_temp);
        lv_label_set_text(s_cool_sp_label, buf);
    }
}

static void on_eco_toggle(lv_event_t *e)
{
    (void)e;

    /* Read current settings */
    thermostat_settings_t therm_settings;
    if (thermostat_settings_get(&therm_settings) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get thermostat settings");
        return;
    }

    /* Toggle between comfort (0) and eco (1) */
    bool was_eco = (therm_settings.therm_mode == 1);
    therm_settings.therm_mode = was_eco ? 0 : 1;  /* Toggle: eco→comfort or comfort→eco */

    /* Save settings */
    if (thermostat_settings_save(&therm_settings) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save thermostat settings");
        return;
    }

    bool is_eco = (therm_settings.therm_mode == 1);
    ESP_LOGI(TAG, "Eco mode toggled to: %s", is_eco ? "ECO" : "COMFORT");

    /* Update icon color - no border/background (blends with pane) */
    const hestia_theme_t *theme = ui_theme_get();
    if (s_eco_btn) {
        /* Update icon color: green for eco, grey for comfort */
        lv_obj_t *icon = lv_obj_get_child(s_eco_btn, 0);
        if (icon) {
            lv_obj_set_style_text_color(icon,
                lv_color_hex(is_eco ? theme->success : theme->text_secondary), 0);
        }
    }

    /* TODO: Update setpoint displays if they show eco/comfort values */
    /* TODO: Update badge to show ECO status when active */
}

/* ─── Periodic timer ────────────────────────────────────────────── */

static void timer_cb(lv_timer_t *t)
{
    (void)t;
    update_clock();

    /* Update sensor display */
    if (s_sensor.has_data) {
        char buf[16];
        int ti = (int)s_sensor.temperature;
        int td = (int)((s_sensor.temperature - (float)ti) * 10);
        if (td < 0) td = -td;
        snprintf(buf, sizeof(buf), "%d.%d", ti, td);
        if (s_temp_label) lv_label_set_text(s_temp_label, buf);

        const char *unit_str = s_sensor.is_celsius ? "\xC2\xB0""C" : "\xC2\xB0""F";
        if (s_temp_unit) lv_label_set_text(s_temp_unit, unit_str);

        int hi = (int)s_sensor.humidity;
        snprintf(buf, sizeof(buf), "%d%%", hi);
        if (s_humi_label) lv_label_set_text(s_humi_label, buf);
    }
}

/* ─── Public API ────────────────────────────────────────────────── */

void ui_main_create(lv_obj_t *scr)
{
    ESP_LOGI(TAG, "Creating main 3-pane UI (landscape 480x320)");

    /* Initialize setpoints based on current temp unit */
    temp_unit_t unit = user_settings_get_temp_unit();
    bool is_celsius = (unit == TEMP_UNIT_CELSIUS);
    if (is_celsius) {
        s_heat_sp = 22.0f;  /* 22°C */
        s_cool_sp = 22.0f;
        ESP_LOGI(TAG, "Initialized setpoints: 22.0°C (step: 0.1°C)");
    } else {
        s_heat_sp = temp_f_to_c(72.0f);  /* 72°F = 22.222°C */
        s_cool_sp = temp_f_to_c(72.0f);
        ESP_LOGI(TAG, "Initialized setpoints: 72.0°F (%.3f°C, step: 0.5°F)", s_heat_sp);
    }

    s_scr = scr;
    ui_common_set_main_screen(scr);

    /* Apply theme to root screen */
    const hestia_theme_t *t = ui_theme_get();
    lv_obj_set_style_bg_color(scr, lv_color_hex(t->bg), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Horizontal flex root */
    lv_obj_t *root = lv_obj_create(scr);
    lv_obj_set_size(root, TFT_WIDTH, TFT_HEIGHT);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_pad_column(root, 0, 0);  /* remove LVGL default flex column gap */
    lv_obj_set_style_pad_row(root, 0, 0);     /* remove LVGL default flex row gap    */
    lv_obj_set_style_radius(root, 0, 0);
    lv_obj_align(root, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    create_sidebar(root);
    create_controls_pane(root);
    create_current_pane(root);

    /* Highlight initial active domain */
    select_domain(UI_DOMAIN_HEATING);

    /* 1-second timer for clock + sensor updates */
    lv_timer_create(timer_cb, 1000, NULL);

    /* Initial clock update */
    update_clock();
}

void ui_main_update_sensor(float temperature, float humidity, bool is_celsius)
{
    s_sensor.temperature = temperature;
    s_sensor.humidity    = humidity;
    s_sensor.is_celsius  = is_celsius;
    s_sensor.has_data    = true;
}

void ui_main_refresh_temp_unit(void)
{
    ESP_LOGI(TAG, "ui_main_refresh_temp_unit() called");

    /* Get current temp unit preference */
    temp_unit_t unit = user_settings_get_temp_unit();
    bool is_celsius = (unit == TEMP_UNIT_CELSIUS);

    ESP_LOGI(TAG, "New unit: %s, has_data=%d, cached_is_celsius=%d",
             is_celsius ? "Celsius" : "Fahrenheit", s_sensor.has_data, s_sensor.is_celsius);

    /* Reset setpoints to standard values based on unit */
    if (is_celsius) {
        s_heat_sp = 22.0f;  /* 22°C */
        s_cool_sp = 22.0f;
        ESP_LOGI(TAG, "Reset setpoints to 22.0°C (step: 0.1°C)");
    } else {
        /* Start at 72.0°F and convert to Celsius */
        s_heat_sp = temp_f_to_c(72.0f);  /* 72°F = 22.222°C */
        s_cool_sp = temp_f_to_c(72.0f);
        ESP_LOGI(TAG, "Reset setpoints to 72.0°F (%.3f°C, step: 0.5°F)", s_heat_sp);
    }

    /* If we have cached sensor data, convert temperature to new unit */
    if (s_sensor.has_data) {
        float old_temp = s_sensor.temperature;
        if (s_sensor.is_celsius && !is_celsius) {
            /* Convert C to F */
            s_sensor.temperature = temp_c_to_f(s_sensor.temperature);
            ESP_LOGI(TAG, "Converted %.1f°C to %.1f°F", old_temp, s_sensor.temperature);
        } else if (!s_sensor.is_celsius && is_celsius) {
            /* Convert F to C */
            s_sensor.temperature = temp_f_to_c(s_sensor.temperature);
            ESP_LOGI(TAG, "Converted %.1f°F to %.1f°C", old_temp, s_sensor.temperature);
        }
        s_sensor.is_celsius = is_celsius;
    }

    /* Force immediate display update */
    timer_cb(NULL);
    ESP_LOGI(TAG, "timer_cb() called to update display");

    /* Update setpoint labels with new values */
    if (s_heat_sp_label) {
        char buf[16];
        float display_temp = is_celsius ? s_heat_sp : temp_c_to_f(s_heat_sp);
        snprintf(buf, sizeof(buf), "%.1f", display_temp);
        lv_label_set_text(s_heat_sp_label, buf);
        ESP_LOGI(TAG, "Updated heat setpoint to %s%s", buf, is_celsius ? "°C" : "°F");
    }

    if (s_cool_sp_label) {
        char buf[16];
        float display_temp = is_celsius ? s_cool_sp : temp_c_to_f(s_cool_sp);
        snprintf(buf, sizeof(buf), "%.1f", display_temp);
        lv_label_set_text(s_cool_sp_label, buf);
        ESP_LOGI(TAG, "Updated cool setpoint to %s%s", buf, is_celsius ? "°C" : "°F");
    }
}

void ui_main_set_domain_status(ui_domain_t domain, ui_domain_status_t status)
{
    if (domain >= UI_DOMAIN_COUNT) return;
    if (!s_domain_dot[domain]) return;

    const hestia_theme_t *t = ui_theme_get();
    uint32_t color;

    switch (status) {
        case UI_STATUS_DISABLED:
            /* Show the dot in a very dim border colour instead of hiding it.
             * Hiding disabled-domain dots makes it look like only 3 domains
             * exist (e.g. the bottom 2 are simply gone). */
            color = t->border;
            break;
        case UI_STATUS_IDLE:
            color = t->inactive_color;
            break;
        case UI_STATUS_RUNNING:
            color = t->success;
            break;
        case UI_STATUS_BOOST:
            color = t->boost_color;
            break;
        default:
            color = t->inactive_color;
    }

    lv_obj_set_style_bg_color(s_domain_dot[domain], lv_color_hex(color), 0);
    /* Dots are always visible; disabled domains just use a dim border colour */
    lv_obj_clear_flag(s_domain_dot[domain], LV_OBJ_FLAG_HIDDEN);
}

void ui_main_activate_domain(ui_domain_t domain)
{
    if (domain < UI_DOMAIN_COUNT) {
        select_domain(domain);
    }
}

lv_obj_t *ui_main_get_screen(void)
{
    return s_scr;
}
