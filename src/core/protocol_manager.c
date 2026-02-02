#include "protocol_manager.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "protocol_manager";

// Active protocol interface
static const protocol_interface_t* active_protocol = NULL;

// Application callbacks
static protocol_callbacks_t app_callbacks = {0};

// Forward declarations for protocol getter functions
#ifdef CONFIG_PROTOCOL_MQTT
extern const protocol_interface_t* protocol_get_mqtt(void);
#endif

#ifdef CONFIG_PROTOCOL_ZIGBEE
extern const protocol_interface_t* protocol_get_zigbee(void);
#endif

#ifdef CONFIG_PROTOCOL_MATTER
extern const protocol_interface_t* protocol_get_matter(void);
#endif

esp_err_t protocol_manager_init(void) {
    ESP_LOGI(TAG, "Initializing protocol manager");

    // Select protocol based on build configuration
#ifdef CONFIG_PROTOCOL_MQTT
    active_protocol = protocol_get_mqtt();
    ESP_LOGI(TAG, "Selected protocol: MQTT");
#elif defined(CONFIG_PROTOCOL_ZIGBEE)
    active_protocol = protocol_get_zigbee();
    ESP_LOGI(TAG, "Selected protocol: Zigbee");
#elif defined(CONFIG_PROTOCOL_MATTER)
    active_protocol = protocol_get_matter();
    ESP_LOGI(TAG, "Selected protocol: Matter");
#else
    #error "No protocol selected! Define CONFIG_PROTOCOL_MQTT, CONFIG_PROTOCOL_ZIGBEE, or CONFIG_PROTOCOL_MATTER"
#endif

    if (active_protocol == NULL) {
        ESP_LOGE(TAG, "Failed to get protocol interface");
        return ESP_ERR_INVALID_STATE;
    }

    // Verify required functions are implemented
    if (active_protocol->init == NULL ||
        active_protocol->start == NULL ||
        active_protocol->get_name == NULL) {
        ESP_LOGE(TAG, "Protocol interface incomplete");
        return ESP_ERR_INVALID_STATE;
    }

    // Initialize the protocol
    esp_err_t err = active_protocol->init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Protocol initialization failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Protocol manager initialized successfully");
    return ESP_OK;
}

esp_err_t protocol_manager_start(void) {
    if (active_protocol == NULL) {
        ESP_LOGE(TAG, "Protocol manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting protocol: %s", active_protocol->get_name());

    esp_err_t err = active_protocol->start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Protocol start failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Protocol started successfully");
    return ESP_OK;
}

esp_err_t protocol_manager_stop(void) {
    if (active_protocol == NULL) {
        ESP_LOGE(TAG, "Protocol manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (active_protocol->stop == NULL) {
        ESP_LOGW(TAG, "Protocol does not support stop operation");
        return ESP_ERR_NOT_SUPPORTED;
    }

    ESP_LOGI(TAG, "Stopping protocol");
    return active_protocol->stop();
}

esp_err_t protocol_manager_register_callbacks(const protocol_callbacks_t* callbacks) {
    if (callbacks == NULL) {
        ESP_LOGE(TAG, "Invalid callbacks pointer");
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&app_callbacks, callbacks, sizeof(protocol_callbacks_t));
    ESP_LOGI(TAG, "Application callbacks registered");
    return ESP_OK;
}

const protocol_interface_t* protocol_manager_get_interface(void) {
    return active_protocol;
}

bool protocol_manager_is_connected(void) {
    if (active_protocol == NULL) {
        return false;
    }

    if (active_protocol->is_connected == NULL) {
        return false;
    }

    return active_protocol->is_connected();
}

const char* protocol_manager_get_name(void) {
    if (active_protocol == NULL || active_protocol->get_name == NULL) {
        return "Unknown";
    }

    return active_protocol->get_name();
}

// Helper functions for protocols to invoke application callbacks
void protocol_manager_invoke_setpoint_callback(float heat_sp, float cool_sp) {
    if (app_callbacks.on_setpoint_change != NULL) {
        app_callbacks.on_setpoint_change(heat_sp, cool_sp);
    }
}

void protocol_manager_invoke_mode_callback(const char* mode) {
    if (app_callbacks.on_mode_change != NULL) {
        app_callbacks.on_mode_change(mode);
    }
}

void protocol_manager_invoke_boost_callback(const char* domain, uint32_t duration) {
    if (app_callbacks.on_boost_command != NULL) {
        app_callbacks.on_boost_command(domain, duration);
    }
}
