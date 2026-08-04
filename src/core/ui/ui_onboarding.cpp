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
#include <string.h>
#include <stdlib.h>
#include <time.h>

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
    (void)e;    ESP_LOGI(TAG, "[DEBUG] HVAC button clicked!");    s_selected_model = DEVICE_MODEL_HVAC;
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

    /* Explicitly ensure screen can receive input events (debug for touch issue) */
    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
    ESP_LOGI(TAG, "Onboarding model screen created, flags: clickable=1, scrollable=0");

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
    ESP_LOGI(TAG, "[DEBUG] Onboarding screen loaded, active screen = %p, created screen = %p",
             (void*)lv_scr_act(), (void*)scr);

    /* Block until Continue is pressed */
    ESP_LOGI(TAG, "Waiting for user to press Continue...");
    xSemaphoreTake(s_continue_sem, portMAX_DELAY);

    /* Cleanup */
    vSemaphoreDelete(s_continue_sem);
    s_continue_sem = NULL;

    /* Note: Don't delete screen here - let next screen replace it via lv_scr_load() */

    ESP_LOGI(TAG, "Onboarding complete, continuing boot...");
}

/* ========================================================================== */
/*                    TIME/TIMEZONE ONBOARDING SCREEN                         */
/* ========================================================================== */

static const struct { const char *label; const char *posix; } TZ_LIST_OB[] = {
    { "UTC",               "UTC0"                              },  // 0
    { "London (GMT/BST)",  "GMT0BST,M3.5.0/1,M10.5.0"         },  // 1  (EU default)
    { "Paris/Berlin (CET)","CET-1CEST,M3.5.0,M10.5.0/3"       },  // 2
    { "Athens (EET)",      "EET-2EEST,M3.5.0/3,M10.5.0/4"     },  // 3
    { "Moscow (MSK)",      "MSK-3"                             },  // 4
    { "Dubai (GST)",       "GST-4"                             },  // 5
    { "India (IST)",       "IST-5:30"                          },  // 6
    { "Bangkok (ICT)",     "ICT-7"                             },  // 7
    { "Singapore (SGT)",   "SGT-8"                             },  // 8
    { "Tokyo (JST)",       "JST-9"                             },  // 9
    { "Sydney (AEST)",     "AEST-10AEDT,M10.1.0,M4.1.0/3"     },  // 10
    { "Auckland (NZST)",   "NZST-12NZDT,M9.5.0,M4.1.0/3"      },  // 11
    { "US Eastern (ET)",   "EST5EDT,M3.2.0,M11.1.0"           },  // 12 (HVAC default)
    { "US Central (CT)",   "CST6CDT,M3.2.0,M11.1.0"           },  // 13
    { "US Mountain (MT)",  "MST7MDT,M3.2.0,M11.1.0"           },  // 14
    { "US Pacific (PT)",   "PST8PDT,M3.2.0,M11.1.0"           },  // 15
    { "US Alaska (AKT)",   "AKST9AKDT,M3.2.0,M11.1.0"         },  // 16
    { "US Hawaii (HST)",   "HST10"                             },  // 17
    { "Sao Paulo (BRT)",   "BRT3BRST,M10.3.0/0,M2.3.0/0"      },  // 18
};
#define TZ_COUNT_OB ((int)(sizeof(TZ_LIST_OB)/sizeof(TZ_LIST_OB[0])))
#define TZ_IDX_HVAC_DEFAULT 12
#define TZ_IDX_EU_DEFAULT   1

static time_settings_t   s_ts_ob;
static lv_obj_t         *s_ntp_sw_ob = NULL;
static lv_obj_t         *s_tz_dd_ob  = NULL;
static SemaphoreHandle_t s_time_sem  = NULL;

