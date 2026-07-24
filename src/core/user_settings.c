#include "user_settings.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "user_settings";
#define NVS_NAMESPACE "user_settings"
#define NVS_KEY_TEMP_UNIT "temp_unit"

static user_settings_t current_settings = {
		.temp_unit = TEMP_UNIT_CELSIUS  // Default to Celsius
};

static bool settings_initialized = false;

esp_err_t user_settings_init(void) {
		if (settings_initialized) {
				return ESP_OK;
		}

		nvs_handle_t nvs_handle;
		esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);

		if (err != ESP_OK) {
				ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
				return err;
		}

		// Try to read temperature unit preference
		uint8_t temp_unit = (uint8_t)TEMP_UNIT_CELSIUS;
		err = nvs_get_u8(nvs_handle, NVS_KEY_TEMP_UNIT, &temp_unit);

		if (err == ESP_ERR_NVS_NOT_FOUND) {
				// First time - save default (Celsius)
				ESP_LOGI(TAG, "No temperature unit preference found, defaulting to Celsius");
				err = nvs_set_u8(nvs_handle, NVS_KEY_TEMP_UNIT, (uint8_t)TEMP_UNIT_CELSIUS);
				if (err == ESP_OK) {
						err = nvs_commit(nvs_handle);
				}
				current_settings.temp_unit = TEMP_UNIT_CELSIUS;
		} else if (err == ESP_OK) {
				// Validate and load
				if (temp_unit <= TEMP_UNIT_FAHRENHEIT) {
						current_settings.temp_unit = (temp_unit_t)temp_unit;
						ESP_LOGI(TAG, "Loaded temperature unit preference: %s",
										 (temp_unit == TEMP_UNIT_CELSIUS) ? "Celsius" : "Fahrenheit");
				} else {
						ESP_LOGW(TAG, "Invalid temperature unit value, resetting to Celsius");
						current_settings.temp_unit = TEMP_UNIT_CELSIUS;
						nvs_set_u8(nvs_handle, NVS_KEY_TEMP_UNIT, (uint8_t)TEMP_UNIT_CELSIUS);
						nvs_commit(nvs_handle);
				}
		} else {
				ESP_LOGE(TAG, "Failed to read temperature unit: %s", esp_err_to_name(err));
		}

		nvs_close(nvs_handle);
		settings_initialized = true;
		return ESP_OK;
}

esp_err_t user_settings_get(user_settings_t *settings) {
		if (!settings_initialized) {
				esp_err_t err = user_settings_init();
				if (err != ESP_OK) {
						return err;
				}
		}

		if (settings == NULL) {
				return ESP_ERR_INVALID_ARG;
		}

		*settings = current_settings;
		return ESP_OK;
}

esp_err_t user_settings_set_temp_unit(temp_unit_t unit) {
		if (unit > TEMP_UNIT_FAHRENHEIT) {
				return ESP_ERR_INVALID_ARG;
		}

		if (!settings_initialized) {
				esp_err_t err = user_settings_init();
				if (err != ESP_OK) {
						return err;
				}
		}

		nvs_handle_t nvs_handle;
		esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);

		if (err != ESP_OK) {
				ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
				return err;
		}

		err = nvs_set_u8(nvs_handle, NVS_KEY_TEMP_UNIT, (uint8_t)unit);
		if (err == ESP_OK) {
				err = nvs_commit(nvs_handle);
				if (err == ESP_OK) {
						current_settings.temp_unit = unit;
						ESP_LOGI(TAG, "Temperature unit set to: %s",
										 (unit == TEMP_UNIT_CELSIUS) ? "Celsius" : "Fahrenheit");
				}
		}

		nvs_close(nvs_handle);
		return err;
}

temp_unit_t user_settings_get_temp_unit(void) {
		if (!settings_initialized) {
				user_settings_init();
		}
		return current_settings.temp_unit;
}

float temp_c_to_f(float celsius) {
		return (celsius * 9.0f / 5.0f) + 32.0f;
}

float temp_f_to_c(float fahrenheit) {
		return (fahrenheit - 32.0f) * 5.0f / 9.0f;
}

float temp_to_user_unit(float celsius) {
		if (!settings_initialized) {
				user_settings_init();
		}

		if (current_settings.temp_unit == TEMP_UNIT_FAHRENHEIT) {
				return temp_c_to_f(celsius);
		}
		return celsius;
}

