#include "ui/ui_settings_model.h"
#include "ui/ui_common.h"
#include "ui/ui_theme.h"
#include "core/user_settings.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ui_model";
static device_model_t s_selected = DEVICE_MODEL_HVAC;

static void on_back(lv_event_t *e) { (void)e; ui_common_pop_screen(); }

static void on_model_change_confirmed(lv_event_t *e)
{
    device_model_t model = (device_model_t)(intptr_t)lv_event_get_user_data(e);

    /* Close the confirmation dialog */
    lv_obj_t *dialog = lv_obj_get_parent(lv_obj_get_parent(lv_event_get_target(e)));
    lv_obj_del(dialog);

    device_config_t cfg;
    device_config_get(&cfg);
    cfg.model       = model;
    cfg.domain_mask = 0x00;
    /* Default relay assignments for the chosen model */
    for (int i = 0; i < RELAY_DOMAIN_COUNT; i++) cfg.relay_map[i] = RELAY_UNASSIGNED;
    if (model == DEVICE_MODEL_HVAC) {
        /* HVAC defaults: Heating S1→R0(W1), Cooling S1→R2(Y1), Fan→R4(G) */
        cfg.domain_mask       = (1<<0)|(1<<2)|(1<<4);
        cfg.relay_map[0]      = 2;   /* Heating S1 → relay 2 (W1) */
        cfg.relay_map[2]      = 0;   /* Cooling S1 → relay 0 (Y1) */
        cfg.relay_map[4]      = 4;   /* Fan        → relay 4 (G)  */
    } else {
        /* EU defaults: Heating S1→R0(L1), Fan→R1(L2) */
        cfg.domain_mask       = (1<<0)|(1<<4);
        cfg.relay_map[0]      = 0;
        cfg.relay_map[4]      = 1;
    }
    device_config_save(&cfg);

    /* Set temperature unit based on model */
    temp_unit_t temp_unit = (model == DEVICE_MODEL_HVAC) ? TEMP_UNIT_FAHRENHEIT : TEMP_UNIT_CELSIUS;
    esp_err_t temp_err = user_settings_set_temp_unit(temp_unit);
    ESP_LOGI(TAG, "Model changed to %s, setting temp unit to %s (result: %s)",
             model == DEVICE_MODEL_HVAC ? "HVAC" : "EU",
             model == DEVICE_MODEL_HVAC ? "Fahrenheit" : "Celsius",
             esp_err_to_name(temp_err));

    /* Delay and verify NVS write completed */
    vTaskDelay(pdMS_TO_TICKS(200));

    /* Verify the setting was saved */
    temp_unit_t verify = user_settings_get_temp_unit();
    ESP_LOGI(TAG, "Verification: temp unit is now %s (expected %s)",
             verify == TEMP_UNIT_FAHRENHEIT ? "Fahrenheit" : "Celsius",
             temp_unit == TEMP_UNIT_FAHRENHEIT ? "Fahrenheit" : "Celsius");

    ESP_LOGI(TAG, "Rebooting to apply model change...");
    /* Restart to apply model change */
    esp_restart();
}

