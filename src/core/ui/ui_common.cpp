#include "ui/ui_common.h"
#include "ui/ui_theme.h"
#include <string.h>

/* ---------- Screen navigation stack ---------- */
#define NAV_STACK_MAX 8
static lv_obj_t *s_screen_stack[NAV_STACK_MAX];
static int        s_stack_top = 0;
static lv_obj_t  *s_main_scr = NULL;

void ui_common_set_main_screen(lv_obj_t *scr)
{
    s_main_scr = scr;
    s_screen_stack[0] = scr;
    s_stack_top = 0;
}

lv_obj_t *ui_common_get_main_screen(void)
{
    return s_main_scr;
}

void ui_common_push_screen(lv_obj_t *new_scr)
{
    if (s_stack_top < NAV_STACK_MAX - 1) {
        s_stack_top++;
        s_screen_stack[s_stack_top] = new_scr;
    }
    lv_scr_load(new_scr);
    lv_obj_invalidate(new_scr);
    /* Force an immediate render cycle so the full new screen is flushed to the
     * display GRAM before returning.  Without this, the partial-refresh buffer
     * may leave stale GRAM content visible in areas not yet redrawn. */
    lv_refr_now(lv_disp_get_default());
    /* Second pass: handles any display that internally double-buffers or has
     * residual dirty regions from the first render. */
    lv_refr_now(lv_disp_get_default());
}

void ui_common_pop_screen(void)
{
    if (s_stack_top > 0) {
        lv_obj_t *dying = s_screen_stack[s_stack_top];
        s_stack_top--;
        lv_obj_t *prev  = s_screen_stack[s_stack_top];
        lv_scr_load(prev);
        lv_obj_invalidate(prev);
        lv_refr_now(lv_disp_get_default());
        lv_refr_now(lv_disp_get_default());
        /* Delete the popped screen after the full repaint so it cannot
         * be composited over the restored screen during the render cycle. */
        if (dying && dying != s_main_scr) {
            lv_obj_del_async(dying);
            s_screen_stack[s_stack_top + 1] = NULL;
        }
    }
}

/* ---------- Header bar ---------- */

lv_obj_t *ui_common_header(lv_obj_t *parent, const char *title,
                            lv_event_cb_t back_cb, void *back_user_data)
{
    const hestia_theme_t *t = ui_theme_get();

    lv_obj_t *hdr = lv_obj_create(parent);
    lv_obj_set_size(hdr, LV_PCT(100), UI_HEADER_H);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(t->surface), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(hdr, lv_color_hex(t->border), LV_PART_MAIN);
    lv_obj_set_style_border_width(hdr, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Back button */
    lv_obj_t *btn = lv_btn_create(hdr);
    lv_obj_set_size(btn, UI_HEADER_H, UI_HEADER_H);
    lv_obj_set_style_bg_color(btn, lv_color_hex(t->surface), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_align(btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(btn, back_cb, LV_EVENT_CLICKED, back_user_data);

    lv_obj_t *back_lbl = lv_label_create(btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_lbl, lv_color_hex(t->primary), 0);
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_20, 0);
    lv_obj_align(back_lbl, LV_ALIGN_CENTER, 0, 0);

    /* Title label */
    lv_obj_t *lbl = lv_label_create(hdr);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    return hdr;
}

/* ---------- Buttons ---------- */

lv_obj_t *ui_common_btn(lv_obj_t *parent, const char *label,
                         lv_event_cb_t cb, void *user_data)
{
    const hestia_theme_t *t = ui_theme_get();
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_width(btn, LV_PCT(100));
    lv_obj_set_height(btn, 44);
    lv_obj_set_style_bg_color(btn, lv_color_hex(t->primary), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 6, 0);
    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, lv_color_hex(t->on_primary), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    return btn;
}

lv_obj_t *ui_common_btn_danger(lv_obj_t *parent, const char *label,
                                lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = ui_common_btn(parent, label, cb, user_data);
    lv_obj_set_style_bg_color(btn, lv_color_hex(ui_theme_get()->danger), 0);
    lv_obj_t *child = lv_obj_get_child(btn, 0);
    if (child) lv_obj_set_style_text_color(child, lv_color_hex(0xFFFFFF), 0);
    return btn;
}

/* ---------- Section label ---------- */

lv_obj_t *ui_common_section_label(lv_obj_t *parent, const char *text)
{
    const hestia_theme_t *t = ui_theme_get();
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(t->text_secondary), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_width(lbl, LV_PCT(100));
    return lbl;
}

/* ---------- Divider ---------- */

lv_obj_t *ui_common_divider(lv_obj_t *parent)
{
    lv_obj_t *div = lv_obj_create(parent);
    lv_obj_set_size(div, LV_PCT(100), 1);
    lv_obj_set_style_bg_color(div, lv_color_hex(ui_theme_get()->border), 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_set_style_radius(div, 0, 0);
    lv_obj_set_style_pad_all(div, 0, 0);
    return div;
}

/* ---------- Row ---------- */

lv_obj_t *ui_common_row(lv_obj_t *parent, const char *label_text)
{
    const hestia_theme_t *t = ui_theme_get();
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), 44);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, UI_PAD, 0);

    return row;
}

/* ---------- Badge ---------- */

lv_obj_t *ui_common_badge(lv_obj_t *parent, const char *text, uint32_t color)
{
    lv_obj_t *badge = lv_obj_create(parent);
    lv_obj_set_style_bg_color(badge, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_radius(badge, 10, 0);
    lv_obj_set_style_pad_hor(badge, 6, 0);
    lv_obj_set_style_pad_ver(badge, 2, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(badge);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    return badge;
}

void ui_common_badge_update(lv_obj_t *badge, const char *text, uint32_t color)
{
    if (!badge) return;
    lv_obj_set_style_bg_color(badge, lv_color_hex(color), 0);
    lv_obj_t *lbl = lv_obj_get_child(badge, 0);
    if (lbl) lv_label_set_text(lbl, text);
}

/* ---------- Toggle row ---------- */

lv_obj_t *ui_common_toggle_row(lv_obj_t *parent, const char *label_text,
                                lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *row = ui_common_row(parent, label_text);
    lv_obj_t *sw  = lv_switch_create(row);
    lv_obj_set_size(sw, 44, 24);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -UI_PAD, 0);
    lv_obj_set_style_bg_color(sw, lv_color_hex(ui_theme_get()->inactive_color), 0);
    lv_obj_set_style_bg_color(sw, lv_color_hex(ui_theme_get()->primary), (lv_style_selector_t)(LV_PART_INDICATOR | LV_STATE_CHECKED));
    if (cb) lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, user_data);
    return sw;
}
