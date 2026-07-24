#include "ui/ui_settings_domains.h"
#include "ui/ui_common.h"
#include "ui/ui_theme.h"
#include "ui/ui_icons.h"
#include "ui/ui_main.h"
#include "core/user_settings.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ui_domains";

/* ─── Domain tile descriptors ───────────────────────────────────── */
typedef struct {
    const char *name;
    const char *icon;
    uint8_t     mask_bit;       /* bit in domain_mask         */
    int         prerequisite;   /* domain idx that must be on (-1 = none) */
    const char *hvac_term;      /* HVAC terminal name (Y1, W1, etc.) */
} domain_tile_t;

#define DOM_HEAT_S1  0
#define DOM_HEAT_S2  1
#define DOM_COOL_S1  2
#define DOM_COOL_S2  3
#define DOM_FAN      4
#define DOM_HUMIDITY 5
#define DOM_HOTWATER 6
#define DOM_REVERSING 7
#define DOM_COUNT    8

static const domain_tile_t DOMAIN_TILES[DOM_COUNT] = {
    { "Heat Stage 1",    ICON_HEATING,   (1<<0), -1,          "W1" },
    { "Heat Stage 2",    ICON_HEATING,   (1<<1), DOM_HEAT_S1, "W2" },
    { "Cool Stage 1",    ICON_COOLING,   (1<<2), -1,          "Y1" },
    { "Cool Stage 2",    ICON_COOLING,   (1<<3), DOM_COOL_S1, "Y2" },
    { "Fan",        ICON_FAN,       (1<<4), -1,          "G"  },
    { "Humidity",   ICON_HUMIDITY,  (1<<5), -1,          "H"  },
    { "Hot Water",  ICON_HOT_WATER, (1<<6), -1,          "HW" },
    { "Rev Valve",  ICON_REVERSING, (1<<7), -1,          "O/B" },
};

/* Tile UI state */
static lv_obj_t  *s_tiles[DOM_COUNT];
static lv_obj_t  *s_relay_labels[DOM_COUNT];
static uint8_t    s_domain_mask    = 0x00;
static uint8_t    s_relay_assign[DOM_COUNT];   /* relay number 0-4, 0xFF=unassigned */
static device_model_t s_model;

static int count_bits(uint8_t v)
{
    int n = 0;
    for (; v; v >>= 1) n += (v & 1);
    return n;
}

/* ─── Relay label text (model-aware) ───────────────────────────── */
static void update_relay_label(int idx)
{
    if (!s_relay_labels[idx]) return;
    uint8_t r = s_relay_assign[idx];
    if (r > 4) {
        lv_label_set_text(s_relay_labels[idx], "-");
    } else {
        char buf[8];
        if (s_model == DEVICE_MODEL_HVAC) {
            /* HVAC: Show terminal name (Y1, W1, G, etc.) */
            snprintf(buf, sizeof(buf), "%s", DOMAIN_TILES[idx].hvac_term);
        } else {
            /* EU: Show relay number (L1-L5) */
            snprintf(buf, sizeof(buf), "L%u", (unsigned)(r + 1));
        }
        lv_label_set_text(s_relay_labels[idx], buf);
    }
}

