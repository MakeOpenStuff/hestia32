#include "ui/ui_settings_system.h"
#include "ui/ui_common.h"
#include "ui/ui_theme.h"
#include "ui/ui_icons.h"
#include "core/user_settings.h"
#include "core/display_manager.h"
#include "core/core_config.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_http_client.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>

#define GITHUB_API_URL "https://api.github.com/repos/MakeOpenStuff/hestia32/releases/latest"
#define HTTP_RECV_BUFFER_SIZE 1024

static const char *TAG = "ui_system";

static lv_obj_t *s_ota_result_label = NULL;
static lv_obj_t *s_pointer_switch = NULL;
static lv_obj_t *s_page_container = NULL;
static lv_obj_t *s_pages[4] = {NULL};
static lv_obj_t *s_next_btn = NULL;
static int s_current_page = 0;
static char s_http_buffer[HTTP_RECV_BUFFER_SIZE];
static int s_http_buffer_len = 0;

/* Forward declaration */
static void update_page_navigation(void);

/* HTTP event handler for OTA check */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (s_http_buffer_len + evt->data_len < HTTP_RECV_BUFFER_SIZE) {
                memcpy(s_http_buffer + s_http_buffer_len, evt->data, evt->data_len);
                s_http_buffer_len += evt->data_len;
                s_http_buffer[s_http_buffer_len] = '\0';
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void on_back(lv_event_t *e)
{
    (void)e;
    /* If on first page, go back to settings. Otherwise, go to previous page */
    if (s_current_page == 0) {
        ui_common_pop_screen();
    } else {
        s_current_page--;
        update_page_navigation();
    }
}

static void update_page_navigation(void)
{
    /* Update page visibility */
    for (int i = 0; i < 4; i++) {
        if (s_pages[i]) {
            if (i == s_current_page) {
                lv_obj_clear_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    /* Next button visible when not on last page */
    if (s_next_btn) {
        if (s_current_page < 3) {
            lv_obj_clear_flag(s_next_btn, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_next_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }
}



static void on_next_page(lv_event_t *e)
{
    (void)e;
    if (s_current_page < 3) {
        s_current_page++;
        update_page_navigation();
    }
}

static void on_restart(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Restart button pressed");

    /* Simple restart - no confirmation needed for restart */
    esp_restart();
}

static void on_factory_reset_confirmed(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Factory reset confirmed - erasing NVS");

    /* Erase NVS partition */
    nvs_flash_erase();

    /* Restart */
    esp_restart();
}

static void on_factory_reset(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Factory reset button pressed - showing confirmation");

    const hestia_theme_t *t = ui_theme_get();

    /* Create modal dialog */
    lv_obj_t *dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(dialog, 360, 200);
    lv_obj_center(dialog);
    lv_obj_set_style_bg_color(dialog, lv_color_hex(t->surface), 0);
    lv_obj_set_style_border_color(dialog, lv_color_hex(t->danger), 0);
    lv_obj_set_style_border_width(dialog, 2, 0);
    lv_obj_set_style_radius(dialog, 12, 0);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lv_obj_t *title = lv_label_create(dialog);
    lv_label_set_text(title, "Factory Reset");
    lv_obj_set_style_text_color(title, lv_color_hex(t->danger), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    /* Message */
    lv_obj_t *msg = lv_label_create(dialog);
    lv_label_set_text(msg, "This will erase all settings\nand restart the device.\n\nAre you sure?");
    lv_obj_set_style_text_color(msg, lv_color_hex(t->text), 0);
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
        lv_obj_del(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(e))));
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cancel_lbl = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_lbl, "Cancel");
    lv_obj_center(cancel_lbl);

    /* Confirm button */
    lv_obj_t *confirm_btn = lv_btn_create(btn_row);
    lv_obj_set_size(confirm_btn, 140, 45);
    lv_obj_set_style_bg_color(confirm_btn, lv_color_hex(t->danger), 0);
    lv_obj_add_event_cb(confirm_btn, on_factory_reset_confirmed, LV_EVENT_CLICKED, NULL);

    lv_obj_t *confirm_lbl = lv_label_create(confirm_btn);
    lv_label_set_text(confirm_lbl, "Reset");
    lv_obj_set_style_text_color(confirm_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(confirm_lbl);
}

static void format_uptime(char *buf, size_t buf_size)
{
    uint64_t uptime_us = esp_timer_get_time();
    uint64_t uptime_sec = uptime_us / 1000000;

    if (uptime_sec < 60) {
        snprintf(buf, buf_size, "%llu seconds", uptime_sec);
    } else if (uptime_sec < 3600) {
        snprintf(buf, buf_size, "%llu minutes", uptime_sec / 60);
    } else if (uptime_sec < 86400) {
        snprintf(buf, buf_size, "%llu hours", uptime_sec / 3600);
    } else {
        snprintf(buf, buf_size, "%llu days", uptime_sec / 86400);
    }
}

static void on_ota_check(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "OTA check button pressed");

    if (s_ota_result_label) {
        lv_label_set_text(s_ota_result_label, "Checking for updates...");
        lv_refr_now(NULL);
    }

    /* Reset buffer */
    s_http_buffer_len = 0;
    s_http_buffer[0] = '\0';

    /* Configure HTTP client */
    esp_http_client_config_t config = {};
    config.url = GITHUB_API_URL;
    config.event_handler = http_event_handler;
    config.timeout_ms = 10000;
    config.buffer_size = 512;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        if (s_ota_result_label) {
            lv_label_set_text(s_ota_result_label, "Error: Failed to initialize HTTP client");
        }
        return;
    }

    /* Add User-Agent header (GitHub API requires it) */
    esp_http_client_set_header(client, "User-Agent", "Hestia32-Firmware");

    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP GET failed: %s", esp_err_to_name(err));
        if (s_ota_result_label) {
            lv_label_set_text_fmt(s_ota_result_label, "Error: %s\nCheck WiFi connection", esp_err_to_name(err));
        }
        esp_http_client_cleanup(client);
        return;
    }

    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP status: %d", status_code);
        if (s_ota_result_label) {
            lv_label_set_text_fmt(s_ota_result_label, "Error: HTTP %d\nURL: api.github.com", status_code);
        }
        esp_http_client_cleanup(client);
        return;
    }

    esp_http_client_cleanup(client);

    /* Parse JSON response */
    cJSON *root = cJSON_Parse(s_http_buffer);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        if (s_ota_result_label) {
            lv_label_set_text(s_ota_result_label, "Error: Invalid JSON response");
        }
        return;
    }

    cJSON *tag_name = cJSON_GetObjectItem(root, "tag_name");

    if (tag_name && cJSON_IsString(tag_name)) {
        const char *latest_version = tag_name->valuestring;
        ESP_LOGI(TAG, "Latest version: %s", latest_version);

        /* Compare with current version - for now just display it */
        if (s_ota_result_label) {
            lv_label_set_text_fmt(s_ota_result_label,
                "Latest: %s\nCurrent: v0.1.0-dev\nCheck GitHub for details",
                latest_version);
        }
    } else {
        ESP_LOGE(TAG, "tag_name not found in response");
        if (s_ota_result_label) {
            lv_label_set_text(s_ota_result_label, "Error: No release version found");
        }
    }

    cJSON_Delete(root);
}

