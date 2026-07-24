#include "ui/ui_settings_datetime.h"
#include "ui/ui_common.h"
#include "ui/ui_theme.h"
#include "core/user_settings.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "ui_datetime";

/* Common IANA timezone → POSIX TZ string pairs */
static const struct { const char *label; const char *posix; } TZ_LIST[] = {
    { "UTC",               "UTC0"                              },
    { "London (GMT/BST)",  "GMT0BST,M3.5.0/1,M10.5.0"        },
    { "Paris/Berlin (CET)","CET-1CEST,M3.5.0,M10.5.0/3"      },
    { "Athens (EET)",      "EET-2EEST,M3.5.0/3,M10.5.0/4"    },
    { "Moscow (MSK)",      "MSK-3"                             },
    { "Dubai (GST)",       "GST-4"                             },
    { "India (IST)",       "IST-5:30"                          },
    { "Bangkok (ICT)",     "ICT-7"                             },
    { "Singapore (SGT)",   "SGT-8"                             },
    { "Tokyo (JST)",       "JST-9"                             },
    { "Sydney (AEST)",     "AEST-10AEDT,M10.1.0,M4.1.0/3"    },
    { "Auckland (NZST)",   "NZST-12NZDT,M9.5.0,M4.1.0/3"     },
    { "US Eastern (ET)",   "EST5EDT,M3.2.0,M11.1.0"           },
    { "US Central (CT)",   "CST6CDT,M3.2.0,M11.1.0"          },
    { "US Mountain (MT)",  "MST7MDT,M3.2.0,M11.1.0"          },
    { "US Pacific (PT)",   "PST8PDT,M3.2.0,M11.1.0"           },
    { "US Alaska (AKT)",   "AKST9AKDT,M3.2.0,M11.1.0"        },
    { "US Hawaii (HST)",   "HST10"                             },
    { "Sao Paulo (BRT)",   "BRT3BRST,M10.3.0/0,M2.3.0/0"     },
    { "Custom (POSIX)",    NULL                                 },  /* last */
};
#define TZ_COUNT  ((int)(sizeof(TZ_LIST)/sizeof(TZ_LIST[0])))

static time_settings_t s_ts;
static lv_obj_t *s_tz_roller  = NULL;
static lv_obj_t *s_ntp_sw     = NULL;

static void apply_tz(int tz_idx)
{
    if (TZ_LIST[tz_idx].posix) {
        setenv("TZ", TZ_LIST[tz_idx].posix, 1);
        tzset();
        strncpy(s_ts.tz_posix, TZ_LIST[tz_idx].posix, sizeof(s_ts.tz_posix)-1);
    }
}

static void on_save(lv_event_t *e)
{
    (void)e;
    if (s_tz_roller) {
        int idx = lv_roller_get_selected(s_tz_roller);
        if (idx < TZ_COUNT - 1) {  /* not "Custom" */
            apply_tz(idx);
        }
    }
    if (s_ntp_sw) {
        s_ts.use_ntp = lv_obj_has_state(s_ntp_sw, LV_STATE_CHECKED) ? 1 : 0;
    }
    time_settings_save(&s_ts);
    ESP_LOGI(TAG, "Time settings saved: NTP=%d TZ=%s", s_ts.use_ntp, s_ts.tz_posix);
    ui_common_pop_screen();
}

static void on_back(lv_event_t *e) { (void)e; ui_common_pop_screen(); }

