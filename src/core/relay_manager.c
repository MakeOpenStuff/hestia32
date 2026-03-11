#include "relay_manager.h"
#include "core_config.h"
#include "esp_log.h"
#include "driver/gpio.h"

#if RELAY_USE_TCA9555
#include "tca9555.h"
#endif

static const char *TAG = "relay_manager";

static bool relay_states[RELAY_COUNT] = {false};
static bool initialized = false;

#if RELAY_USE_TCA9555
static bool use_tca9555 = false;
#endif

esp_err_t relay_manager_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing relay control...");

#if RELAY_USE_TCA9555
    // Try to initialize TCA9555 I2C GPIO expander
    ESP_LOGI(TAG, "Probing TCA9555 at address 0x%02X (SDA=GPIO%d, SCL=GPIO%d)...",
             TCA9555_I2C_ADDR, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);

    esp_err_t ret = tca9555_init(I2C_MASTER_NUM, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, TCA9555_I2C_ADDR);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "TCA9555 detected! Using I2C GPIO expander mode");
        use_tca9555 = true;

        // Configure relay pins as outputs (active LOW for most relay modules)
        for (int i = 0; i < RELAY_COUNT; i++) {
            uint8_t pin = RELAY_TCA9555_PIN_BASE + i;
            tca9555_set_pin_mode(I2C_MASTER_NUM, TCA9555_I2C_ADDR, pin, TCA9555_PIN_OUTPUT);
            tca9555_set_pin_level(I2C_MASTER_NUM, TCA9555_I2C_ADDR, pin, 1);  // OFF (active LOW)
            relay_states[i] = false;
        }

        ESP_LOGI(TAG, "TCA9555 relay pins configured (pins %d-%d)",
                 RELAY_TCA9555_PIN_BASE, RELAY_TCA9555_PIN_BASE + RELAY_COUNT - 1);
    } else {
        ESP_LOGW(TAG, "TCA9555 not detected, TCA9555 features not available");
        // Don't fail - XIAO can still work without relays for testing
    }
#else
    // Direct GPIO mode (DevKit)
    ESP_LOGI(TAG, "Using direct GPIO mode");

    gpio_config_t io_conf = {
        .pin_bit_mask = ((1ULL << RELAY_GPIO_1) |
                         (1ULL << RELAY_GPIO_2) |
                         (1ULL << RELAY_GPIO_3) |
                         (1ULL << RELAY_GPIO_4)),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed");
        return ret;
    }

    // Initialize all relays to OFF (active LOW)
    gpio_set_level(RELAY_GPIO_1, 1);
    gpio_set_level(RELAY_GPIO_2, 1);
    gpio_set_level(RELAY_GPIO_3, 1);
    gpio_set_level(RELAY_GPIO_4, 1);

    for (int i = 0; i < RELAY_COUNT; i++) {
        relay_states[i] = false;
    }

    ESP_LOGI(TAG, "Direct GPIO relay pins configured (GPIOs %d,%d,%d,%d)",
             RELAY_GPIO_1, RELAY_GPIO_2, RELAY_GPIO_3, RELAY_GPIO_4);
#endif

    initialized = true;
    ESP_LOGI(TAG, "Relay manager initialized successfully");
    return ESP_OK;
}

esp_err_t relay_manager_deinit(void)
{
    if (!initialized) {
        return ESP_OK;
    }

#if RELAY_USE_TCA9555
    if (use_tca9555) {
        // Turn off all relays before deinitializing
        for (int i = 0; i < RELAY_COUNT; i++) {
            uint8_t pin = RELAY_TCA9555_PIN_BASE + i;
            tca9555_set_pin_level(I2C_MASTER_NUM, TCA9555_I2C_ADDR, pin, 1);
        }
        tca9555_deinit(I2C_MASTER_NUM);
    }
#else
    // Turn off all relays
    gpio_set_level(RELAY_GPIO_1, 1);
    gpio_set_level(RELAY_GPIO_2, 1);
    gpio_set_level(RELAY_GPIO_3, 1);
    gpio_set_level(RELAY_GPIO_4, 1);
#endif

    initialized = false;
    ESP_LOGI(TAG, "Relay manager deinitialized");
    return ESP_OK;
}

esp_err_t relay_manager_set_relay(uint8_t relay_num, bool state)
{
    if (!initialized) {
        ESP_LOGE(TAG, "Relay manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (relay_num >= RELAY_COUNT) {
        ESP_LOGE(TAG, "Invalid relay number: %d", relay_num);
        return ESP_ERR_INVALID_ARG;
    }

    // Active LOW logic: 0=ON, 1=OFF
    uint8_t level = state ? 0 : 1;

#if RELAY_USE_TCA9555
    if (use_tca9555) {
        uint8_t pin = RELAY_TCA9555_PIN_BASE + relay_num;
        esp_err_t ret = tca9555_set_pin_level(I2C_MASTER_NUM, TCA9555_I2C_ADDR, pin, level);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set TCA9555 pin %d", pin);
            return ret;
        }
        relay_states[relay_num] = state;
        ESP_LOGI(TAG, "Relay %d (TCA9555 pin %d): %s", relay_num, pin, state ? "ON" : "OFF");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "TCA9555 not available, relay %d not controlled", relay_num);
        return ESP_ERR_NOT_SUPPORTED;
    }
#else
    // Direct GPIO mode
    gpio_num_t gpio_pin;
    switch (relay_num) {
        case 0: gpio_pin = RELAY_GPIO_1; break;
        case 1: gpio_pin = RELAY_GPIO_2; break;
        case 2: gpio_pin = RELAY_GPIO_3; break;
        case 3: gpio_pin = RELAY_GPIO_4; break;
        default: return ESP_ERR_INVALID_ARG;
    }

    gpio_set_level(gpio_pin, level);
    relay_states[relay_num] = state;
    ESP_LOGI(TAG, "Relay %d (GPIO %d): %s", relay_num, gpio_pin, state ? "ON" : "OFF");
    return ESP_OK;
#endif
}

esp_err_t relay_manager_get_relay(uint8_t relay_num, bool *state)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (relay_num >= RELAY_COUNT || state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *state = relay_states[relay_num];
    return ESP_OK;
}

esp_err_t relay_manager_set_all(uint8_t states)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

#if RELAY_USE_TCA9555
    if (use_tca9555) {
        // Set all relay pins at once (active LOW)
        uint8_t port_value = 0xFF;  // Start with all OFF
        for (int i = 0; i < RELAY_COUNT; i++) {
            if (states & (1 << i)) {
                // Turn relay ON (active LOW)
                port_value &= ~(1 << (RELAY_TCA9555_PIN_BASE + i));
                relay_states[i] = true;
            } else {
                relay_states[i] = false;
            }
        }

        uint8_t port0 = port_value & 0xFF;
        uint8_t port1 = (port_value >> 8) & 0xFF;
        return tca9555_set_ports(I2C_MASTER_NUM, TCA9555_I2C_ADDR, port0, port1);
    } else {
        ESP_LOGW(TAG, "TCA9555 not available");
        return ESP_ERR_NOT_SUPPORTED;
    }
#else
    // Direct GPIO mode
    for (int i = 0; i < RELAY_COUNT; i++) {
        bool state = (states & (1 << i)) != 0;
        relay_manager_set_relay(i, state);
    }
    return ESP_OK;
#endif
}

bool relay_manager_using_tca9555(void)
{
#if RELAY_USE_TCA9555
    return use_tca9555;
#else
    return false;
#endif
}