const char* user_settings_get_temp_unit_symbol(void) {
		if (!settings_initialized) {
				user_settings_init();
		}

		return (current_settings.temp_unit == TEMP_UNIT_CELSIUS) ? "\xC2\xB0""C" : "\xC2\xB0""F";
}

/* ─── Device config (NVS: "device_cfg") ─────────────────────────
 * NOTE: This namespace is intentionally NOT erased by factory reset
 * because the model is hardware-specific and must survive reflashing.
 * ─────────────────────────────────────────────────────────────── */

#define DEV_CFG_NS "device_cfg"

static device_config_t s_dev_cfg = {
		.model       = DEVICE_MODEL_HVAC,
		.theme       = HESTIA_THEME_DARK,
		.domain_mask = (1 << 0) | (1 << 2) | (1 << 4),   /* Heat S1, Cool S1, Fan */
		.relay_map   = {2, 0xFF, 0, 0xFF, 4, 0xFF, 0xFF}, /* W1=R2, Y1=R0, G=R4   */
};
static bool s_dev_cfg_init = false;

esp_err_t device_config_init(void)
{
		if (s_dev_cfg_init) return ESP_OK;

		nvs_handle_t h;
		esp_err_t err = nvs_open(DEV_CFG_NS, NVS_READWRITE, &h);
		if (err != ESP_OK) {
				ESP_LOGE(TAG, "device_cfg open failed: %s", esp_err_to_name(err));
				/* Keep compile-time default */
				s_dev_cfg_init = true;
				return err;
		}

		uint8_t u8;
		if (nvs_get_u8(h, "model",  &u8) == ESP_OK) s_dev_cfg.model  = (device_model_t)u8;
		if (nvs_get_u8(h, "theme",  &u8) == ESP_OK) s_dev_cfg.theme  = (hestia_theme_id_t)u8;
		if (nvs_get_u8(h, "dmask",  &u8) == ESP_OK) s_dev_cfg.domain_mask = u8;
	if (nvs_get_u8(h, "show_ptr", &u8) == ESP_OK) s_dev_cfg.show_pointer = u8;
	if (nvs_get_u8(h, "show_time", &u8) == ESP_OK) s_dev_cfg.show_time = u8;
		nvs_close(h);
		s_dev_cfg_init = true;
		ESP_LOGI(TAG, "device_cfg loaded: model=%d theme=%d dmask=0x%02X",
		         s_dev_cfg.model, s_dev_cfg.theme, s_dev_cfg.domain_mask);
		return ESP_OK;
}

esp_err_t device_config_get(device_config_t *cfg)
{
		if (!s_dev_cfg_init) device_config_init();
		if (!cfg) return ESP_ERR_INVALID_ARG;
		*cfg = s_dev_cfg;
		return ESP_OK;
}

esp_err_t device_config_save(const device_config_t *cfg)
{
		if (!cfg) return ESP_ERR_INVALID_ARG;
		s_dev_cfg = *cfg;

		nvs_handle_t h;
		esp_err_t err = nvs_open(DEV_CFG_NS, NVS_READWRITE, &h);
		if (err != ESP_OK) return err;

		nvs_set_u8(h, "model",     (uint8_t)cfg->model);
		nvs_set_u8(h, "theme",     (uint8_t)cfg->theme);
		nvs_set_u8(h, "dmask",     cfg->domain_mask);	nvs_set_u8(h, "show_ptr",  cfg->show_pointer);
	nvs_set_u8(h, "show_time", cfg->show_time);		nvs_set_blob(h, "relay_map", cfg->relay_map, RELAY_DOMAIN_COUNT);
		err = nvs_commit(h);
		nvs_close(h);
		return err;
}

device_model_t    device_config_get_model(void)  { if (!s_dev_cfg_init) device_config_init(); return s_dev_cfg.model; }
hestia_theme_id_t device_config_get_theme(void)  { if (!s_dev_cfg_init) device_config_init(); return s_dev_cfg.theme; }
uint8_t device_config_get_domain_mask(void)       { if (!s_dev_cfg_init) device_config_init(); return s_dev_cfg.domain_mask; }

/* ─── Display settings (NVS: "display_cfg") ───────────────────── */

#define DISP_CFG_NS "display_cfg"

static display_settings_t s_disp = {
		.brightness       = 80,
		.sleep_bri        = 0,
		.sleep_timeout_sec = 60,
};
static bool s_disp_init = false;

