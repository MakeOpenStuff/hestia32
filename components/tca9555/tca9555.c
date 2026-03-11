#include "tca9555.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "tca9555";

// Mutex for thread-safe I2C access
static SemaphoreHandle_t tca9555_mutex = NULL;

// Shadow registers to minimize I2C transactions
static uint8_t shadow_output[2] = {0xFF, 0xFF};  // Port 0 and Port 1
static uint8_t shadow_config[2] = {0xFF, 0xFF};  // Port 0 and Port 1 (all inputs by default)

static esp_err_t tca9555_write_reg(i2c_port_t i2c_port, uint8_t i2c_addr, uint8_t reg, uint8_t value)
{
    uint8_t write_buf[2] = {reg, value};
    return i2c_master_write_to_device(i2c_port, i2c_addr, write_buf, 2, pdMS_TO_TICKS(100));
}

static esp_err_t tca9555_read_reg(i2c_port_t i2c_port, uint8_t i2c_addr, uint8_t reg, uint8_t *value)
{
    return i2c_master_write_read_device(i2c_port, i2c_addr, &reg, 1, value, 1, pdMS_TO_TICKS(100));
}

esp_err_t tca9555_init(i2c_port_t i2c_port, int sda_pin, int scl_pin, uint8_t i2c_addr)
{
    esp_err_t ret;

    // Create mutex if not already created
    if (tca9555_mutex == NULL) {
        tca9555_mutex = xSemaphoreCreateMutex();
        if (tca9555_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    // Configure I2C master
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,  // 100kHz
    };

    ret = i2c_param_config(i2c_port, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(i2c_port, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2C initialized (SDA: GPIO%d, SCL: GPIO%d)", sda_pin, scl_pin);

    // Probe device
    if (!tca9555_probe(i2c_port, i2c_addr)) {
        ESP_LOGE(TAG, "TCA9555 not found at address 0x%02X", i2c_addr);
        i2c_driver_delete(i2c_port);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "TCA9555 found at address 0x%02X", i2c_addr);

    // Initialize shadow registers from device
    tca9555_read_reg(i2c_port, i2c_addr, TCA9555_REG_OUTPUT_PORT0, &shadow_output[0]);
    tca9555_read_reg(i2c_port, i2c_addr, TCA9555_REG_OUTPUT_PORT1, &shadow_output[1]);
    tca9555_read_reg(i2c_port, i2c_addr, TCA9555_REG_CONFIG_PORT0, &shadow_config[0]);
    tca9555_read_reg(i2c_port, i2c_addr, TCA9555_REG_CONFIG_PORT1, &shadow_config[1]);

    return ESP_OK;
}

esp_err_t tca9555_deinit(i2c_port_t i2c_port)
{
    return i2c_driver_delete(i2c_port);
}

bool tca9555_probe(i2c_port_t i2c_port, uint8_t i2c_addr)
{
    uint8_t data;
    esp_err_t ret = i2c_master_write_read_device(i2c_port, i2c_addr,
                                                  (uint8_t[]){TCA9555_REG_INPUT_PORT0}, 1,
                                                  &data, 1, pdMS_TO_TICKS(100));
    return (ret == ESP_OK);
}

esp_err_t tca9555_set_pin_mode(i2c_port_t i2c_port, uint8_t i2c_addr, uint8_t pin, uint8_t direction)
{
    if (pin > 15) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(tca9555_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_ERR_TIMEOUT;
    }

    uint8_t port = pin / 8;
    uint8_t bit = pin % 8;
    uint8_t reg = (port == 0) ? TCA9555_REG_CONFIG_PORT0 : TCA9555_REG_CONFIG_PORT1;

    if (direction == TCA9555_PIN_INPUT) {
        shadow_config[port] |= (1 << bit);
    } else {
        shadow_config[port] &= ~(1 << bit);
    }

    esp_err_t ret = tca9555_write_reg(i2c_port, i2c_addr, reg, shadow_config[port]);

    xSemaphoreGive(tca9555_mutex);
    return ret;
}

esp_err_t tca9555_set_pin_level(i2c_port_t i2c_port, uint8_t i2c_addr, uint8_t pin, uint8_t level)
{
    if (pin > 15) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(tca9555_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_ERR_TIMEOUT;
    }

    uint8_t port = pin / 8;
    uint8_t bit = pin % 8;
    uint8_t reg = (port == 0) ? TCA9555_REG_OUTPUT_PORT0 : TCA9555_REG_OUTPUT_PORT1;

    if (level) {
        shadow_output[port] |= (1 << bit);
    } else {
        shadow_output[port] &= ~(1 << bit);
    }

    esp_err_t ret = tca9555_write_reg(i2c_port, i2c_addr, reg, shadow_output[port]);

    xSemaphoreGive(tca9555_mutex);
    return ret;
}

esp_err_t tca9555_get_pin_level(i2c_port_t i2c_port, uint8_t i2c_addr, uint8_t pin, uint8_t *level)
{
    if (pin > 15 || level == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t port = pin / 8;
    uint8_t bit = pin % 8;
    uint8_t reg = (port == 0) ? TCA9555_REG_INPUT_PORT0 : TCA9555_REG_INPUT_PORT1;

    uint8_t port_value;
    esp_err_t ret = tca9555_read_reg(i2c_port, i2c_addr, reg, &port_value);
    if (ret == ESP_OK) {
        *level = (port_value >> bit) & 0x01;
    }

    return ret;
}

esp_err_t tca9555_set_ports(i2c_port_t i2c_port, uint8_t i2c_addr, uint8_t port0_value, uint8_t port1_value)
{
    if (xSemaphoreTake(tca9555_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_ERR_TIMEOUT;
    }

    shadow_output[0] = port0_value;
    shadow_output[1] = port1_value;

    esp_err_t ret = tca9555_write_reg(i2c_port, i2c_addr, TCA9555_REG_OUTPUT_PORT0, port0_value);
    if (ret == ESP_OK) {
        ret = tca9555_write_reg(i2c_port, i2c_addr, TCA9555_REG_OUTPUT_PORT1, port1_value);
    }

    xSemaphoreGive(tca9555_mutex);
    return ret;
}
