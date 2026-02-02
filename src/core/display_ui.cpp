#include "core/display_ui.h"
#include "core/display_config.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "display_ui";

void display_ui_create_provisioning(lv_obj_t *scr)
{
    ESP_LOGI(TAG, "Creating minimal provisioning UI");

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "WiFi Setup Required\n\n"
                             "Connect to: HESTIA32\n"
                             "Open: 192.168.4.1\n\n"
                             "Device will restart\nafter configuration");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

void display_ui_create_main(lv_obj_t *scr, lv_obj_t **touch_dot, volatile uint32_t *flush_count)
{
    ESP_LOGI(TAG, "Creating full main UI");

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);

    // Create grid of colorful tiles
    int cols = 4, rows = 6;
    int tile_w = TFT_WIDTH / cols;
    int tile_h = TFT_HEIGHT / rows;
    uint32_t colors[8] = {0x2364AA, 0x3DA5D9, 0x73BFB8, 0xFEC601,
                         0xEA7317, 0x6C2E2F, 0x8A2BE2, 0x2ECC71};
    int ci = 0;

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            lv_obj_t *tile = lv_obj_create(scr);
            lv_obj_set_size(tile, tile_w - 2, tile_h - 2);
            lv_obj_set_style_bg_color(tile, lv_color_hex(colors[ci % 8]), 0);
            lv_obj_set_style_radius(tile, 6, 0);
            lv_obj_set_pos(tile, x * tile_w + 1, y * tile_h + 1);
            ci++;
        }
    }

    // Create animated bar
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_size(bar, 20, TFT_HEIGHT - 40);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(bar, 10, 20);

    // Animation wrapper to avoid cast warning
    auto anim_set_x = [](void* obj, int32_t v) {
        lv_obj_set_x((lv_obj_t*)obj, (lv_coord_t)v);
    };

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, bar);
    lv_anim_set_values(&a, 10, TFT_WIDTH - 30);
    lv_anim_set_time(&a, 6000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, anim_set_x);
    lv_anim_start(&a);

    // Create FPS counter
    lv_obj_t *fps = lv_label_create(scr);
    lv_label_set_text(fps, "FPS:");
    lv_obj_set_style_text_color(fps, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(fps, LV_ALIGN_TOP_LEFT, 8, 8);

    // FPS update timer
    static uint32_t last_t = 0;
    struct TimerData {
        lv_obj_t *label;
        volatile uint32_t *flush_count;
    };

    static TimerData timer_data;
    timer_data.label = fps;
    timer_data.flush_count = flush_count;

    lv_timer_t *tmr = lv_timer_create([](lv_timer_t *t) {
        TimerData *data = (TimerData*)t->user_data;
        uint32_t now = lv_tick_get();
        if (now - last_t >= 1000) {
            char buf[32];
            lv_snprintf(buf, sizeof(buf), "FPS:%lu", (unsigned long)*(data->flush_count));
            lv_label_set_text(data->label, buf);
            *(data->flush_count) = 0;
            last_t = now;
        }
    }, 16, &timer_data);
    (void)tmr;

    // Create touch dot indicator (create LAST so it's on top)
    *touch_dot = lv_obj_create(scr);
    lv_obj_set_style_bg_color(*touch_dot, lv_color_hex(0xFF00FF), 0);
    lv_obj_set_style_bg_opa(*touch_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(*touch_dot, 2, 0);
    lv_obj_set_style_border_color(*touch_dot, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_size(*touch_dot, 16, 16);
    lv_obj_set_style_radius(*touch_dot, 8, 0);
    lv_obj_add_flag(*touch_dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(*touch_dot, LV_OBJ_FLAG_FLOATING); // Always on top
    lv_obj_clear_flag(*touch_dot, LV_OBJ_FLAG_CLICKABLE); // Don't block touches

    ESP_LOGI(TAG, "UI creation complete");
}