static void on_time_continue(lv_event_t *e)
{
    (void)e;

    /* Read NTP switch state */
    s_ts_ob.use_ntp = lv_obj_has_state(s_ntp_sw_ob, LV_STATE_CHECKED) ? 1 : 0;

    /* Read timezone dropdown selection */
    uint16_t sel = lv_dropdown_get_selected(s_tz_dd_ob);
    if (sel < TZ_COUNT_OB) {
        strncpy(s_ts_ob.tz_posix, TZ_LIST_OB[sel].posix, sizeof(s_ts_ob.tz_posix) - 1);
        s_ts_ob.tz_posix[sizeof(s_ts_ob.tz_posix) - 1] = '\0';

        /* Apply timezone immediately */
        setenv("TZ", s_ts_ob.tz_posix, 1);
        tzset();
    }

    /* Save time settings */
    time_settings_save(&s_ts_ob);

    ESP_LOGI(TAG, "Time settings saved: NTP=%d, TZ=%s",
             s_ts_ob.use_ntp, s_ts_ob.tz_posix);

    /* Signal completion */
    if (s_time_sem) {
        xSemaphoreGive(s_time_sem);
    }
}

void ui_onboarding_time_show(void)
{
    const hestia_theme_t *t = ui_theme_get();

    /* Load current time settings */
    time_settings_init();
    time_settings_get(&s_ts_ob);

    /* Determine timezone preselection based on device model */
    device_config_t cfg;
    device_config_get(&cfg);
    bool is_hvac = (cfg.model == DEVICE_MODEL_HVAC);
    int default_tz_idx = is_hvac ? TZ_IDX_HVAC_DEFAULT : TZ_IDX_EU_DEFAULT;

    /* Find matching timezone entry, fallback to model default if UTC0 (never set) */
    int presel = default_tz_idx;
    if (strcmp(s_ts_ob.tz_posix, "UTC0") != 0) {
        for (int i = 0; i < TZ_COUNT_OB; i++) {
            if (strcmp(TZ_LIST_OB[i].posix, s_ts_ob.tz_posix) == 0) {
                presel = i;
                break;
            }
        }
    }

    ESP_LOGI(TAG, "Showing time onboarding with presel: model=%s, tz_idx=%d",
             is_hvac ? "HVAC" : "EU", presel);

    /* Create semaphore for blocking */
    s_time_sem = xSemaphoreCreateBinary();

    /* Create full-screen onboarding UI */
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(t->bg), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Internet Time");
    lv_obj_set_style_text_color(title, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    /* Subtitle */
    lv_obj_t *subtitle = lv_label_create(scr);
    lv_label_set_text(subtitle, "Choose how your device syncs the clock");
    lv_obj_set_style_text_color(subtitle, lv_color_hex(t->text_secondary), 0);
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_14, 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 60);

    /* NTP row container */
    lv_obj_t *ntp_cont = lv_obj_create(scr);
    lv_obj_set_size(ntp_cont, 440, 50);
    lv_obj_align(ntp_cont, LV_ALIGN_TOP_MID, 0, 90);
    lv_obj_set_style_bg_color(ntp_cont, lv_color_hex(t->surface), 0);
    lv_obj_set_style_bg_opa(ntp_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ntp_cont, 0, 0);
    lv_obj_set_style_radius(ntp_cont, 8, 0);
    lv_obj_clear_flag(ntp_cont, LV_OBJ_FLAG_SCROLLABLE);

    /* NTP label */
    lv_obj_t *ntp_label = lv_label_create(ntp_cont);
    lv_label_set_text(ntp_label, "Internet Time");
    lv_obj_set_style_text_color(ntp_label, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(ntp_label, &lv_font_montserrat_16, 0);
    lv_obj_align(ntp_label, LV_ALIGN_LEFT_MID, 16, 0);

    /* NTP switch */
    s_ntp_sw_ob = lv_switch_create(ntp_cont);
    lv_obj_align(s_ntp_sw_ob, LV_ALIGN_RIGHT_MID, -16, 0);
    lv_obj_set_style_bg_color(s_ntp_sw_ob, lv_color_hex(t->inactive_color), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ntp_sw_ob, lv_color_hex(t->primary), LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (s_ts_ob.use_ntp) {
        lv_obj_add_state(s_ntp_sw_ob, LV_STATE_CHECKED);
    }

    /* Timezone label */
    lv_obj_t *tz_label = lv_label_create(scr);
    lv_label_set_text(tz_label, "Timezone");
    lv_obj_set_style_text_color(tz_label, lv_color_hex(t->text_secondary), 0);
    lv_obj_set_style_text_font(tz_label, &lv_font_montserrat_14, 0);
    lv_obj_align(tz_label, LV_ALIGN_TOP_LEFT, 20, 152);

    /* Build timezone options string */
    static char tz_opts[512];
    tz_opts[0] = '\0';
    for (int i = 0; i < TZ_COUNT_OB; i++) {
        strncat(tz_opts, TZ_LIST_OB[i].label, sizeof(tz_opts) - strlen(tz_opts) - 2);
        if (i < TZ_COUNT_OB - 1) {
            strncat(tz_opts, "\n", sizeof(tz_opts) - strlen(tz_opts) - 1);
        }
    }

    /* Timezone dropdown */
    s_tz_dd_ob = lv_dropdown_create(scr);
    lv_obj_set_size(s_tz_dd_ob, 440, 44);
    lv_obj_align(s_tz_dd_ob, LV_ALIGN_TOP_MID, 0, 172);
    lv_dropdown_set_options(s_tz_dd_ob, tz_opts);
    lv_dropdown_set_selected(s_tz_dd_ob, (uint16_t)presel);

    /* Style dropdown */
    lv_obj_set_style_bg_color(s_tz_dd_ob, lv_color_hex(t->surface), 0);
    lv_obj_set_style_bg_opa(s_tz_dd_ob, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_tz_dd_ob, lv_color_hex(t->border), 0);
    lv_obj_set_style_border_width(s_tz_dd_ob, 1, 0);
    lv_obj_set_style_text_color(s_tz_dd_ob, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(s_tz_dd_ob, &lv_font_montserrat_14, 0);

    /* Style dropdown list */
    lv_obj_t *list = lv_dropdown_get_list(s_tz_dd_ob);
    if (list) {
        lv_obj_set_style_max_height(list, 120, 0);
        lv_obj_set_style_bg_color(list, lv_color_hex(t->surface), 0);
        lv_obj_set_style_text_color(list, lv_color_hex(t->text), 0);
        lv_obj_set_style_text_color(list, lv_color_hex(t->primary), LV_PART_SELECTED);
    }

    /* Continue button */
    lv_obj_t *continue_btn = lv_btn_create(scr);
    lv_obj_set_size(continue_btn, 200, 50);
    lv_obj_set_style_bg_color(continue_btn, lv_color_hex(t->primary), 0);
    lv_obj_set_style_radius(continue_btn, 8, 0);
    lv_obj_set_style_shadow_width(continue_btn, 0, 0);
    lv_obj_align(continue_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(continue_btn, on_time_continue, LV_EVENT_CLICKED, NULL);

    lv_obj_t *continue_label = lv_label_create(continue_btn);
    lv_label_set_text(continue_label, "Continue");
    lv_obj_set_style_text_color(continue_label, lv_color_hex(t->on_primary), 0);
    lv_obj_set_style_text_font(continue_label, &lv_font_montserrat_16, 0);
    lv_obj_align(continue_label, LV_ALIGN_CENTER, 0, 0);

    /* Load screen */
    lv_scr_load(scr);

    /* Block until Continue is pressed */
    ESP_LOGI(TAG, "Waiting for user to press Continue...");
    xSemaphoreTake(s_time_sem, portMAX_DELAY);

    /* Cleanup */
    vSemaphoreDelete(s_time_sem);
    s_time_sem = NULL;

    /* Note: Don't delete screen here - display_clear_screen() in main.c will clean up */

    ESP_LOGI(TAG, "Time onboarding complete, continuing boot...");
}