static void on_ota_install(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "OTA install stub - not yet implemented");

    if (s_ota_result_label) {
        lv_label_set_text(s_ota_result_label, "OTA install feature coming soon");
    }
}

static void on_pointer_toggle(lv_event_t *e)
{
    lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
    bool show_pointer = lv_obj_has_state(sw, LV_STATE_CHECKED);

    /* Save to device config */
    device_config_t cfg;
    if (device_config_get(&cfg) == ESP_OK) {
        cfg.show_pointer = show_pointer ? 1 : 0;
        esp_err_t err = device_config_save(&cfg);

        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Touch pointer %s", show_pointer ? "enabled" : "disabled");
            /* Update pointer visibility immediately */
            display_set_touch_pointer_visible(show_pointer);
        } else {
            ESP_LOGE(TAG, "Failed to save pointer setting: %d", err);
        }
    }
}

static void on_recalibrate(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Touch recalibration triggered");

    /* Run calibration wizard - this will clear the screen and show targets */
    display_recalibrate();
}

void ui_settings_system_open(void)
{
    const hestia_theme_t *t = ui_theme_get();
    s_current_page = 0;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(scr, lv_color_hex(t->bg), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Header with title, back button, and navigation arrows */
    lv_obj_t *header = lv_obj_create(scr);
    lv_obj_set_size(header, UI_SCREEN_W, UI_HEADER_H);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(t->surface), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(header, lv_color_hex(t->border), LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    /* Back button */
    lv_obj_t *back_btn = lv_btn_create(header);
    lv_obj_set_size(back_btn, UI_HEADER_H, UI_HEADER_H);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(t->surface), 0);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(back_btn, 0, 0);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_radius(back_btn, 0, 0);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(back_btn, on_back, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_icon = lv_label_create(back_btn);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_icon, lv_color_hex(t->primary), 0);
    lv_obj_set_style_text_font(back_icon, &lv_font_montserrat_20, 0);
    lv_obj_align(back_icon, LV_ALIGN_CENTER, 0, 0);

    /* Title */
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "System");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(t->text), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    /* Next page button (right arrow in top right corner) */
    s_next_btn = lv_btn_create(header);
    lv_obj_set_size(s_next_btn, UI_HEADER_H, UI_HEADER_H);
    lv_obj_set_style_bg_color(s_next_btn, lv_color_hex(t->surface), 0);
    lv_obj_set_style_bg_opa(s_next_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(s_next_btn, 0, 0);
    lv_obj_set_style_border_width(s_next_btn, 0, 0);
    lv_obj_set_style_radius(s_next_btn, 0, 0);
    lv_obj_align(s_next_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(s_next_btn, on_next_page, LV_EVENT_CLICKED, NULL);

    lv_obj_t *next_icon = lv_label_create(s_next_btn);
    lv_label_set_text(next_icon, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(next_icon, lv_color_hex(t->primary), 0);
    lv_obj_set_style_text_font(next_icon, &lv_font_montserrat_20, 0);
    lv_obj_align(next_icon, LV_ALIGN_CENTER, 0, 0);

    /* Page container */
    s_page_container = lv_obj_create(scr);
    lv_obj_set_size(s_page_container, UI_SCREEN_W, UI_SCREEN_H - UI_HEADER_H);
    lv_obj_align(s_page_container, LV_ALIGN_TOP_LEFT, 0, UI_HEADER_H);
    lv_obj_set_style_bg_color(s_page_container, lv_color_hex(t->bg), 0);
    lv_obj_set_style_border_width(s_page_container, 0, 0);
    lv_obj_set_style_pad_all(s_page_container, 0, 0);
    lv_obj_clear_flag(s_page_container, LV_OBJ_FLAG_SCROLLABLE);

    /* ============ PAGE 1: ACTIONS (Restart & Factory Reset) ============ */
    s_pages[0] = lv_obj_create(s_page_container);
    lv_obj_set_size(s_pages[0], LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s_pages[0], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_pages[0], 0, 0);
    lv_obj_set_style_pad_all(s_pages[0], 16, 0);
    lv_obj_set_flex_flow(s_pages[0], LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_pages[0], LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_pages[0], 16, 0);

    /* Page title */
    lv_obj_t *page1_title = lv_label_create(s_pages[0]);
    lv_label_set_text(page1_title, "System Actions");
    lv_obj_set_style_text_color(page1_title, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(page1_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_pad_bottom(page1_title, 16, 0);

    /* Restart button */
    lv_obj_t *restart_btn = lv_btn_create(s_pages[0]);
    lv_obj_set_size(restart_btn, LV_PCT(90), 60);
    lv_obj_set_style_bg_color(restart_btn, lv_color_hex(t->danger), 0);
    lv_obj_add_event_cb(restart_btn, on_restart, LV_EVENT_CLICKED, NULL);
    lv_obj_set_flex_flow(restart_btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(restart_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(restart_btn, 8, 0);

    lv_obj_t *restart_icon = lv_label_create(restart_btn);
    lv_label_set_text(restart_icon, ICON_RESTART);
    lv_obj_set_style_text_font(restart_icon, ICON_FONT_20, 0);

    lv_obj_t *restart_lbl = lv_label_create(restart_btn);
    lv_label_set_text(restart_lbl, "Restart Device");
    lv_obj_set_style_text_font(restart_lbl, &lv_font_montserrat_16, 0);

    /* Factory Reset button */
    lv_obj_t *reset_btn = lv_btn_create(s_pages[0]);
    lv_obj_set_size(reset_btn, LV_PCT(90), 60);
    lv_obj_set_style_bg_color(reset_btn, lv_color_hex(t->warning), 0);
    lv_obj_add_event_cb(reset_btn, on_factory_reset, LV_EVENT_CLICKED, NULL);
    lv_obj_set_flex_flow(reset_btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(reset_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(reset_btn, 8, 0);

    lv_obj_t *reset_icon = lv_label_create(reset_btn);
    lv_label_set_text(reset_icon, ICON_WARNING);
    lv_obj_set_style_text_font(reset_icon, ICON_FONT_20, 0);
    lv_obj_set_style_text_color(reset_icon, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t *reset_lbl = lv_label_create(reset_btn);
    lv_label_set_text(reset_lbl, "Factory Reset");
    lv_obj_set_style_text_font(reset_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(reset_lbl, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t *warning = lv_label_create(s_pages[0]);
    lv_label_set_text(warning, "Factory reset erases all settings");
    lv_obj_set_style_text_color(warning, lv_color_hex(t->text_secondary), 0);
    lv_obj_set_style_text_font(warning, &lv_font_montserrat_14, 0);

    /* ============ PAGE 2: OTA UPDATES ============ */
    s_pages[1] = lv_obj_create(s_page_container);
    lv_obj_set_size(s_pages[1], LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s_pages[1], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_pages[1], 0, 0);
    lv_obj_set_style_pad_all(s_pages[1], 16, 0);
    lv_obj_set_flex_flow(s_pages[1], LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_pages[1], LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(s_pages[1], 12, 0);
    lv_obj_add_flag(s_pages[1], LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *ota_title = lv_label_create(s_pages[1]);
    lv_label_set_text(ota_title, "Firmware Updates");
    lv_obj_set_style_text_color(ota_title, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(ota_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_pad_bottom(ota_title, 8, 0);

    lv_obj_t *check_btn = lv_btn_create(s_pages[1]);
    lv_obj_set_size(check_btn, LV_PCT(100), 50);
    lv_obj_set_style_bg_color(check_btn, lv_color_hex(0x2196F3), 0);
    lv_obj_add_event_cb(check_btn, on_ota_check, LV_EVENT_CLICKED, NULL);

    lv_obj_t *check_label = lv_label_create(check_btn);
    lv_label_set_text(check_label, "Check for Updates");
    lv_obj_center(check_label);

    lv_obj_t *install_btn = lv_btn_create(s_pages[1]);
    lv_obj_set_size(install_btn, LV_PCT(100), 50);
    lv_obj_set_style_bg_color(install_btn, lv_color_hex(0xFF9800), 0);
    lv_obj_add_event_cb(install_btn, on_ota_install, LV_EVENT_CLICKED, NULL);

    lv_obj_t *install_label = lv_label_create(install_btn);
    lv_label_set_text(install_label, "Install Update (Coming Soon)");
    lv_obj_center(install_label);

    s_ota_result_label = lv_label_create(s_pages[1]);
    lv_label_set_text(s_ota_result_label, "");
    lv_obj_set_style_text_color(s_ota_result_label, lv_color_hex(t->text_secondary), 0);
    lv_obj_set_style_text_font(s_ota_result_label, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(s_ota_result_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_ota_result_label, LV_PCT(100));

    /* ============ PAGE 3: SYSTEM INFO ============ */
    s_pages[2] = lv_obj_create(s_page_container);
    lv_obj_set_size(s_pages[2], LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s_pages[2], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_pages[2], 0, 0);
    lv_obj_set_style_pad_all(s_pages[2], 16, 0);
    lv_obj_set_flex_flow(s_pages[2], LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_pages[2], LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(s_pages[2], 12, 0);
    lv_obj_add_flag(s_pages[2], LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *info_title = lv_label_create(s_pages[2]);
    lv_label_set_text(info_title, "System Information");
    lv_obj_set_style_text_color(info_title, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(info_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_pad_bottom(info_title, 16, 0);

    char uptime_buf[64];
    format_uptime(uptime_buf, sizeof(uptime_buf));

    lv_obj_t *uptime_label = lv_label_create(s_pages[2]);
    lv_label_set_text_fmt(uptime_label, "Uptime: %s", uptime_buf);
    lv_obj_set_style_text_color(uptime_label, lv_color_hex(t->text_secondary), 0);

    lv_obj_t *version_label = lv_label_create(s_pages[2]);
		// show build version and date/time of build
    lv_label_set_text_fmt(version_label, "Version: %s", APP_VERSION);
    lv_obj_set_style_text_color(version_label, lv_color_hex(t->text_secondary), 0);

    lv_obj_t *build_label = lv_label_create(s_pages[2]);
    lv_label_set_text_fmt(build_label, "Built: %s %s", __DATE__, __TIME__);
    lv_obj_set_style_text_color(build_label, lv_color_hex(t->text_secondary), 0);

    /* ============ PAGE 4: TOUCH SETTINGS ============ */
    s_pages[3] = lv_obj_create(s_page_container);
    lv_obj_set_size(s_pages[3], LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s_pages[3], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_pages[3], 0, 0);
    lv_obj_set_style_pad_all(s_pages[3], 16, 0);
    lv_obj_set_flex_flow(s_pages[3], LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_pages[3], LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(s_pages[3], 12, 0);
    lv_obj_add_flag(s_pages[3], LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *touch_title = lv_label_create(s_pages[3]);
    lv_label_set_text(touch_title, "Touch Settings");
    lv_obj_set_style_text_color(touch_title, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(touch_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_pad_bottom(touch_title, 16, 0);

    lv_obj_t *pointer_row = lv_obj_create(s_pages[3]);
    lv_obj_set_size(pointer_row, LV_PCT(100), 50);
    lv_obj_set_style_bg_color(pointer_row, lv_color_hex(t->surface), 0);
    lv_obj_set_style_border_width(pointer_row, 0, 0);
    lv_obj_set_style_radius(pointer_row, 8, 0);
    lv_obj_clear_flag(pointer_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *pointer_title = lv_label_create(pointer_row);
    lv_label_set_text(pointer_title, "Show Pointer");
    lv_obj_set_style_text_color(pointer_title, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(pointer_title, &lv_font_montserrat_16, 0);
    lv_obj_align(pointer_title, LV_ALIGN_LEFT_MID, 16, 0);

    s_pointer_switch = lv_switch_create(pointer_row);
    lv_obj_align(s_pointer_switch, LV_ALIGN_RIGHT_MID, -16, 0);

    device_config_t cfg;
    if (device_config_get(&cfg) == ESP_OK && cfg.show_pointer) {
        lv_obj_add_state(s_pointer_switch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(s_pointer_switch, on_pointer_toggle, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *recal_btn = lv_btn_create(s_pages[3]);
    lv_obj_set_size(recal_btn, LV_PCT(100), 50);
    lv_obj_set_style_bg_color(recal_btn, lv_color_hex(0x9C27B0), 0);
    lv_obj_add_event_cb(recal_btn, on_recalibrate, LV_EVENT_CLICKED, NULL);

    lv_obj_t *recal_label = lv_label_create(recal_btn);
    lv_label_set_text(recal_label, "Recalibrate Touchscreen");
    lv_obj_center(recal_label);

    /* Initialize page navigation */
    update_page_navigation();

    ui_common_push_screen(scr);
}
