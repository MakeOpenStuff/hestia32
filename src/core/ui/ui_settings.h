#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Open the settings hub (push on top of main screen). */
void ui_settings_open(void);

/* Called by sub-screens to return to the settings hub. */
void ui_settings_back(void);

#ifdef __cplusplus
}
#endif
