#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "blink_test";

// XIAO ESP32-C5 built-in LED is on GPIO 6
#define BLINK_GPIO 27

void app_main(void)
{
    ESP_LOGI(TAG, "Blink test starting...");

    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    int level = 0;
    while (1) {
        ESP_LOGI(TAG, "LED %s", level ? "ON" : "OFF");
        gpio_set_level(BLINK_GPIO, level);
        level = !level;
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}