/* ─── Toggle callback ──────────────────────────────────────────── */
static void on_tile_click(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    bool currently_enabled = (s_domain_mask & DOMAIN_TILES[idx].mask_bit);

    /* Toggle state */
    if (!currently_enabled) {
        /* Enabling: check prerequisite */
        int prereq = DOMAIN_TILES[idx].prerequisite;
        if (prereq >= 0 && !(s_domain_mask & DOMAIN_TILES[prereq].mask_bit)) {
            ESP_LOGW(TAG, "Must enable %s first", DOMAIN_TILES[prereq].name);
            return;
        }
        /* 5-relay cap */
        if (count_bits(s_domain_mask | DOMAIN_TILES[idx].mask_bit) > 5) {
            /* Show friendly popup */
            const hestia_theme_t *t = ui_theme_get();
            lv_obj_t *blocker = lv_obj_create(lv_scr_act());
            lv_obj_set_size(blocker, LV_PCT(100), LV_PCT(100));
            lv_obj_set_style_bg_opa(blocker, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(blocker, 0, 0);
            lv_obj_set_style_radius(blocker, 0, 0);
            lv_obj_clear_flag(blocker, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t *popup = lv_obj_create(blocker);
            lv_obj_set_size(popup, 320, 140);
            lv_obj_set_style_bg_color(popup, lv_color_hex(t->surface), 0);
            lv_obj_set_style_bg_opa(popup, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(popup, 2, 0);
            lv_obj_set_style_border_color(popup, lv_color_hex(t->primary), 0);
            lv_obj_set_style_radius(popup, 12, 0);
            lv_obj_set_style_shadow_width(popup, 20, 0);
            lv_obj_set_style_shadow_opa(popup, LV_OPA_50, 0);
            lv_obj_align(popup, LV_ALIGN_CENTER, 0, 0);
            lv_obj_add_flag(blocker, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(blocker, [](lv_event_t *e) { lv_obj_del((lv_obj_t*)lv_event_get_target(e)); }, LV_EVENT_CLICKED, NULL);

            lv_obj_t *icon = lv_label_create(popup);
            lv_label_set_text(icon, ICON_WARNING);
            lv_obj_set_style_text_color(icon, lv_color_hex(0xFFAA00), 0);
            lv_obj_set_style_text_font(icon, ICON_FONT_28 ? ICON_FONT_28 : &lv_font_montserrat_24, 0);
            lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 0);

            lv_obj_t *msg = lv_label_create(popup);
            lv_label_set_text(msg, "Your model supports\nup to 5 domains only.");
            lv_obj_set_style_text_color(msg, lv_color_hex(t->text), 0);
            lv_obj_set_style_text_font(msg, &lv_font_montserrat_16, 0);
            lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(msg, LV_ALIGN_CENTER, 0, 10);

            // lv_obj_t *hint = lv_label_create(popup);
            // lv_label_set_text(hint, "Tap anywhere to close");
            // lv_obj_set_style_text_color(hint, lv_color_hex(t->text_secondary), 0);
            // lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
            // lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -12);

            ESP_LOGW(TAG, "Max 5 relays; cannot enable %s", DOMAIN_TILES[idx].name);
            return;
        }
        s_domain_mask |= DOMAIN_TILES[idx].mask_bit;
        /* Auto-assign next free relay */
        for (uint8_t r = 0; r < 5; r++) {
            bool used = false;
            for (int j = 0; j < DOM_COUNT; j++) {
                if (s_relay_assign[j] == r) { used = true; break; }
            }
            if (!used) { s_relay_assign[idx] = r; break; }
        }
    } else {
        /* Disabling: cascade to dependencies */
        s_domain_mask &= ~DOMAIN_TILES[idx].mask_bit;
        for (int j = 0; j < DOM_COUNT; j++) {
            if (DOMAIN_TILES[j].prerequisite == idx &&
                (s_domain_mask & DOMAIN_TILES[j].mask_bit)) {
                s_domain_mask &= ~DOMAIN_TILES[j].mask_bit;
                s_relay_assign[j] = 0xFF;
                update_relay_label(j);
            }
        }
        s_relay_assign[idx] = 0xFF;
    }
    update_relay_label(idx);

    /* Update tile visual state */
    const hestia_theme_t *t = ui_theme_get();
    bool enabled = (s_domain_mask & DOMAIN_TILES[idx].mask_bit);
    lv_obj_set_style_bg_color(s_tiles[idx],
        lv_color_hex(enabled ? t->primary : t->surface), 0);
    lv_obj_set_style_border_color(s_tiles[idx],
        lv_color_hex(enabled ? t->primary : t->border), 0);

    /* Update text colors - icon, name, and relay label are children 0, 1, 2 */
    lv_obj_t *icon = lv_obj_get_child(s_tiles[idx], 0);
    lv_obj_t *name = lv_obj_get_child(s_tiles[idx], 1);
    lv_obj_t *relay = lv_obj_get_child(s_tiles[idx], 2);
    if (icon) {
        lv_obj_set_style_text_color(icon,
            lv_color_hex(enabled ? 0xFFFFFF : t->text_secondary), 0);
    }
    if (name) {
        lv_obj_set_style_text_color(name,
            lv_color_hex(enabled ? 0xFFFFFF : t->text), 0);
    }
    if (relay) {
        lv_obj_set_style_text_color(relay,
            lv_color_hex(enabled ? 0xFFDD00 : t->text_secondary), 0);
    }
}

/* ─── Save callback ─────────────────────────────────────────────── */
static void on_save(lv_event_t *e)
{
    (void)e;
    device_config_t cfg;
    device_config_get(&cfg);
    cfg.domain_mask = s_domain_mask;
    memcpy(cfg.relay_map, s_relay_assign, sizeof(s_relay_assign));
    device_config_save(&cfg);
    ESP_LOGI(TAG, "Domain config saved: mask=0x%02X", s_domain_mask);

    /* Notify main UI to reload sidebar */
    ui_main_reload_sidebar();

    ui_common_pop_screen();
}

static void on_back(lv_event_t *e) { (void)e; ui_common_pop_screen(); }

/* ─── Open ──────────────────────────────────────────────────────── */
void ui_settings_domains_open(void)
{
    const hestia_theme_t *t = ui_theme_get();

    /* Load current config */
    device_config_t cfg;
    device_config_get(&cfg);
    s_domain_mask = cfg.domain_mask;
    s_model = cfg.model;
    memcpy(s_relay_assign, cfg.relay_map, sizeof(s_relay_assign));

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(scr, lv_color_hex(t->bg), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    ui_common_header(scr, "Domain Configuration", on_back, NULL);

    /* Grid body - 4 tiles top row, 3 tiles bottom row */
    lv_obj_t *body = lv_obj_create(scr);
    lv_obj_set_size(body, UI_SCREEN_W, UI_SCREEN_H - UI_HEADER_H - 60);
    lv_obj_align(body, LV_ALIGN_TOP_LEFT, 0, UI_HEADER_H);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_all(body, UI_PAD, 0);
    lv_obj_set_style_radius(body, 0, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    const int tile_w = (UI_SCREEN_W - UI_PAD * 5) / 4;  /* 4 tiles per row */
    const int tile_h = (UI_SCREEN_H - UI_HEADER_H - 60 - UI_PAD * 3) / 2;  /* 2 rows */

    for (int i = 0; i < DOM_COUNT; i++) {
        int row = (i < 4) ? 0 : 1;
        int col = (i < 4) ? i : (i - 4);
        int x = col * (tile_w + UI_PAD);
        int y = row * (tile_h + UI_PAD);

        bool enabled = (s_domain_mask & DOMAIN_TILES[i].mask_bit);

        lv_obj_t *tile = lv_obj_create(body);
        lv_obj_set_size(tile, tile_w, tile_h);
        lv_obj_set_pos(tile, x, y);
        lv_obj_set_style_bg_color(tile, lv_color_hex(enabled ? t->primary : t->surface), 0);
        lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(tile, 2, 0);
        lv_obj_set_style_border_color(tile, lv_color_hex(enabled ? t->primary : t->border), 0);
        lv_obj_set_style_radius(tile, 8, 0);
        lv_obj_set_style_pad_all(tile, 6, 0);
        lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(tile, on_tile_click, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        s_tiles[i] = tile;

        /* Icon */
        lv_obj_t *icon = lv_label_create(tile);
        lv_label_set_text(icon, DOMAIN_TILES[i].icon);
        lv_obj_set_style_text_color(icon,
            lv_color_hex(enabled ? 0xFFFFFF : t->text_secondary), 0);
        lv_obj_set_style_text_font(icon, ICON_FONT_20 ? ICON_FONT_20 : &lv_font_montserrat_14, 0);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 2);

        /* Name */
        lv_obj_t *name = lv_label_create(tile);
        lv_label_set_text(name, DOMAIN_TILES[i].name);
        lv_obj_set_style_text_color(name,
            lv_color_hex(enabled ? 0xFFFFFF : t->text), 0);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
        lv_obj_set_width(name, tile_w - 12);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_align(name, LV_ALIGN_TOP_MID, 0, 30);

        /* Relay label */
        lv_obj_t *rl = lv_label_create(tile);
        lv_obj_set_style_text_color(rl,
            lv_color_hex(enabled ? 0xFFDD00 : t->text_secondary), 0);
        lv_obj_set_style_text_font(rl, &lv_font_montserrat_16, 0);
        lv_obj_align(rl, LV_ALIGN_BOTTOM_MID, 0, 0);
        s_relay_labels[i] = rl;
        update_relay_label(i);
    }

    /* Save button */
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