static void on_model_select(lv_event_t *e)
{
    device_model_t model = (device_model_t)(intptr_t)lv_event_get_user_data(e);
    if (model == s_selected) { ui_common_pop_screen(); return; }

    const hestia_theme_t *t = ui_theme_get();

    /* Create modal dialog for confirmation */
    lv_obj_t *dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(dialog, 380, 200);
    lv_obj_center(dialog);
    lv_obj_set_style_bg_color(dialog, lv_color_hex(t->surface), 0);
    lv_obj_set_style_border_width(dialog, 2, 0);
    lv_obj_set_style_border_color(dialog, lv_color_hex(t->warning), 0);
    lv_obj_set_style_radius(dialog, 12, 0);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lv_obj_t *title = lv_label_create(dialog);
    lv_label_set_text(title, "Change Device Model?");
    lv_obj_set_style_text_color(title, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    /* Message */
    lv_obj_t *msg = lv_label_create(dialog);
    const char *model_name = (model == DEVICE_MODEL_HVAC) ? "HVAC" : "EU";
    lv_label_set_text_fmt(msg, "Switching to %s model will reset\nall domain assignments and\nrestart the device.", model_name);
    lv_obj_set_style_text_color(msg, lv_color_hex(t->text_secondary), 0);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, -20);

    /* Button row */
    lv_obj_t *btn_row = lv_obj_create(dialog);
    lv_obj_set_size(btn_row, LV_PCT(90), 50);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(btn_row, LV_ALIGN_BOTTOM_MID, 0, -10);

    /* Cancel button */
    lv_obj_t *cancel_btn = lv_btn_create(btn_row);
    lv_obj_set_size(cancel_btn, 140, 45);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(t->surface_variant), 0);
    lv_obj_add_event_cb(cancel_btn, [](lv_event_t *e) {
        lv_obj_t *dialog = lv_obj_get_parent(lv_obj_get_parent(lv_event_get_target(e)));
        lv_obj_del(dialog);
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cancel_lbl = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_lbl, "Cancel");
    lv_obj_center(cancel_lbl);

    /* Confirm button */
    lv_obj_t *confirm_btn = lv_btn_create(btn_row);
    lv_obj_set_size(confirm_btn, 140, 45);
    lv_obj_set_style_bg_color(confirm_btn, lv_color_hex(t->warning), 0);
    lv_obj_add_event_cb(confirm_btn, on_model_change_confirmed, LV_EVENT_CLICKED, (void *)(intptr_t)model);

    lv_obj_t *confirm_lbl = lv_label_create(confirm_btn);
    lv_label_set_text(confirm_lbl, "Change & Restart");
    lv_obj_set_style_text_color(confirm_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(confirm_lbl);
}

void ui_settings_model_open(void)
{
    device_config_t cfg;
    device_config_get(&cfg);
    s_selected = cfg.model;

    const hestia_theme_t *t = ui_theme_get();

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(scr, lv_color_hex(t->bg), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    ui_common_header(scr, "Device Model", on_back, NULL);

    /* Warning note */
    lv_obj_t *warn = lv_label_create(scr);
    lv_label_set_text(warn, LV_SYMBOL_WARNING
                      "  Changing model resets domain assignments and restarts.");
    lv_obj_set_style_text_color(warn, lv_color_hex(t->warning), 0);
    lv_obj_set_style_text_font(warn, &lv_font_montserrat_14, 0);
    lv_obj_set_width(warn, UI_SCREEN_W - UI_PAD * 2);
    lv_label_set_long_mode(warn, LV_LABEL_LONG_WRAP);
    lv_obj_align(warn, LV_ALIGN_TOP_LEFT, UI_PAD, UI_HEADER_H + UI_PAD);

    /* Model cards */
    struct {
        device_model_t  id;
        const char     *name;
        const char     *desc;
        const char     *terminals;
    } models[] = {
        { DEVICE_MODEL_HVAC, "HVAC",
          "North American HVAC systems\n(heat pumps, furnaces, A/C)",
          "Common: R (24VAC)\nOutputs: Y1, Y2, W1, W2, G" },
        { DEVICE_MODEL_EU, "EU",
          "European mains-wiring systems\n(boilers, zone valves, fans)",
          "Common: N (neutral)\nOutputs: L1, L2, L3, L4, L5" },
    };

    const int card_y = UI_HEADER_H + 50;
    const int card_h = UI_SCREEN_H - card_y - UI_PAD;
    const int card_w = (UI_SCREEN_W - UI_PAD * 3) / 2;

    for (int i = 0; i < 2; i++) {
        int cx = UI_PAD + i * (card_w + UI_PAD);

        lv_obj_t *card = lv_obj_create(scr);
        lv_obj_set_size(card, card_w, card_h);
        lv_obj_set_pos(card, cx, card_y);
        bool active = (models[i].id == s_selected);
        lv_obj_set_style_bg_color(card, lv_color_hex(t->surface), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(card,
            lv_color_hex(active ? t->primary : t->border), 0);
        lv_obj_set_style_border_width(card, active ? 2 : 1, 0);
        lv_obj_set_style_radius(card, 10, 0);
        lv_obj_set_style_pad_all(card, UI_PAD, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(card, lv_color_hex(t->surface_variant), LV_STATE_PRESSED);
        lv_obj_add_event_cb(card, on_model_select, LV_EVENT_CLICKED,
                            (void *)(intptr_t)models[i].id);

        /* Title */
        lv_obj_t *name = lv_label_create(card);
        lv_label_set_text(name, models[i].name);
        lv_obj_set_style_text_color(name, lv_color_hex(active ? t->primary : t->text), 0);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_20, 0);
        lv_obj_align(name, LV_ALIGN_TOP_MID, 0, 0);

        /* Description */
        lv_obj_t *desc = lv_label_create(card);
        lv_label_set_text(desc, models[i].desc);
        lv_obj_set_style_text_color(desc, lv_color_hex(t->text_secondary), 0);
        lv_obj_set_style_text_font(desc, &lv_font_montserrat_14, 0);
        lv_obj_set_width(desc, card_w - UI_PAD * 2);
        lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);
        lv_obj_align(desc, LV_ALIGN_TOP_MID, 0, 30);

        /* Terminal names */
        lv_obj_t *term = lv_label_create(card);
        lv_label_set_text(term, models[i].terminals);
        lv_obj_set_style_text_color(term, lv_color_hex(t->accent), 0);
        lv_obj_set_style_text_font(term, &lv_font_montserrat_14, 0);
        lv_obj_set_width(term, card_w - UI_PAD * 2);
        lv_label_set_long_mode(term, LV_LABEL_LONG_WRAP);
        lv_obj_align(term, LV_ALIGN_BOTTOM_MID, 0, 0);
    }

    ui_common_push_screen(scr);
}
