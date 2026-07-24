#include "ui/ui_settings.h"
#include "ui/ui_common.h"
#include "ui/ui_theme.h"
#include "ui/ui_icons.h"
#include "ui/ui_settings_domains.h"
#include "ui/ui_settings_thermostat.h"
#include "ui/ui_settings_display.h"
#include "ui/ui_settings_protocol.h"
#include "ui/ui_settings_datetime.h"
#include "ui/ui_settings_locale.h"
#include "ui/ui_settings_theme.h"
#include "ui/ui_settings_model.h"
#include "ui/ui_settings_system.h"
#include "ui/ui_color_test.h"
#include "esp_log.h"

static const char *TAG = "ui_settings";

/* ─── Settings tile descriptors ───────────────────────────────── */
typedef struct {
    const char *icon;
    const char *label;
    void (*open_fn)(void);
} settings_tile_t;

static void open_domains(void)     { ui_settings_domains_open(); }
static void open_thermostat(void)  { ui_settings_thermostat_open(); }
static void open_display(void)     { ui_settings_display_open(); }
static void open_protocol(void)    { ui_settings_protocol_open(); }
static void open_datetime(void)    { ui_settings_datetime_open(); }
static void open_locale(void)      { ui_settings_locale_open(); }
static void open_theme(void)       { ui_settings_theme_open(); }
static void open_model(void)       { ui_settings_model_open(); }
static void open_system(void)      { ui_settings_system_open(); }
static void open_color_test(void)  { ui_color_test_open(); }

static const settings_tile_t TILES[] = {
    { ICON_WIRING,    "Domains",       open_domains    },
    { ICON_THERMOSTAT,"Thermostat",    open_thermostat },
    { ICON_BRIGHTNESS,"Display",       open_display    },
    { ICON_PROTOCOL,  "Connectivity",  open_protocol   },
    { ICON_GLOBE,     "Locale",        open_locale     },
    { ICON_THEME,     "Theme",         open_theme      },
    { ICON_MODEL,     "Model",         open_model      },
    { ICON_WRENCH,    "System",        open_system     },
    // { ICON_WARNING,   "Color Test",    open_color_test }, // Disabled - for color troubleshooting
};
#define TILE_COUNT  ((int)(sizeof(TILES) / sizeof(TILES[0])))

/* ─── Back button handler ──────────────────────────────────────── */
static void on_back(lv_event_t *e)
{
    (void)e;
    ui_common_pop_screen();
}

/* ─── Tile tap handler ─────────────────────────────────────────── */
static void on_tile(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < TILE_COUNT && TILES[idx].open_fn) {
        TILES[idx].open_fn();
    }
}

/* ─── Open settings hub ────────────────────────────────────────── */
void ui_settings_open(void)
{
    ESP_LOGI(TAG, "Opening settings hub");
    const hestia_theme_t *t = ui_theme_get();

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(scr, lv_color_hex(t->bg), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Header */
    ui_common_header(scr, "Settings", on_back, NULL);

    /* Tile grid — 2 rows × 4 cols (8 tiles, all cells filled) */
    const int COLS    = 4;
    const int tile_w  = (UI_SCREEN_W - UI_PAD * 2) / COLS;
    const int tile_h  = (UI_SCREEN_H - UI_HEADER_H - UI_PAD * 2) / 2;
    const int grid_y  = UI_HEADER_H + UI_PAD;

    for (int i = 0; i < TILE_COUNT; i++) {
        int col = i % COLS;
        int row = i / COLS;
        int x   = UI_PAD + col * tile_w;
        int y   = grid_y + row * tile_h;

        lv_obj_t *tile = lv_obj_create(scr);
        lv_obj_set_size(tile, tile_w - UI_PAD_SM, tile_h - UI_PAD_SM);
        lv_obj_set_pos(tile, x, y);
        lv_obj_set_style_bg_color(tile, lv_color_hex(t->surface), 0);
        lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(tile, lv_color_hex(t->border), 0);
        lv_obj_set_style_border_width(tile, 1, 0);
        lv_obj_set_style_radius(tile, 8, 0);
        lv_obj_set_style_pad_all(tile, UI_PAD, 0);
        lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(tile, on_tile, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        /* Pressed state */
        lv_obj_set_style_bg_color(tile, lv_color_hex(t->surface_variant),
                                   LV_STATE_PRESSED);

        /* Icon */
        lv_obj_t *icon_lbl = lv_label_create(tile);
        lv_label_set_text(icon_lbl, TILES[i].icon);
        lv_obj_set_style_text_color(icon_lbl, lv_color_hex(t->primary), 0);
        lv_obj_set_style_text_font(icon_lbl, ICON_FONT_48 ? ICON_FONT_48 : &lv_font_montserrat_48, 0);
        lv_obj_align(icon_lbl, LV_ALIGN_CENTER, 0, -12);

        /* Label */
        lv_obj_t *name_lbl = lv_label_create(tile);
        lv_label_set_text(name_lbl, TILES[i].label);
        lv_obj_set_style_text_color(name_lbl, lv_color_hex(t->text), 0);
        lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_14, 0);
        lv_obj_align(name_lbl, LV_ALIGN_BOTTOM_MID, 0, -4);
    }

    ui_common_push_screen(scr);
}

void ui_settings_back(void)
{
    ui_common_pop_screen();
}