void ui_settings_datetime_open(void)
{
    time_settings_init();
    time_settings_get(&s_ts);

    const hestia_theme_t *t = ui_theme_get();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(scr, lv_color_hex(t->bg), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    ui_common_header(scr, "Date & Time", on_back, NULL);

    lv_obj_t *body = lv_obj_create(scr);
    lv_obj_set_size(body, UI_SCREEN_W, UI_SCREEN_H - UI_HEADER_H - 60);
    lv_obj_align(body, LV_ALIGN_TOP_LEFT, 0, UI_HEADER_H);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_hor(body, UI_PAD, 0);
    lv_obj_set_style_pad_ver(body, UI_PAD_SM, 0);
    lv_obj_set_style_radius(body, 0, 0);
    lv_obj_set_layout(body, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);

    /* NTP toggle */
    s_ntp_sw = ui_common_toggle_row(body, "Use NTP (auto time sync)", NULL, NULL);
    if (s_ts.use_ntp) lv_obj_add_state(s_ntp_sw, LV_STATE_CHECKED);

    /* Time format */
    lv_obj_t *fmt_row = lv_obj_create(body);
    lv_obj_set_size(fmt_row, LV_PCT(100), 44);
    lv_obj_set_style_bg_opa(fmt_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(fmt_row, 0, 0);
    lv_obj_set_style_pad_all(fmt_row, 0, 0);
    lv_obj_set_style_radius(fmt_row, 0, 0);
    lv_obj_clear_flag(fmt_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *fmt_lbl = lv_label_create(fmt_row);
    lv_label_set_text(fmt_lbl, "Clock format");
    lv_obj_set_style_text_color(fmt_lbl, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(fmt_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(fmt_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    const char *fmt_opts[] = { "24h", "12h" };
    for (int i = 0; i < 2; i++) {
        lv_obj_t *fb = lv_btn_create(fmt_row);
        lv_obj_set_size(fb, 50, 30);
        bool active = (i == (s_ts.time_format_24h ? 0 : 1));
        lv_obj_set_style_bg_color(fb, lv_color_hex(active ? t->primary : t->surface_variant), 0);
        lv_obj_set_style_bg_opa(fb, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(fb, 0, 0);
        lv_obj_set_style_radius(fb, 6, 0);
        lv_obj_align(fb, LV_ALIGN_RIGHT_MID, -i*56, 0);
        lv_obj_t *fl = lv_label_create(fb);
        lv_label_set_text(fl, fmt_opts[i]);
        lv_obj_set_style_text_color(fl, lv_color_hex(active ? t->on_primary : t->text), 0);
        lv_obj_set_style_text_font(fl, &lv_font_montserrat_14, 0);
        lv_obj_align(fl, LV_ALIGN_CENTER, 0, 0);
    }

    /* Timezone roller */
    lv_obj_t *tz_sec = ui_common_section_label(body, "Timezone");
    (void)tz_sec;

    /* Build roller string */
    static char tz_opts[TZ_COUNT * 32];
    tz_opts[0] = '\0';
    for (int i = 0; i < TZ_COUNT; i++) {
        strncat(tz_opts, TZ_LIST[i].label, sizeof(tz_opts)-strlen(tz_opts)-2);
        if (i < TZ_COUNT - 1) strncat(tz_opts, "\n", sizeof(tz_opts)-strlen(tz_opts)-1);
    }

    s_tz_roller = lv_roller_create(body);
    lv_roller_set_options(s_tz_roller, tz_opts, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(s_tz_roller, 3);
    lv_obj_set_width(s_tz_roller, LV_PCT(100));
    lv_obj_set_style_bg_color(s_tz_roller, lv_color_hex(t->surface), 0);
    lv_obj_set_style_bg_color(s_tz_roller, lv_color_hex(t->primary), LV_PART_SELECTED);
    lv_obj_set_style_text_color(s_tz_roller, lv_color_hex(t->text), 0);
    lv_obj_set_style_text_font(s_tz_roller, &lv_font_montserrat_14, 0);

    /* Pre-select current TZ */
    for (int i = 0; i < TZ_COUNT - 1; i++) {
        if (TZ_LIST[i].posix && strcmp(TZ_LIST[i].posix, s_ts.tz_posix) == 0) {
            lv_roller_set_selected(s_tz_roller, (uint16_t)i, LV_ANIM_OFF);
            break;
        }
    }

    lv_obj_t *save_area = lv_obj_create(scr);
    lv_obj_set_size(save_area, UI_SCREEN_W, 52);
    lv_obj_align(save_area, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(save_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(save_area, 0, 0);
    lv_obj_set_style_pad_all(save_area, UI_PAD, 0);
    lv_obj_clear_flag(save_area, LV_OBJ_FLAG_SCROLLABLE);
    ui_common_btn(save_area, "SAVE", on_save, NULL);

    ui_common_push_screen(scr);
}
