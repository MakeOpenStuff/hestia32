#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "i2c_scanner.h"

static const char *TAG = "i2c_scanner_test";

#define XIAO_I2C_SCL_GPIO 24
#define XIAO_I2C_SDA_GPIO 23

void app_main(void)
{
    ESP_LOGI(TAG, "Standalone I2C scanner test starting...");
    ESP_LOGI(TAG, "Using XIAO pins SCL=%d SDA=%d", XIAO_I2C_SCL_GPIO, XIAO_I2C_SDA_GPIO);

    while (1) {
        i2c_scanner_scan(XIAO_I2C_SCL_GPIO, XIAO_I2C_SDA_GPIO);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
