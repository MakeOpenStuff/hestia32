#pragma once

#include "esp_lcd_panel_vendor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create LCD panel for ILI9488
 *
 * @param io LCD panel IO handle
 * @param panel_dev_config LCD panel device configuration
 * @param ret_panel Returned LCD panel handle
 * @return ESP_OK on success
 */
esp_err_t esp_lcd_new_panel_ili9488(const esp_lcd_panel_io_handle_t io,
                                      const esp_lcd_panel_dev_config_t *panel_dev_config,
                                      esp_lcd_panel_handle_t *ret_panel);

#ifdef __cplusplus
}
#endif
