#include "protocols/zigbee/protocol_zigbee.h"
#include "esp_log.h"

static const char *TAG = "zigbee_protocol";

// Zigbee protocol stub implementation
static esp_err_t zigbee_init(void) {
    ESP_LOGW(TAG, "Zigbee protocol not yet implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t zigbee_start(void) {
    ESP_LOGW(TAG, "Zigbee protocol not yet implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t zigbee_stop(void) {
    ESP_LOGW(TAG, "Zigbee protocol not yet implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t zigbee_report_temperature(float temperature) {
    (void)temperature;
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t zigbee_report_humidity(float humidity) {
    (void)humidity;
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t zigbee_report_setpoint(float heat_setpoint, float cool_setpoint) {
    (void)heat_setpoint;
    (void)cool_setpoint;
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t zigbee_report_mode(const char* mode) {
    (void)mode;
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t zigbee_report_relay_state(bool heating, bool cooling, bool fan,
                                           bool humidifier, bool dehumidifier, bool hot_water) {
    (void)heating; (void)cooling; (void)fan;
    (void)humidifier; (void)dehumidifier; (void)hot_water;
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t zigbee_report_boost_state(const char* domain, uint32_t remaining_sec) {
    (void)domain;
    (void)remaining_sec;
    return ESP_ERR_NOT_SUPPORTED;
}

static bool zigbee_is_connected(void) {
    return false;
}

static bool zigbee_is_provisioned(void) {
    return false;
}

static esp_err_t zigbee_start_provisioning(void) {
    ESP_LOGW(TAG, "Zigbee provisioning not yet implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

static const char* zigbee_get_name(void) {
    return "Zigbee";
}

// Protocol interface for Zigbee
static const protocol_interface_t zigbee_protocol = {
    .init = zigbee_init,
    .start = zigbee_start,
    .stop = zigbee_stop,
    .report_temperature = zigbee_report_temperature,
    .report_humidity = zigbee_report_humidity,
    .report_setpoint = zigbee_report_setpoint,
    .report_mode = zigbee_report_mode,
    .report_relay_state = zigbee_report_relay_state,
    .report_boost_state = zigbee_report_boost_state,
    .is_connected = zigbee_is_connected,
    .is_provisioned = zigbee_is_provisioned,
    .start_provisioning = zigbee_start_provisioning,
    .get_name = zigbee_get_name,
};

const protocol_interface_t* protocol_get_zigbee(void) {
    return &zigbee_protocol;
}