esp_err_t display_settings_init(void)
{
		if (s_disp_init) return ESP_OK;
		nvs_handle_t h;
		if (nvs_open(DISP_CFG_NS, NVS_READWRITE, &h) != ESP_OK) { s_disp_init = true; return ESP_OK; }
		uint8_t u8; uint16_t u16;
		if (nvs_get_u8(h,  "brightness", &u8)  == ESP_OK) s_disp.brightness        = u8;
		if (nvs_get_u8(h,  "sleep_bri",  &u8)  == ESP_OK) s_disp.sleep_bri         = u8;
		if (nvs_get_u16(h, "sleep_sec",  &u16) == ESP_OK) s_disp.sleep_timeout_sec = u16;
		nvs_close(h);
		s_disp_init = true;
		return ESP_OK;
}

esp_err_t display_settings_get(display_settings_t *s)
{
		if (!s_disp_init) display_settings_init();
		if (!s) return ESP_ERR_INVALID_ARG;
		*s = s_disp; return ESP_OK;
}

esp_err_t display_settings_save(const display_settings_t *s)
{
		if (!s) return ESP_ERR_INVALID_ARG;
		s_disp = *s;
		nvs_handle_t h;
		if (nvs_open(DISP_CFG_NS, NVS_READWRITE, &h) != ESP_OK) return ESP_ERR_INVALID_STATE;
		nvs_set_u8(h,  "brightness", s->brightness);
		nvs_set_u8(h,  "sleep_bri",  s->sleep_bri);
		nvs_set_u16(h, "sleep_sec",  s->sleep_timeout_sec);
		esp_err_t err = nvs_commit(h);
		nvs_close(h);
		return err;
}

/* ─── Time settings (NVS: "time_cfg") ─────────────────────────── */

#define TIME_CFG_NS "time_cfg"

static time_settings_t s_time_cfg = {
		.use_ntp        = 1,
		.ntp_url        = "pool.ntp.org",
		.tz_posix       = "UTC0",
		.time_format_24h = 1,
};
static bool s_time_init = false;

esp_err_t time_settings_init(void)
{
		if (s_time_init) return ESP_OK;
		nvs_handle_t h;
		if (nvs_open(TIME_CFG_NS, NVS_READWRITE, &h) != ESP_OK) { s_time_init = true; return ESP_OK; }
		uint8_t u8;
		if (nvs_get_u8(h, "use_ntp",  &u8) == ESP_OK) s_time_cfg.use_ntp         = u8;
		if (nvs_get_u8(h, "fmt24h",   &u8) == ESP_OK) s_time_cfg.time_format_24h = u8;
		size_t len = sizeof(s_time_cfg.ntp_url);
		nvs_get_str(h, "ntp_url", s_time_cfg.ntp_url,   &len);
		len = sizeof(s_time_cfg.tz_posix);
		nvs_get_str(h, "tz_posix", s_time_cfg.tz_posix, &len);
		nvs_close(h);
		s_time_init = true;
		return ESP_OK;
}

esp_err_t time_settings_get(time_settings_t *s)
{
		if (!s_time_init) time_settings_init();
		if (!s) return ESP_ERR_INVALID_ARG;
		*s = s_time_cfg; return ESP_OK;
}

esp_err_t time_settings_save(const time_settings_t *s)
{
		if (!s) return ESP_ERR_INVALID_ARG;
		s_time_cfg = *s;
		nvs_handle_t h;
		if (nvs_open(TIME_CFG_NS, NVS_READWRITE, &h) != ESP_OK) return ESP_ERR_INVALID_STATE;
		nvs_set_u8(h,  "use_ntp",  s->use_ntp);
		nvs_set_u8(h,  "fmt24h",   s->time_format_24h);
		nvs_set_str(h, "ntp_url",  s->ntp_url);
		nvs_set_str(h, "tz_posix", s->tz_posix);
		esp_err_t err = nvs_commit(h);
		nvs_close(h);
		return err;
}

/* ─── Thermostat settings (NVS: "therm_cfg") ──────────────────── */

#define THERM_CFG_NS "therm_cfg"

static thermostat_settings_t s_therm_cfg = {
		.heat_setpoint_c        = 22.0f,  // 72°F for HVAC, 22°C for EU
		.cool_setpoint_c        = 24.0f,
		.eco_heat_offset        = 2.0f,
		.eco_cool_offset        = 2.0f,
		.humidity_setpoint      = 50.0f,
		.s2_heat_delay_min      = 10,
		.s2_cool_delay_min      = 5,
		.s2_temp_delta          = 0.75f,
		.therm_mode             = 0,
		.open_window_en         = 1,
		.open_window_threshold_c = 2.0f,
		.open_window_window_min = 3,
};
static bool s_therm_init = false;

