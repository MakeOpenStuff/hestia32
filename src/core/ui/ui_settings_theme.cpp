#include "ui/ui_settings_theme.h"
#include "ui/ui_common.h"
#include "ui/ui_theme.h"
#include "core/user_settings.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ui_theme_cfg";
static hestia_theme_id_t s_selected     = HESTIA_THEME_DARK;
static hestia_theme_id_t s_pending_theme = HESTIA_THEME_DARK;

static void on_back(lv_event_t *e)  { (void)e; ui_common_pop_screen(); }

/* Confirmation msgbox callback: Apply & Restart or Cancel */
static void on_confirm_restart(lv_event_t *e)
{
    lv_obj_t *mbox = lv_event_get_current_target(e);
    uint16_t  btn  = lv_msgbox_get_active_btn(mbox);
    if (btn == 0) {  /* "Apply & Restart" */
        device_config_t cfg;
        device_config_get(&cfg);
        cfg.theme = s_pending_theme;
        device_config_save(&cfg);
        ESP_LOGI(TAG, "Theme %d saved, restarting", (int)s_pending_theme);
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_restart();
    }
    /* "Cancel": close the dialog, leave theme unchanged */
    lv_msgbox_close(mbox);
}

static void on_select(lv_event_t *e)
{
    s_pending_theme = (hestia_theme_id_t)(intptr_t)lv_event_get_user_data(e);
    const hestia_theme_t *t = ui_theme_get();

    static const char *btns[] = {"Yes", "Cancel", ""};
    lv_obj_t *mbox = lv_msgbox_create(NULL,
        "Restart Required",
        "Restart to apply this theme?",
        btns, false);

    /* Style the dialog with the current theme so it doesn't flash default colours */
    lv_obj_t *bg = lv_obj_get_parent(mbox);
    if (bg) {
        lv_obj_set_style_bg_color(bg, lv_color_hex(t->bg), 0);
        lv_obj_set_style_bg_opa(bg, LV_OPA_60, 0);
    }
    lv_obj_set_style_bg_color(mbox, lv_color_hex(t->surface), 0);
    lv_obj_set_style_bg_opa(mbox, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(mbox, lv_color_hex(t->border), 0);
    lv_obj_set_style_border_width(mbox, 1, 0);
    lv_obj_set_style_text_color(mbox, lv_color_hex(t->text), 0);

    lv_obj_add_event_cb(mbox, on_confirm_restart, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_center(mbox);
}

/* Draw a mini preview swatch block */
static void draw_swatch(lv_obj_t *parent, const hestia_theme_t *th, int x, int y, int w, int h)
{
    lv_obj_t *outer = lv_obj_create(parent);
    lv_obj_set_size(outer, w, h);
    lv_obj_set_pos(outer, x, y);
    lv_obj_set_style_bg_color(outer, lv_color_hex(th->bg), 0);
    lv_obj_set_style_bg_opa(outer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(outer, lv_color_hex(0x707070), 0);  /* neutral gray — always visible on any card bg */
    lv_obj_set_style_border_width(outer, 2, 0);
    lv_obj_set_style_radius(outer, 6, 0);
    lv_obj_set_style_pad_all(outer, 4, 0);
    lv_obj_clear_flag(outer, LV_OBJ_FLAG_SCROLLABLE);

    /* Surface bar */
    lv_obj_t *surf = lv_obj_create(outer);
    lv_obj_set_size(surf, LV_PCT(100), 18);
    lv_obj_set_style_bg_color(surf, lv_color_hex(th->surface), 0);
    lv_obj_set_style_bg_opa(surf, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(surf, 0, 0);
    lv_obj_set_style_radius(surf, 3, 0);
    lv_obj_align(surf, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Primary accent dot */
    lv_obj_t *dot = lv_obj_create(outer);
    lv_obj_set_size(dot, 14, 14);
    lv_obj_set_style_bg_color(dot, lv_color_hex(th->primary), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(dot, LV_ALIGN_BOTTOM_LEFT, 2, -2);

    /* Text sample */
    lv_obj_t *txt = lv_label_create(outer);
    lv_label_set_text(txt, "Aa");
    lv_obj_set_style_text_color(txt, lv_color_hex(th->text), 0);
    lv_obj_set_style_text_font(txt, &lv_font_montserrat_14, 0);
    lv_obj_align(txt, LV_ALIGN_BOTTOM_RIGHT, -2, -2);
}

void ui_settings_theme_open(void)
{
    device_config_t cfg;
    device_config_get(&cfg);
    s_selected = cfg.theme;

    const hestia_theme_t *t = ui_theme_get();

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(scr, lv_color_hex(t->bg), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    ui_common_header(scr, "Theme", on_back, NULL);

    /* Theme cards.  "Dark" uses HESTIA_THEME_LIGHT_DEF and "Light" uses
     * HESTIA_THEME_DARK_DEF — names reflect the user's visual perception of
     * what each setting looks like on this specific display. */
    struct { hestia_theme_id_t id; const char *name; const hestia_theme_t *def; } themes[] = {
        { HESTIA_THEME_LIGHT,  "Dark",   &HESTIA_THEME_LIGHT_DEF },
        { HESTIA_THEME_DARK,   "Light",  &HESTIA_THEME_DARK_DEF  },
        { HESTIA_THEME_CUSTOM, "Custom", NULL                     },
    };

    const int card_w = (UI_SCREEN_W - UI_PAD * 4) / 3;
    const int card_h = UI_SCREEN_H - UI_HEADER_H - UI_PAD * 2;
    const int card_y = UI_HEADER_H + UI_PAD;

    for (int i = 0; i < 3; i++) {
        bool is_selected = (themes[i].id == s_selected);
        const hestia_theme_t *th = themes[i].def ? themes[i].def : ui_theme_get();
        int card_x = UI_PAD + i * (card_w + UI_PAD);

        /* The card IS the swatch: its background is the theme's own bg colour.
         * No child swatch object — child lv_obj widgets absorb clicks even when
         * not explicitly clickable, which made only the gap at the bottom work. */
        lv_obj_t *card = lv_obj_create(scr);
        lv_obj_set_size(card, card_w, card_h);
        lv_obj_set_pos(card, card_x, card_y);
        lv_obj_set_style_bg_color(card, lv_color_hex(th->bg), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(card,
            lv_color_hex(is_selected ? t->primary : (uint32_t)0x707070), 0);
        lv_obj_set_style_border_width(card, is_selected ? 3 : 1, 0);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_pad_all(card, UI_PAD, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        if (!is_selected) {
            /* Non-selected: clickable, subtle press feedback */
            lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(card,
                lv_color_hex(th->surface), LV_STATE_PRESSED);
            lv_obj_add_event_cb(card, on_select, LV_EVENT_CLICKED,
                                (void *)(intptr_t)themes[i].id);
        } else {
            /* Selected: NOT clickable — cannot re-apply current theme */
            lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);
        }

        /* Theme name */
        lv_obj_t *name = lv_label_create(card);
        lv_label_set_text(name, themes[i].name);
        lv_obj_set_style_text_color(name, lv_color_hex(th->text), 0);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
        lv_obj_align(name, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_clear_flag(name, LV_OBJ_FLAG_CLICKABLE);

        /* Separator line below the name */
        lv_obj_t *sep = lv_obj_create(card);
        lv_obj_set_size(sep, LV_PCT(100), 1);
        lv_obj_set_style_bg_color(sep, lv_color_hex(th->border), 0);
        lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(sep, 0, 0);
        lv_obj_set_style_radius(sep, 0, 0);
        lv_obj_set_style_pad_all(sep, 0, 0);
        lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, 18);

        /* Surface bar — small preview strip below the name */
        lv_obj_t *surf = lv_obj_create(card);
        lv_obj_set_size(surf, LV_PCT(100), 14);
        lv_obj_set_style_bg_color(surf, lv_color_hex(th->surface), 0);
        lv_obj_set_style_bg_opa(surf, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(surf, 0, 0);
        lv_obj_set_style_radius(surf, 3, 0);
        lv_obj_set_style_pad_all(surf, 0, 0);
        lv_obj_clear_flag(surf, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(surf, LV_ALIGN_TOP_MID, 0, 20);

        /* Primary accent dot */
        lv_obj_t *accent = lv_obj_create(card);
        lv_obj_set_size(accent, 14, 14);
        lv_obj_set_style_bg_color(accent, lv_color_hex(th->primary), 0);
        lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(accent, 0, 0);
        lv_obj_set_style_radius(accent, LV_RADIUS_CIRCLE, 0);
        lv_obj_clear_flag(accent, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(accent, LV_ALIGN_BOTTOM_LEFT, 2, -4);

        /* Large centred checkmark on the selected card */
        if (is_selected) {
            lv_obj_t *check = lv_label_create(card);
            lv_label_set_text(check, LV_SYMBOL_OK);
            lv_obj_set_style_text_color(check, lv_color_hex(t->primary), 0);
            lv_obj_set_style_text_font(check, &lv_font_montserrat_36, 0);
            lv_obj_align(check, LV_ALIGN_CENTER, 0, 0);
            lv_obj_clear_flag(check, LV_OBJ_FLAG_CLICKABLE);
        }
    }

    ui_common_push_screen(scr);
}

