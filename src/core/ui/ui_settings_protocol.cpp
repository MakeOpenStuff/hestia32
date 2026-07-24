#include "ui/ui_settings_protocol.h"
#include "ui/ui_common.h"
#include "ui/ui_theme.h"
#include "ui/ui_icons.h"
extern "C" {
#include "core/protocol_manager.h"
}
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

#ifdef CONFIG_PROTOCOL_MQTT
extern "C" {
#include "protocols/mqtt/wifi_manager.h"
#include "protocols/mqtt/wifi_provisioning.h"
}
#include "esp_wifi.h"
#include "esp_netif.h"
#endif

static const char *TAG = "ui_protocol";

static void on_back(lv_event_t *e)  { (void)e; ui_common_pop_screen(); }

static void on_reprovision(lv_event_t *e)
{
    (void)e;
#ifdef CONFIG_PROTOCOL_MQTT
    ESP_LOGI(TAG, "User requested re-provisioning");
    wifi_prov_reset();
    esp_restart();
#endif
}

void ui_settings_protocol_open(void)
{
    const hestia_theme_t *t = ui_theme_get();

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(scr, lv_color_hex(t->bg), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    ui_common_header(scr, "Connectivity", on_back, NULL);

    lv_obj_t *body = lv_obj_create(scr);
    lv_obj_set_size(body, UI_SCREEN_W, UI_SCREEN_H - UI_HEADER_H - 60);
    lv_obj_align(body, LV_ALIGN_TOP_LEFT, 0, UI_HEADER_H);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_all(body, UI_PAD, 0);
    lv_obj_set_style_radius(body, 0, 0);
    lv_obj_set_layout(body, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    /* Protocol name */
    const protocol_interface_t *iface = protocol_manager_get_interface();
    const char *proto_name = iface ? iface->get_name() : "Unknown";

    lv_obj_t *proto_row = lv_obj_create(body);
    lv_obj_set_size(proto_row, LV_PCT(100), 36);
    lv_obj_set_style_bg_opa(proto_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(proto_row, 0, 0);
    lv_obj_set_style_pad_all(proto_row, 0, 0);
    lv_obj_set_style_radius(proto_row, 0, 0);
    lv_obj_clear_flag(proto_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ptitle = lv_label_create(proto_row);
    lv_label_set_text(ptitle, "Protocol");
    lv_obj_set_style_text_color(ptitle, lv_color_hex(t->text_secondary), 0);
    lv_obj_set_style_text_font(ptitle, &lv_font_montserrat_14, 0);
    lv_obj_align(ptitle, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *pval = lv_label_create(proto_row);
    lv_label_set_text(pval, proto_name);
    lv_obj_set_style_text_color(pval, lv_color_hex(t->accent), 0);
    lv_obj_set_style_text_font(pval, &lv_font_montserrat_16, 0);
    lv_obj_align(pval, LV_ALIGN_RIGHT_MID, 0, 0);

    ui_common_divider(body);

#ifdef CONFIG_PROTOCOL_MQTT
    /* WiFi: SSID, IP, MAC */
    char ssid[33]  = "-";
    char ip_str[20]= "-";
    char mac_str[20]= "-";

    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        strncpy(ssid, (char*)ap_info.ssid, sizeof(ssid)-1);
    }

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
        }
        uint8_t mac[6];
        if (esp_netif_get_mac(netif, mac) == ESP_OK) {
            snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
        }
    }

    /* Info rows */
    const char *labels[] = { "SSID", "IP Address", "MAC" };
    const char *values[] = { ssid, ip_str, mac_str };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *row = lv_obj_create(body);
        lv_obj_set_size(row, LV_PCT(100), 36);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
        lv_obj_set_style_border_color(row, lv_color_hex(t->border), LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *k = lv_label_create(row);
        lv_label_set_text(k, labels[i]);
        lv_obj_set_style_text_color(k, lv_color_hex(t->text_secondary), 0);
        lv_obj_set_style_text_font(k, &lv_font_montserrat_14, 0);
        lv_obj_align(k, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *v = lv_label_create(row);
        lv_label_set_text(v, values[i]);
        lv_obj_set_style_text_color(v, lv_color_hex(t->text), 0);
        lv_obj_set_style_text_font(v, &lv_font_montserrat_14, 0);
        lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);
    }
#else
    lv_obj_t *note = lv_label_create(body);
    lv_label_set_text(note, "Protocol not yet implemented");
    lv_obj_set_style_text_color(note, lv_color_hex(t->text_secondary), 0);
    lv_obj_set_style_text_font(note, &lv_font_montserrat_14, 0);
#endif

    /* Re-provision button (MQTT only) */
    lv_obj_t *btn_area = lv_obj_create(scr);
    lv_obj_set_size(btn_area, UI_SCREEN_W, 52);
    lv_obj_align(btn_area, LV_ALIGN_BOTTOM_LEFT, 0, -10);
    lv_obj_set_style_bg_opa(btn_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_area, 0, 0);
    lv_obj_set_style_pad_all(btn_area, UI_PAD, 0);
    lv_obj_clear_flag(btn_area, LV_OBJ_FLAG_SCROLLABLE);
#ifdef CONFIG_PROTOCOL_MQTT
    ui_common_btn_danger(btn_area, "RE-PROVISION (restarts device)", on_reprovision, NULL);
#endif

    ui_common_push_screen(scr);
}
