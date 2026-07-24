#include "ui/ui_common.h"
#include "ui/ui_theme.h"

static void on_back(lv_event_t *e)
{
    (void)e;
    ui_common_pop_screen();
}

extern "C" void ui_color_test_open(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0); // Black background

    // Header
    ui_common_header(scr, "Color Test", on_back, NULL);

    // Container for color tiles
    lv_obj_t *container = lv_obj_create(scr);
    lv_obj_set_size(container, LV_PCT(100), 270);
    lv_obj_align(container, LV_ALIGN_TOP_LEFT, 0, 50);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 8, 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(container, 8, 0);
    lv_obj_set_style_pad_column(container, 8, 0);

    // Test colors with their hex values
    struct {
        uint32_t color;
        const char *name;
        int number;
    } test_colors[] = {
        {0xFF0000, "R", 1},   // Pure RED
        {0x00FF00, "G", 2},   // Pure GREEN
        {0x0000FF, "B", 3},   // Pure BLUE
        {0xFFFF00, "Y", 4},   // YELLOW (R+G)
        {0xFF00FF, "M", 5},   // MAGENTA (R+B)
        {0x00FFFF, "C", 6},   // CYAN (G+B)
        {0xFFFFFF, "W", 7},   // WHITE
        {0x000000, "K", 8},   // BLACK
        {0xFF8000, "O", 9},   // ORANGE
        {0x808080, "Gr", 10}, // GRAY
        {0x800000, "DR", 11}, // DARK RED
        {0x008000, "DG", 12}, // DARK GREEN
    };

    for (size_t i = 0; i < sizeof(test_colors) / sizeof(test_colors[0]); i++) {
        lv_obj_t *tile = lv_obj_create(container);
        lv_obj_set_size(tile, 70, 60);
        lv_obj_set_style_bg_color(tile, lv_color_hex(test_colors[i].color), 0);
        lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(tile, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_width(tile, 1, 0);
        lv_obj_set_style_radius(tile, 4, 0);

        lv_obj_t *num_label = lv_label_create(tile);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", test_colors[i].number);
        lv_label_set_text(num_label, buf);
        lv_obj_set_style_text_color(num_label,
            lv_color_hex(test_colors[i].color == 0x000000 ? 0xFFFFFF : 0x000000), 0);
        lv_obj_set_style_text_font(num_label, &lv_font_montserrat_20, 0);
        lv_obj_align(num_label, LV_ALIGN_TOP_LEFT, 4, 2);

        lv_obj_t *name_label = lv_label_create(tile);
        lv_label_set_text(name_label, test_colors[i].name);
        lv_obj_set_style_text_color(name_label,
            lv_color_hex(test_colors[i].color == 0x000000 ? 0xFFFFFF : 0x000000), 0);
        lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);
        lv_obj_align(name_label, LV_ALIGN_BOTTOM_RIGHT, -4, -2);
    }

    // Instructions
    lv_obj_t *info = lv_label_create(scr);
    lv_label_set_text(info, "Report each number's color:\n1-12");
    lv_obj_set_style_text_color(info, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(info, LV_ALIGN_BOTTOM_MID, 0, -10);

    ui_common_push_screen(scr);
}
