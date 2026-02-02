#include "protocols/matter/protocol_matter.h"
#include "esp_log.h"

static const char *TAG = "matter_protocol";

// Matter protocol stub implementation
static esp_err_t matter_init(void) {
    ESP_LOGW(TAG, "Matter protocol not yet implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t matter_start(void) {
    ESP_LOGW(TAG, "Matter protocol not yet implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t matter_stop(void) {
    ESP_LOGW(TAG, "Matter protocol not yet implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t matter_report_temperature(float temperature) {
    (void)temperature;
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t matter_report_humidity(float humidity) {
    (void)humidity;
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t matter_report_setpoint(float heat_setpoint, float cool_setpoint) {
    (void)heat_setpoint;
    (void)cool_setpoint;
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t matter_report_mode(const char* mode) {
    (void)mode;
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t matter_report_relay_state(bool heating, bool cooling, bool fan,
                                           bool humidifier, bool dehumidifier, bool hot_water) {
    (void)heating; (void)cooling; (void)fan;
    (void)humidifier; (void)dehumidifier; (void)hot_water;
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t matter_report_boost_state(const char* domain, uint32_t remaining_sec) {
    (void)domain;
    (void)remaining_sec;
    return ESP_ERR_NOT_SUPPORTED;
}

static bool matter_is_connected(void) {
    return false;
}

static bool matter_is_provisioned(void) {
    return false;
}

static esp_err_t matter_start_provisioning(void) {
    ESP_LOGW(TAG, "Matter commissioning not yet implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

static const char* matter_get_name(void) {
    return "Matter";
}

// Protocol interface for Matter
static const protocol_interface_t matter_protocol = {
    .init = matter_init,
    .start = matter_start,
    .stop = matter_stop,
    .report_temperature = matter_report_temperature,
    .report_humidity = matter_report_humidity,
    .report_setpoint = matter_report_setpoint,
    .report_mode = matter_report_mode,
    .report_relay_state = matter_report_relay_state,
    .report_boost_state = matter_report_boost_state,
    .is_connected = matter_is_connected,
    .is_provisioned = matter_is_provisioned,
    .start_provisioning = matter_start_provisioning,
    .get_name = matter_get_name,
};

const protocol_interface_t* protocol_get_matter(void) {
    return &matter_protocol;
}
