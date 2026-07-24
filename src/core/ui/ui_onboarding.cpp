#include "ui/ui_onboarding.h"
#include "ui/ui_common.h"
#include "ui/ui_theme.h"
#include "core/user_settings.h"
#include "core/display_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "ui_onboarding";
#define NVS_NAMESPACE "onboarding"
#define NVS_KEY_COMPLETE "complete"

static device_model_t s_selected_model = DEVICE_MODEL_HVAC;
static temp_unit_t s_selected_unit = TEMP_UNIT_CELSIUS;
static lv_obj_t *s_model_hvac_btn = NULL;
static lv_obj_t *s_model_eu_btn = NULL;
static lv_obj_t *s_temp_f_btn = NULL;
static lv_obj_t *s_temp_c_btn = NULL;
static SemaphoreHandle_t s_continue_sem = NULL;

/* Check if onboarding has been completed */
bool ui_onboarding_needed(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Onboarding namespace not found, onboarding needed");
        return true;
    }

    uint8_t complete = 0;
    err = nvs_get_u8(nvs_handle, NVS_KEY_COMPLETE, &complete);
    nvs_close(nvs_handle);

    if (err != ESP_OK || complete == 0) {
        ESP_LOGI(TAG, "Onboarding not complete, showing onboarding screen");
        return true;
    }

    ESP_LOGI(TAG, "Onboarding already complete");
    return false;
}

/* Mark onboarding as complete */
void ui_onboarding_mark_complete(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for onboarding: %s", esp_err_to_name(err));
        return;
    }

    nvs_set_u8(nvs_handle, NVS_KEY_COMPLETE, 1);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Onboarding marked as complete");
}

static void update_model_buttons(void)
{
    const hestia_theme_t *t = ui_theme_get();

    if (s_model_hvac_btn) {
        bool is_hvac = (s_selected_model == DEVICE_MODEL_HVAC);
        lv_obj_set_style_bg_color(s_model_hvac_btn,
            lv_color_hex(is_hvac ? t->primary : t->surface_variant), 0);
        lv_obj_t *label = lv_obj_get_child(s_model_hvac_btn, 0);
        if (label) {
            lv_obj_set_style_text_color(label,
                lv_color_hex(is_hvac ? t->on_primary : t->text), 0);
        }
    }

    if (s_model_eu_btn) {
        bool is_eu = (s_selected_model == DEVICE_MODEL_EU);
        lv_obj_set_style_bg_color(s_model_eu_btn,
            lv_color_hex(is_eu ? t->primary : t->surface_variant), 0);
        lv_obj_t *label = lv_obj_get_child(s_model_eu_btn, 0);
        if (label) {
            lv_obj_set_style_text_color(label,
                lv_color_hex(is_eu ? t->on_primary : t->text), 0);
        }
    }
}

static void update_temp_buttons(void)
{
    const hestia_theme_t *t = ui_theme_get();

    if (s_temp_f_btn) {
        bool is_f = (s_selected_unit == TEMP_UNIT_FAHRENHEIT);
        lv_obj_set_style_bg_color(s_temp_f_btn,
            lv_color_hex(is_f ? t->primary : t->surface_variant), 0);
        lv_obj_t *label = lv_obj_get_child(s_temp_f_btn, 0);
        if (label) {
            lv_obj_set_style_text_color(label,
                lv_color_hex(is_f ? t->on_primary : t->text), 0);
        }
    }

    if (s_temp_c_btn) {
        bool is_c = (s_selected_unit == TEMP_UNIT_CELSIUS);
        lv_obj_set_style_bg_color(s_temp_c_btn,
            lv_color_hex(is_c ? t->primary : t->surface_variant), 0);
        lv_obj_t *label = lv_obj_get_child(s_temp_c_btn, 0);
        if (label) {
            lv_obj_set_style_text_color(label,
                lv_color_hex(is_c ? t->on_primary : t->text), 0);
        }
    }
}

static void on_model_hvac(lv_event_t *e)
{
    (void)e;
    s_selected_model = DEVICE_MODEL_HVAC;
    s_selected_unit = TEMP_UNIT_FAHRENHEIT;  // Default for HVAC
    update_model_buttons();
    update_temp_buttons();
    ESP_LOGI(TAG, "Selected HVAC model, default to Fahrenheit");
}

static void on_model_eu(lv_event_t *e)
{
    (void)e;
    s_selected_model = DEVICE_MODEL_EU;
    s_selected_unit = TEMP_UNIT_CELSIUS;  // Default for EU
    update_model_buttons();
    update_temp_buttons();
    ESP_LOGI(TAG, "Selected EU model, default to Celsius");
}

