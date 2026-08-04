#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check if onboarding is needed (first boot or after factory reset)
 *
 * @return true if onboarding screen should be shown
 */
bool ui_onboarding_needed(void);

/**
 * @brief Mark onboarding as complete
 */
void ui_onboarding_mark_complete(void);

/**
 * @brief Show the onboarding screen (model + temp unit selection)
 * Blocks until user presses Continue button.
 * Must be called from LVGL task context.
 */
void ui_onboarding_show(void);

/**
 * @brief Show the time/timezone onboarding screen
 * Blocks until user presses Continue button.
 * Must be called from LVGL task context.
 */
void ui_onboarding_time_show(void);

#ifdef __cplusplus
}
#endif
