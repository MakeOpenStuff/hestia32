#include "ui/ui_settings_protocol.h"
#include "ui/ui_common.h"
#include "ui/ui_theme.h"
#include "ui/ui_icons.h"
extern "C" {
#include "core/protocol_manager.h"
}
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

#ifdef CONFIG_PROTOCOL_MQTT
extern "C" {
#include "protocols/mqtt/wifi_manager.h"
#include "protocols/mqtt/wifi_provisioning.h"
}
#include "esp_netif.h"
#include "esp_wifi.h"
#endif

static const char *TAG = "ui_protocol";

static lv_obj_t *s_pages[2] = {NULL};
static lv_obj_t *s_next_btn = NULL;
static int s_current_page = 0;
static lv_timer_t *s_refresh_timer = NULL;
static bool s_mqtt_page_built = false;

static lv_obj_t *s_network_value_label = NULL;
static lv_obj_t *s_signal_dbm_label = NULL;
static lv_obj_t *s_signal_icon = NULL;
static lv_obj_t *s_ip_value_label = NULL;
static lv_obj_t *s_mac_value_label = NULL;

typedef enum {
    WIFI_STATE_NOT_PROVISIONED = 0,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_PROVISIONED_NOT_CONNECTED,
} wifi_state_t;

typedef struct {
    wifi_state_t state;
    bool connected;
    bool provisioned;
    int rssi;
    char ssid[33];
    char ip[20];
    char mac[20];
#ifdef CONFIG_PROTOCOL_MQTT
    char server_url[MAX_SERVER_URL_LEN];
    char node_name[MAX_NODE_NAME_LEN];
#endif
} connectivity_snapshot_t;

static void update_page_navigation(void);
static void refresh_network_live(void);
static bool ensure_mqtt_page_built(void);
static void collect_snapshot(connectivity_snapshot_t *snap);
static void create_page_mqtt(lv_obj_t *page, const connectivity_snapshot_t *snap);
static void create_page_mqtt_placeholder(lv_obj_t *page, const char *msg);

static uint32_t signal_color(bool connected, int rssi)
{
    const hestia_theme_t *t = ui_theme_get();
    if (!connected) {
        return t->text_secondary;
    }
    if (rssi >= -60) {
        return 0x2E7D32;  /* green */
    }
    if (rssi >= -75) {
        return 0xF57C00;  /* orange */
    }
    return 0xC62828;      /* red */
}

static void on_back(lv_event_t *e)
{
    (void)e;
    if (s_current_page == 0) {
        ui_common_pop_screen();
    } else {
        s_current_page--;
        update_page_navigation();
    }
}

static void on_next_page(lv_event_t *e)
{
    (void)e;
    if (s_current_page == 0) {
        if (!ensure_mqtt_page_built()) {
            ESP_LOGW(TAG, "MQTT page unavailable due to low LVGL memory");
        }
        s_current_page = 1;
        update_page_navigation();
    }
}

static void on_reprovision(lv_event_t *e)
{
    (void)e;
#ifdef CONFIG_PROTOCOL_MQTT
    ESP_LOGI(TAG, "User requested WiFi re-provisioning");
    if (wifi_prov_reset() != ESP_OK) {
        ESP_LOGW(TAG, "WiFi re-provision reset failed, restarting anyway");
    }
    esp_restart();
#endif
}