static void on_temp_f(lv_event_t *e)
{
    (void)e;
    s_selected_unit = TEMP_UNIT_FAHRENHEIT;
    update_temp_buttons();
    ESP_LOGI(TAG, "Selected Fahrenheit");
}

static void on_temp_c(lv_event_t *e)
{
    (void)e;
    s_selected_unit = TEMP_UNIT_CELSIUS;
    update_temp_buttons();
    ESP_LOGI(TAG, "Selected Celsius");
}

static void on_continue(lv_event_t *e)
{
    (void)e;

    /* Save model to device config */
    device_config_t cfg;
    device_config_get(&cfg);
    cfg.model = s_selected_model;

    /* Set default domains and relay mappings based on model */
    for (int i = 0; i < RELAY_DOMAIN_COUNT; i++) cfg.relay_map[i] = RELAY_UNASSIGNED;
    if (s_selected_model == DEVICE_MODEL_HVAC) {
        cfg.domain_mask = (1<<0)|(1<<2)|(1<<4);  // Heat S1, Cool S1, Fan
        cfg.relay_map[0] = 2;   // Heating S1 → relay 2 (W1)
        cfg.relay_map[2] = 0;   // Cooling S1 → relay 0 (Y1)
        cfg.relay_map[4] = 4;   // Fan → relay 4 (G)
    } else {
        cfg.domain_mask = (1<<0)|(1<<4);  // Heat S1, Fan
        cfg.relay_map[0] = 0;   // Heating S1 → relay 0 (L1)
        cfg.relay_map[4] = 1;   // Fan → relay 1 (L2)
    }
    device_config_save(&cfg);

    /* Save temp unit */
    user_settings_set_temp_unit(s_selected_unit);

    /* Mark onboarding complete */
    ui_onboarding_mark_complete();

    ESP_LOGI(TAG, "Onboarding complete: model=%s, temp_unit=%s",
             s_selected_model == DEVICE_MODEL_HVAC ? "HVAC" : "EU",
             s_selected_unit == TEMP_UNIT_FAHRENHEIT ? "Fahrenheit" : "Celsius");

    /* Signal completion */
    if (s_continue_sem) {
        xSemaphoreGive(s_continue_sem);
    }
}

