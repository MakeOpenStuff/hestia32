#include "rgb_led_test.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "rgb_led_test";

// RMT WS2812 configuration
#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us

static rmt_channel_handle_t led_chan = NULL;
static rmt_encoder_handle_t led_encoder = NULL;

// WS2812 uses rmt_symbol_word_t (correct ESP-IDF v5.x structure)
static void ws2812_set_color(uint8_t r, uint8_t g, uint8_t b)
{
		// WS2812 timing: T0H=0.4us, T0L=0.85us, T1H=0.8us, T1L=0.45us
		// At 10MHz: T0H=4, T0L=8, T1H=8, T1L=4
		rmt_symbol_word_t bit0 = {
				.level0 = 1,
				.duration0 = 4,  // 0.4us
				.level1 = 0,
				.duration1 = 8,  // 0.8us
		};

		rmt_symbol_word_t bit1 = {
				.level0 = 1,
				.duration0 = 8,  // 0.8us
				.level1 = 0,
				.duration1 = 4,  // 0.4us
		};

		rmt_symbol_word_t reset = {
				.level0 = 0,
				.duration0 = 500,  // 50us reset
				.level1 = 0,
				.duration1 = 0,
		};

		// Build GRB data (24 bits + reset)
		rmt_symbol_word_t symbols[25];
		uint8_t bytes[3] = {g, r, b};  // WS2812 wants GRB order

		for (int byte_idx = 0; byte_idx < 3; byte_idx++) {
				uint8_t byte = bytes[byte_idx];
				for (int bit = 7; bit >= 0; bit--) {
						symbols[byte_idx * 8 + (7 - bit)] = (byte & (1 << bit)) ? bit1 : bit0;
				}
		}
		symbols[24] = reset;  // Add reset pulse

		rmt_transmit_config_t tx_config = {
				.loop_count = 0,
		};

		rmt_transmit(led_chan, led_encoder, symbols, sizeof(symbols), &tx_config);
		rmt_tx_wait_all_done(led_chan, -1);
}

esp_err_t rgb_led_test_init(void)
{
		ESP_LOGI(TAG, "========================================");
		ESP_LOGI(TAG, "RGB LED Test Initialization");
		ESP_LOGI(TAG, "Initializing WS2812 on GPIO %d (simple RMT)", RGB_LED_GPIO);
		ESP_LOGI(TAG, "========================================");

		// Configure RMT TX channel
		rmt_tx_channel_config_t tx_chan_config = {
				.clk_src = RMT_CLK_SRC_DEFAULT,
				.gpio_num = RGB_LED_GPIO,
				.mem_block_symbols = 64,
				.resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
				.trans_queue_depth = 4,
				.flags.invert_out = false,
				.flags.with_dma = false,
		};

		esp_err_t ret = rmt_new_tx_channel(&tx_chan_config, &led_chan);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Failed to create RMT TX channel: %s", esp_err_to_name(ret));
				return ret;
		}

		// Simple copy encoder
		rmt_copy_encoder_config_t encoder_config = {};
		ret = rmt_new_copy_encoder(&encoder_config, &led_encoder);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Failed to create RMT encoder: %s", esp_err_to_name(ret));
				return ret;
		}

		ret = rmt_enable(led_chan);
		if (ret != ESP_OK) {
				ESP_LOGE(TAG, "Failed to enable RMT channel: %s", esp_err_to_name(ret));
				return ret;
		}

		ESP_LOGI(TAG, "WS2812 LED initialized successfully (no external components)");

		// Clear LED
		ws2812_set_color(0, 0, 0);
		return ESP_OK;
}

void rgb_led_test_run(void)
{
		ESP_LOGI(TAG, "========================================");
		ESP_LOGI(TAG, "Starting WS2812 RGB LED color cycle");
		ESP_LOGI(TAG, "GPIO: %d (Direct RMT control)", RGB_LED_GPIO);
		ESP_LOGI(TAG, "========================================");

		typedef struct {
				const char *name;
				uint8_t r, g, b;
				uint32_t duration_ms;
		} color_t;

		const color_t colors[] = {
				{"OFF",     0,   0,   0,   1000},
				{"RED",     255, 0,   0,   2000},
				{"GREEN",   0,   255, 0,   2000},
				{"BLUE",    0,   0,   255, 2000},
				{"YELLOW",  255, 255, 0,   2000},
				{"CYAN",    0,   255, 255, 2000},
				{"MAGENTA", 255, 0,   255, 2000},
				{"WHITE",   255, 255, 255, 2000},
				{"DIM RED", 32,  0,   0,   1000},
				{"DIM GRN", 0,   32,  0,   1000},
				{"DIM BLU", 0,   0,   32,  1000},
		};

		const int num_colors = sizeof(colors) / sizeof(colors[0]);

		while (1) {
				for (int i = 0; i < num_colors; i++) {
						ws2812_set_color(colors[i].r, colors[i].g, colors[i].b);
						vTaskDelay(pdMS_TO_TICKS(colors[i].duration_ms));
				}

				ESP_LOGI(TAG, "--- Cycle complete, restarting ---");
		}
}