static void update_page_navigation(void)
{
    for (int i = 0; i < 2; i++) {
        if (!s_pages[i]) {
            continue;
        }

        if (i == s_current_page) {
            lv_obj_clear_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_next_btn) {
        if (s_current_page == 0) {
            lv_obj_clear_flag(s_next_btn, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_next_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static bool ensure_mqtt_page_built(void)
{
    if (s_mqtt_page_built) {
        return true;
    }

    if (!s_pages[1]) {
        return false;
    }

    connectivity_snapshot_t snap;
    collect_snapshot(&snap);
    create_page_mqtt(s_pages[1], &snap);
    s_mqtt_page_built = true;
    return true;
}

static void on_screen_delete(lv_event_t *e)
{
    (void)e;
    if (s_refresh_timer) {
        lv_timer_del(s_refresh_timer);
        s_refresh_timer = NULL;
    }

    s_network_value_label = NULL;
    s_signal_dbm_label = NULL;
    s_signal_icon = NULL;
    s_ip_value_label = NULL;
    s_mac_value_label = NULL;
    s_pages[0] = NULL;
    s_pages[1] = NULL;
    s_next_btn = NULL;
    s_mqtt_page_built = false;
}

static lv_obj_t *create_kv_row(lv_obj_t *parent,
                               int y,
                               const char *label,
                               const char *value,
                               uint32_t value_color,
                               bool ellipsis)
{
    const hestia_theme_t *t = ui_theme_get();

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_pos(row, UI_PAD, y);
    lv_obj_set_size(row, UI_SCREEN_W - (UI_PAD * 2), 38);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(row, lv_color_hex(t->border), LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *k = lv_label_create(row);
    lv_label_set_text(k, label);
    lv_obj_set_style_text_color(k, lv_color_hex(t->text_secondary), 0);
    lv_obj_set_style_text_font(k, &lv_font_montserrat_14, 0);
    lv_obj_align(k, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *v = lv_label_create(row);
    lv_label_set_text(v, value);
    lv_obj_set_style_text_color(v, lv_color_hex(value_color), 0);
    lv_obj_set_style_text_font(v, &lv_font_montserrat_14, 0);
    lv_obj_set_width(v, 270);
    lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
    if (ellipsis) {
        lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);
    }
    lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);

    return row;
}

static void create_signal_value(lv_obj_t *row, bool connected, int rssi)
{
    const hestia_theme_t *t = ui_theme_get();
    const int wrap_w = 144;
    const int wrap_h = 24;
    const int icon_w = 20;
    const int icon_h = 20;
    const int gap_w = 12;
    const int label_w = wrap_w - icon_w - gap_w;

    lv_obj_t *wrap = lv_obj_create(row);
    lv_obj_set_size(wrap, wrap_w, wrap_h);
    lv_obj_set_style_bg_opa(wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wrap, 0, 0);
    lv_obj_set_style_pad_all(wrap, 0, 0);
    lv_obj_clear_flag(wrap, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(wrap, 0);
    lv_obj_align(wrap, LV_ALIGN_RIGHT_MID, 0, 0);

    s_signal_dbm_label = lv_label_create(wrap);
    lv_obj_set_width(s_signal_dbm_label, label_w);
    lv_obj_set_style_text_align(s_signal_dbm_label, LV_TEXT_ALIGN_RIGHT, 0);
    if (connected) {
        lv_label_set_text_fmt(s_signal_dbm_label, "%d dBm", rssi);
        lv_obj_set_style_text_color(s_signal_dbm_label, lv_color_hex(t->text), 0);
    } else {
        lv_label_set_text(s_signal_dbm_label, "offline");
        lv_obj_set_style_text_color(s_signal_dbm_label, lv_color_hex(t->text_secondary), 0);
    }
    lv_obj_set_style_text_font(s_signal_dbm_label, &lv_font_montserrat_14, 0);
    lv_obj_align(s_signal_dbm_label, LV_ALIGN_LEFT_MID, 0, 0);

    s_signal_icon = lv_label_create(wrap);
    lv_label_set_text(s_signal_icon, ICON_WIFI);
    lv_obj_set_style_text_font(s_signal_icon, ICON_FONT_20 ? ICON_FONT_20 : &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_signal_icon, lv_color_hex(signal_color(connected, rssi)), 0);
    lv_obj_set_size(s_signal_icon, icon_w, icon_h);
    lv_obj_set_pos(s_signal_icon, label_w + gap_w, (wrap_h - icon_h) / 2);
}

static void collect_snapshot(connectivity_snapshot_t *snap)
{
    memset(snap, 0, sizeof(*snap));
    snap->rssi = -95;

    strncpy(snap->ssid, "-", sizeof(snap->ssid) - 1);
    strncpy(snap->ip, "-", sizeof(snap->ip) - 1);
    strncpy(snap->mac, "-", sizeof(snap->mac) - 1);

#ifdef CONFIG_PROTOCOL_MQTT
    strncpy(snap->server_url, "Not configured", sizeof(snap->server_url) - 1);
    strncpy(snap->node_name, "hestia32", sizeof(snap->node_name) - 1);

    snap->provisioned = wifi_prov_is_provisioned();
    snap->connected = wifi_is_connected();

    wifi_config_data_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    if (wifi_prov_get_config(&cfg) == ESP_OK) {
        if (cfg.ssid[0] != '\0') {
            strncpy(snap->ssid, cfg.ssid, sizeof(snap->ssid) - 1);
            snap->ssid[sizeof(snap->ssid) - 1] = '\0';
        }
        if (cfg.server_url[0] != '\0') {
            strncpy(snap->server_url, cfg.server_url, sizeof(snap->server_url) - 1);
            snap->server_url[sizeof(snap->server_url) - 1] = '\0';
        }
        if (cfg.node_name[0] != '\0') {
            strncpy(snap->node_name, cfg.node_name, sizeof(snap->node_name) - 1);
            snap->node_name[sizeof(snap->node_name) - 1] = '\0';
        }
    }

    if (!snap->provisioned) {
        snap->state = WIFI_STATE_NOT_PROVISIONED;
        return;
    }

    if (snap->connected) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            strncpy(snap->ssid, (const char *)ap_info.ssid, sizeof(snap->ssid) - 1);
            snap->ssid[sizeof(snap->ssid) - 1] = '\0';
            snap->rssi = ap_info.rssi;
        }

        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif != NULL) {
            esp_netif_ip_info_t ip_info;
            if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
                snprintf(snap->ip, sizeof(snap->ip), IPSTR, IP2STR(&ip_info.ip));
            }

            uint8_t mac[6];
            if (esp_netif_get_mac(netif, mac) == ESP_OK) {
                snprintf(snap->mac, sizeof(snap->mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            }
        }

        snap->state = WIFI_STATE_CONNECTED;
    } else {
        snap->state = WIFI_STATE_PROVISIONED_NOT_CONNECTED;
    }
#else
    snap->state = WIFI_STATE_NOT_PROVISIONED;
#endif
}

static void create_page_network(lv_obj_t *page, const connectivity_snapshot_t *snap)
{
    const hestia_theme_t *t = ui_theme_get();

    lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);

#ifdef CONFIG_PROTOCOL_MQTT
    if (snap->state == WIFI_STATE_NOT_PROVISIONED) {
        lv_obj_t *center = lv_obj_create(page);
        lv_obj_set_size(center, UI_SCREEN_W, UI_SCREEN_H - UI_HEADER_H);
        lv_obj_set_style_bg_opa(center, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(center, 0, 0);
        lv_obj_set_style_pad_all(center, 0, 0);
        lv_obj_set_flex_flow(center, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(center, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(center, 14, 0);
        lv_obj_clear_flag(center, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *msg = lv_label_create(center);
        lv_label_set_text(msg, "Not connected to WiFi\n\nTo connect, press the button below.");
        lv_obj_set_style_text_color(msg, lv_color_hex(t->text_secondary), 0);
        lv_obj_set_style_text_font(msg, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t *btn_wrap = lv_obj_create(center);
        lv_obj_set_size(btn_wrap, 320, 52);
        lv_obj_set_style_bg_opa(btn_wrap, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(btn_wrap, 0, 0);
        lv_obj_set_style_pad_all(btn_wrap, 0, 0);
        lv_obj_clear_flag(btn_wrap, LV_OBJ_FLAG_SCROLLABLE);

        ui_common_btn(btn_wrap, "RE-PROVISION WIFI (restarts device)", on_reprovision, NULL);
        return;
    }

    char network_value[64];
    snprintf(network_value, sizeof(network_value), "WiFi - %s", snap->ssid);

    int y = 10;
    lv_obj_t *network_row = create_kv_row(page, y, "Network", network_value, t->text, true);
    s_network_value_label = lv_obj_get_child(network_row, 1);
    y += 40;

    lv_obj_t *signal_row = lv_obj_create(page);
    lv_obj_set_pos(signal_row, UI_PAD, y);
    lv_obj_set_size(signal_row, UI_SCREEN_W - (UI_PAD * 2), 38);
    lv_obj_set_style_bg_opa(signal_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(signal_row, 0, 0);
    lv_obj_set_style_border_side(signal_row, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(signal_row, lv_color_hex(t->border), LV_PART_MAIN);
    lv_obj_set_style_border_width(signal_row, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(signal_row, 0, 0);
    lv_obj_set_style_radius(signal_row, 0, 0);
    lv_obj_clear_flag(signal_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *signal_label = lv_label_create(signal_row);
    lv_label_set_text(signal_label, "Signal");
    lv_obj_set_style_text_color(signal_label, lv_color_hex(t->text_secondary), 0);
    lv_obj_set_style_text_font(signal_label, &lv_font_montserrat_14, 0);
    lv_obj_align(signal_label, LV_ALIGN_LEFT_MID, 0, 0);

    create_signal_value(signal_row, snap->connected, snap->rssi);
    y += 40;

    lv_obj_t *ip_row = create_kv_row(page,
                                     y,
                                     "IP (DHCP)",
                                     snap->ip,
                                     snap->connected ? t->text : t->text_secondary,
                                     false);
    s_ip_value_label = lv_obj_get_child(ip_row, 1);
    y += 40;

    lv_obj_t *mac_row = create_kv_row(page,
                                      y,
                                      "MAC",
                                      snap->mac,
                                      snap->connected ? t->text : t->text_secondary,
                                      false);
    s_mac_value_label = lv_obj_get_child(mac_row, 1);

    lv_obj_t *btn_area = lv_obj_create(page);
    lv_obj_set_size(btn_area, UI_SCREEN_W - (UI_PAD * 2), 52);
    lv_obj_set_pos(btn_area, UI_PAD, UI_SCREEN_H - UI_HEADER_H - 58);
    lv_obj_set_style_bg_opa(btn_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_area, 0, 0);
    lv_obj_set_style_pad_all(btn_area, 0, 0);
    lv_obj_clear_flag(btn_area, LV_OBJ_FLAG_SCROLLABLE);
    ui_common_btn_danger(btn_area, "RE-PROVISION WIFI (restarts device)", on_reprovision, NULL);
#else
    lv_obj_t *msg = lv_label_create(page);
    lv_label_set_text(msg, "Connectivity details for this protocol\nare not implemented yet.");
    lv_obj_set_style_text_color(msg, lv_color_hex(t->text_secondary), 0);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(msg, UI_SCREEN_W - 40);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, 0);
#endif
}

static void refresh_network_live(void)
{
#ifdef CONFIG_PROTOCOL_MQTT
    if (!s_signal_icon && !s_signal_dbm_label && !s_ip_value_label && !s_mac_value_label && !s_network_value_label) {
        return;
    }

    const hestia_theme_t *t = ui_theme_get();
    connectivity_snapshot_t snap;
    collect_snapshot(&snap);

    if (s_network_value_label) {
        char network_value[64];
        snprintf(network_value, sizeof(network_value), "WiFi - %s", snap.ssid);
        lv_label_set_text(s_network_value_label, network_value);
    }

    if (s_signal_dbm_label) {
        if (snap.connected) {
            lv_label_set_text_fmt(s_signal_dbm_label, "%d dBm", snap.rssi);
            lv_obj_set_style_text_color(s_signal_dbm_label, lv_color_hex(t->text), 0);
        } else {
            lv_label_set_text(s_signal_dbm_label, "offline");
            lv_obj_set_style_text_color(s_signal_dbm_label, lv_color_hex(t->text_secondary), 0);
        }
    }

    if (s_signal_icon) {
        lv_obj_set_style_text_color(s_signal_icon,
                                    lv_color_hex(signal_color(snap.connected, snap.rssi)),
                                    0);
    }

    if (s_ip_value_label) {
        lv_label_set_text(s_ip_value_label, snap.ip);
        lv_obj_set_style_text_color(s_ip_value_label,
                                    lv_color_hex(snap.connected ? t->text : t->text_secondary),
                                    0);
    }

    if (s_mac_value_label) {
        lv_label_set_text(s_mac_value_label, snap.mac);
        lv_obj_set_style_text_color(s_mac_value_label,
                                    lv_color_hex(snap.connected ? t->text : t->text_secondary),
                                    0);
    }
#endif
}

static void on_refresh_timer(lv_timer_t *t)
{
    (void)t;
    refresh_network_live();
}

static void create_page_mqtt(lv_obj_t *page, const connectivity_snapshot_t *snap)
{
    const hestia_theme_t *t = ui_theme_get();
    const protocol_interface_t *iface = protocol_manager_get_interface();
    const char *proto_name = iface ? iface->get_name() : "Unknown";

    lv_obj_clean(page);
    lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(page);
    lv_label_set_text(title, "MQTT");
    lv_obj_set_style_text_color(title, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(title, UI_PAD, 10);

    char info[420];
#ifdef CONFIG_PROTOCOL_MQTT
    snprintf(info, sizeof(info),
             "Protocol: %s\n"
             "Network: WiFi - %s\n"
             "Broker/Server: %s\n"
             "Client ID: %s\n"
             "Status: %s\n\n"
             "Auth/topic fields are not configured\n"
             "in this firmware yet.",
             proto_name,
             snap->ssid,
             snap->server_url,
             snap->node_name,
             snap->connected ? "Connected" : (snap->provisioned ? "Provisioned, offline" : "Not provisioned"));
#else
    snprintf(info, sizeof(info),
             "Protocol: %s\n\n"
             "MQTT page is available only\n"
             "in MQTT builds.",
             proto_name);
#endif

    lv_obj_t *body = lv_label_create(page);
    lv_label_set_text(body, info);
    lv_obj_set_style_text_color(body, lv_color_hex(t->text_secondary), 0);
    lv_obj_set_style_text_font(body, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(body, UI_SCREEN_W - (UI_PAD * 2));
    lv_obj_set_pos(body, UI_PAD, 44);
}

static void create_page_mqtt_placeholder(lv_obj_t *page, const char *msg)
{
    const hestia_theme_t *t = ui_theme_get();
    lv_obj_clean(page);
    lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *note = lv_label_create(page);
    lv_label_set_text(note, msg);
    lv_obj_set_style_text_color(note, lv_color_hex(t->text_secondary), 0);
    lv_obj_set_style_text_font(note, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(note, UI_SCREEN_W - 32);
    lv_obj_align(note, LV_ALIGN_CENTER, 0, 0);
}

void ui_settings_protocol_open(void)
{
    const hestia_theme_t *t = ui_theme_get();
    s_network_value_label = NULL;
    s_signal_dbm_label = NULL;
    s_signal_icon = NULL;
    s_ip_value_label = NULL;
    s_mac_value_label = NULL;

    connectivity_snapshot_t snap;
    collect_snapshot(&snap);
    s_current_page = 0;
    s_mqtt_page_built = false;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(scr, lv_color_hex(t->bg), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, on_screen_delete, LV_EVENT_DELETE, NULL);

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

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "Connectivity");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(t->text), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

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

    lv_obj_t *page_container = lv_obj_create(scr);
    lv_obj_set_size(page_container, UI_SCREEN_W, UI_SCREEN_H - UI_HEADER_H);
    lv_obj_align(page_container, LV_ALIGN_TOP_LEFT, 0, UI_HEADER_H);
    lv_obj_set_style_bg_color(page_container, lv_color_hex(t->bg), 0);
    lv_obj_set_style_border_width(page_container, 0, 0);
    lv_obj_set_style_pad_all(page_container, 0, 0);
    lv_obj_clear_flag(page_container, LV_OBJ_FLAG_SCROLLABLE);

    s_pages[0] = lv_obj_create(page_container);
    lv_obj_set_size(s_pages[0], LV_PCT(100), LV_PCT(100));
    create_page_network(s_pages[0], &snap);

    s_pages[1] = lv_obj_create(page_container);
    lv_obj_set_size(s_pages[1], LV_PCT(100), LV_PCT(100));
    create_page_mqtt_placeholder(s_pages[1], "Loading MQTT details...");
    lv_obj_add_flag(s_pages[1], LV_OBJ_FLAG_HIDDEN);

    update_page_navigation();

    if (s_refresh_timer) {
        lv_timer_del(s_refresh_timer);
        s_refresh_timer = NULL;
    }
    s_refresh_timer = lv_timer_create(on_refresh_timer, 5000, NULL);
    refresh_network_live();

    ui_common_push_screen(scr);
}