void ui_onboarding_show(void)
{
    const hestia_theme_t *t = ui_theme_get();

    /* Check if model already exists in NVS to preset switches */
    device_config_t cfg;
    device_config_get(&cfg);
    s_selected_model = cfg.model;

    /* Force correct temp unit based on model */
    if (s_selected_model == DEVICE_MODEL_HVAC) {
        s_selected_unit = TEMP_UNIT_FAHRENHEIT;  /* HVAC always uses F */
    } else {
        s_selected_unit = TEMP_UNIT_CELSIUS;     /* EU always uses C */
    }

    ESP_LOGI(TAG, "Showing onboarding with preset: model=%s, temp=%s (forced by model)",
             s_selected_model == DEVICE_MODEL_HVAC ? "HVAC" : "EU",
             s_selected_unit == TEMP_UNIT_FAHRENHEIT ? "F" : "C");

    /* Create semaphore for blocking */
    s_continue_sem = xSemaphoreCreateBinary();

    /* Create full-screen onboarding UI */
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(t->bg), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Welcome to Hestia32");
    lv_obj_set_style_text_color(title, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    /* Subtitle */
    lv_obj_t *subtitle = lv_label_create(scr);
    lv_label_set_text(subtitle, "Choose your device configuration");
    lv_obj_set_style_text_color(subtitle, lv_color_hex(t->text_secondary), 0);
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_14, 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 55);

    /* Model selection label */
    lv_obj_t *model_label = lv_label_create(scr);
    lv_label_set_text(model_label, "Device Model");
    lv_obj_set_style_text_color(model_label, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(model_label, &lv_font_montserrat_16, 0);
    lv_obj_align(model_label, LV_ALIGN_TOP_LEFT, 20, 85);

    /* HVAC button */
    s_model_hvac_btn = lv_btn_create(scr);
    lv_obj_set_size(s_model_hvac_btn, 200, 50);
    lv_obj_set_style_radius(s_model_hvac_btn, 8, 0);
    lv_obj_set_style_shadow_width(s_model_hvac_btn, 0, 0);
    lv_obj_align(s_model_hvac_btn, LV_ALIGN_TOP_LEFT, 20, 115);
    lv_obj_add_event_cb(s_model_hvac_btn, on_model_hvac, LV_EVENT_CLICKED, NULL);

    lv_obj_t *hvac_label = lv_label_create(s_model_hvac_btn);
    lv_label_set_text(hvac_label, "HVAC");
    lv_obj_set_style_text_font(hvac_label, &lv_font_montserrat_16, 0);
    lv_obj_align(hvac_label, LV_ALIGN_CENTER, 0, 0);

    /* EU button */
    s_model_eu_btn = lv_btn_create(scr);
    lv_obj_set_size(s_model_eu_btn, 200, 50);
    lv_obj_set_style_radius(s_model_eu_btn, 8, 0);
    lv_obj_set_style_shadow_width(s_model_eu_btn, 0, 0);
    lv_obj_align(s_model_eu_btn, LV_ALIGN_TOP_RIGHT, -20, 115);
    lv_obj_add_event_cb(s_model_eu_btn, on_model_eu, LV_EVENT_CLICKED, NULL);

    lv_obj_t *eu_label = lv_label_create(s_model_eu_btn);
    lv_label_set_text(eu_label, "EU");
    lv_obj_set_style_text_font(eu_label, &lv_font_montserrat_16, 0);
    lv_obj_align(eu_label, LV_ALIGN_CENTER, 0, 0);

    /* Temperature unit label */
    lv_obj_t *temp_label = lv_label_create(scr);
    lv_label_set_text(temp_label, "Temperature Unit");
    lv_obj_set_style_text_color(temp_label, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_16, 0);
    lv_obj_align(temp_label, LV_ALIGN_TOP_LEFT, 20, 170);

    /* °F button (left, under HVAC) */
    s_temp_f_btn = lv_btn_create(scr);
    lv_obj_set_size(s_temp_f_btn, 200, 50);
    lv_obj_set_style_radius(s_temp_f_btn, 8, 0);
    lv_obj_set_style_shadow_width(s_temp_f_btn, 0, 0);
    lv_obj_align(s_temp_f_btn, LV_ALIGN_TOP_LEFT, 20, 200);
    lv_obj_add_event_cb(s_temp_f_btn, on_temp_f, LV_EVENT_CLICKED, NULL);

    lv_obj_t *f_label = lv_label_create(s_temp_f_btn);
    lv_label_set_text(f_label, "°F");
    lv_obj_set_style_text_font(f_label, &lv_font_montserrat_16, 0);
    lv_obj_align(f_label, LV_ALIGN_CENTER, 0, 0);

    /* °C button (right, under EU) */
    s_temp_c_btn = lv_btn_create(scr);
    lv_obj_set_size(s_temp_c_btn, 200, 50);
    lv_obj_set_style_radius(s_temp_c_btn, 8, 0);
    lv_obj_set_style_shadow_width(s_temp_c_btn, 0, 0);
    lv_obj_align(s_temp_c_btn, LV_ALIGN_TOP_RIGHT, -20, 200);
    lv_obj_add_event_cb(s_temp_c_btn, on_temp_c, LV_EVENT_CLICKED, NULL);

    lv_obj_t *c_label = lv_label_create(s_temp_c_btn);
    lv_label_set_text(c_label, "°C");
    lv_obj_set_style_text_font(c_label, &lv_font_montserrat_16, 0);
    lv_obj_align(c_label, LV_ALIGN_CENTER, 0, 0);

    /* Continue button */
    lv_obj_t *continue_btn = lv_btn_create(scr);
    lv_obj_set_size(continue_btn, 200, 50);
    lv_obj_set_style_bg_color(continue_btn, lv_color_hex(t->primary), 0);
    lv_obj_set_style_radius(continue_btn, 8, 0);
    lv_obj_set_style_shadow_width(continue_btn, 0, 0);
    lv_obj_align(continue_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(continue_btn, on_continue, LV_EVENT_CLICKED, NULL);

    lv_obj_t *continue_label = lv_label_create(continue_btn);
    lv_label_set_text(continue_label, "Continue");
    lv_obj_set_style_text_color(continue_label, lv_color_hex(t->on_primary), 0);
    lv_obj_set_style_text_font(continue_label, &lv_font_montserrat_16, 0);
    lv_obj_align(continue_label, LV_ALIGN_CENTER, 0, 0);

    /* Update button states */
    update_model_buttons();
    update_temp_buttons();

    /* Load screen */
    lv_scr_load(scr);

    /* Block until Continue is pressed */
    ESP_LOGI(TAG, "Waiting for user to press Continue...");
    xSemaphoreTake(s_continue_sem, portMAX_DELAY);

    /* Cleanup */
    vSemaphoreDelete(s_continue_sem);
    s_continue_sem = NULL;

    /* Delete onboarding screen */
    lv_obj_del(scr);

    ESP_LOGI(TAG, "Onboarding complete, continuing boot...");
}