static void therm_nvs_load(nvs_handle_t h)
{
		/* NVS doesn't support float directly; store as int16_t scaled ×10 */
		int16_t iv;
		uint8_t  u8;
		uint16_t u16;
		if (nvs_get_i16(h, "heat_sp",    &iv) == ESP_OK) s_therm_cfg.heat_setpoint_c    = iv / 10.0f;
		if (nvs_get_i16(h, "cool_sp",    &iv) == ESP_OK) s_therm_cfg.cool_setpoint_c    = iv / 10.0f;
		if (nvs_get_i16(h, "eco_heat",   &iv) == ESP_OK) s_therm_cfg.eco_heat_offset    = iv / 10.0f;
		if (nvs_get_i16(h, "eco_cool",   &iv) == ESP_OK) s_therm_cfg.eco_cool_offset    = iv / 10.0f;
		if (nvs_get_i16(h, "humi_sp",    &iv) == ESP_OK) s_therm_cfg.humidity_setpoint  = iv / 10.0f;
		if (nvs_get_u16(h, "s2h_min",   &u16) == ESP_OK) s_therm_cfg.s2_heat_delay_min  = u16;
		if (nvs_get_u16(h, "s2c_min",   &u16) == ESP_OK) s_therm_cfg.s2_cool_delay_min  = u16;
		if (nvs_get_i16(h, "s2_delta",   &iv) == ESP_OK) s_therm_cfg.s2_temp_delta      = iv / 100.0f;
		if (nvs_get_u8(h,  "mode",       &u8)  == ESP_OK) s_therm_cfg.therm_mode         = u8;
		if (nvs_get_u8(h,  "win_en",     &u8)  == ESP_OK) s_therm_cfg.open_window_en     = u8;
		if (nvs_get_i16(h, "win_thr",    &iv) == ESP_OK) s_therm_cfg.open_window_threshold_c = iv / 10.0f;
		if (nvs_get_u16(h, "win_min",   &u16) == ESP_OK) s_therm_cfg.open_window_window_min  = u16;
}

static void therm_nvs_save(nvs_handle_t h, const thermostat_settings_t *s)
{
		nvs_set_i16(h, "heat_sp",  (int16_t)(s->heat_setpoint_c    * 10));
		nvs_set_i16(h, "cool_sp",  (int16_t)(s->cool_setpoint_c    * 10));
		nvs_set_i16(h, "eco_heat", (int16_t)(s->eco_heat_offset     * 10));
		nvs_set_i16(h, "eco_cool", (int16_t)(s->eco_cool_offset     * 10));
		nvs_set_i16(h, "humi_sp",  (int16_t)(s->humidity_setpoint   * 10));
		nvs_set_u16(h, "s2h_min",  s->s2_heat_delay_min);
		nvs_set_u16(h, "s2c_min",  s->s2_cool_delay_min);
		nvs_set_i16(h, "s2_delta", (int16_t)(s->s2_temp_delta       * 100));
		nvs_set_u8(h,  "mode",     s->therm_mode);
		nvs_set_u8(h,  "win_en",   s->open_window_en);
		nvs_set_i16(h, "win_thr",  (int16_t)(s->open_window_threshold_c  * 10));
		nvs_set_u16(h, "win_min",  s->open_window_window_min);
}

esp_err_t thermostat_settings_init(void)
{
		if (s_therm_init) return ESP_OK;
		nvs_handle_t h;
		if (nvs_open(THERM_CFG_NS, NVS_READWRITE, &h) == ESP_OK) {
				therm_nvs_load(h);
				nvs_close(h);
		}
		s_therm_init = true;
		return ESP_OK;
}

esp_err_t thermostat_settings_get(thermostat_settings_t *s)
{
		if (!s_therm_init) thermostat_settings_init();
		if (!s) return ESP_ERR_INVALID_ARG;
		*s = s_therm_cfg; return ESP_OK;
}

esp_err_t thermostat_settings_save(const thermostat_settings_t *s)
{
		if (!s) return ESP_ERR_INVALID_ARG;
		s_therm_cfg = *s;
		nvs_handle_t h;
		if (nvs_open(THERM_CFG_NS, NVS_READWRITE, &h) != ESP_OK) return ESP_ERR_INVALID_STATE;
		therm_nvs_save(h, s);
		esp_err_t err = nvs_commit(h);
		nvs_close(h);
		return err;
}

