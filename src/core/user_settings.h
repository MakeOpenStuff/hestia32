#ifndef USER_SETTINGS_H
#define USER_SETTINGS_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Temperature unit preference
 * Guard matches thermostat.h so the same enumerator names can coexist.
 */
#ifndef TEMP_UNIT_DEFINED
#define TEMP_UNIT_DEFINED
typedef enum {
		TEMP_UNIT_CELSIUS = 0,
		TEMP_UNIT_FAHRENHEIT = 1
} TempUnit;
typedef TempUnit temp_unit_t;
#endif

/**
 * @brief User settings structure
 */
typedef struct {
		temp_unit_t temp_unit;
} user_settings_t;

/**
 * @brief Initialize user settings from NVS
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t user_settings_init(void);

/**
 * @brief Get current user settings
 *
 * @param settings Pointer to store settings
 * @return esp_err_t ESP_OK on success
 */
esp_err_t user_settings_get(user_settings_t *settings);

/**
 * @brief Set temperature unit preference
 *
 * @param unit Temperature unit (TEMP_UNIT_CELSIUS or TEMP_UNIT_FAHRENHEIT)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t user_settings_set_temp_unit(temp_unit_t unit);

/**
 * @brief Get temperature unit preference
 *
 * @return temp_unit_t Current temperature unit
 */
temp_unit_t user_settings_get_temp_unit(void);

/**
 * @brief Convert Celsius to Fahrenheit
 *
 * @param celsius Temperature in Celsius
 * @return float Temperature in Fahrenheit
 */
float temp_c_to_f(float celsius);

/**
 * @brief Convert Fahrenheit to Celsius
 *
 * @param fahrenheit Temperature in Fahrenheit
 * @return float Temperature in Celsius
 */
float temp_f_to_c(float fahrenheit);

/**
 * @brief Convert temperature to user's preferred unit
 *
 * @param celsius Temperature in Celsius
 * @return float Temperature in user's preferred unit
 */
float temp_to_user_unit(float celsius);

/**
 * @brief Get temperature unit symbol string
 *
 * @return const char* "°C" or "°F"
 */
const char* user_settings_get_temp_unit_symbol(void);

/* ─── Device model ─────────────────────────────────────────────── */

typedef enum {
    DEVICE_MODEL_HVAC = 0,  /* North American HVAC (R + Y1/Y2/W1/W2/G) */
    DEVICE_MODEL_EU   = 1,  /* European mains (N + L1-L5)               */
} device_model_t;

typedef enum {
    HESTIA_THEME_DARK   = 0,
    HESTIA_THEME_LIGHT  = 1,
    HESTIA_THEME_CUSTOM = 2,
} hestia_theme_id_t;

#define RELAY_DOMAIN_COUNT 8   /* Heating S1/S2, Cooling S1/S2, Fan, Humidity, HotWater, Reversing */
#define RELAY_UNASSIGNED   0xFF

typedef struct {
    device_model_t   model;
    hestia_theme_id_t theme;
    uint8_t          domain_mask;              /* bit per domain (bit 0=Heating S1, …, bit 7=Reversing) */
    uint8_t          relay_map[RELAY_DOMAIN_COUNT]; /* relay 0-4, 0xFF = unassigned   */
    uint8_t          show_pointer;             /* 1=show touch pointer, 0=hidden (default 0) */
    uint8_t          show_time;                /* 1=show time in main UI, 0=hidden (default 1) */
} device_config_t;

esp_err_t device_config_init(void);
esp_err_t device_config_get(device_config_t *cfg);
esp_err_t device_config_save(const device_config_t *cfg);
device_model_t    device_config_get_model(void);
hestia_theme_id_t device_config_get_theme(void);
uint8_t           device_config_get_domain_mask(void);

/* ─── Display settings ─────────────────────────────────────────── */

typedef struct {
    uint8_t  brightness;          /* active screen brightness 10-100% */
    uint8_t  sleep_bri;           /* sleep brightness 0-100% (0=off)  */
    uint16_t sleep_timeout_sec;   /* 0 = never sleep                  */
} display_settings_t;

esp_err_t display_settings_init(void);
esp_err_t display_settings_get(display_settings_t *s);
esp_err_t display_settings_save(const display_settings_t *s);

/* ─── Time / NTP settings ──────────────────────────────────────── */

typedef struct {
    uint8_t use_ntp;            /* 0=manual, 1=NTP                  */
    char    ntp_url[64];        /* NTP server hostname               */
    char    tz_posix[64];       /* POSIX TZ string                  */
    uint8_t time_format_24h;    /* 1=24h, 0=12h                     */
} time_settings_t;

esp_err_t time_settings_init(void);
esp_err_t time_settings_get(time_settings_t *s);
esp_err_t time_settings_save(const time_settings_t *s);

/* ─── Thermostat settings ──────────────────────────────────────── */

typedef struct {
    float    heat_setpoint_c;       /* comfort heat setpoint °C          */
    float    cool_setpoint_c;       /* comfort cool setpoint °C          */
    float    eco_heat_offset;       /* eco heat = comfort - offset       */
    float    eco_cool_offset;       /* eco cool = comfort + offset       */
    float    humidity_setpoint;     /* target humidity %                 */
    uint16_t s2_heat_delay_min;     /* stage-2 heat activation delay min */
    uint16_t s2_cool_delay_min;     /* stage-2 cool activation delay min */
    float    s2_temp_delta;         /* stage-2 temp deviation threshold  */
    uint8_t  therm_mode;            /* 0=comfort, 1=eco, 2=off          */
    uint8_t  open_window_en;        /* open window detection enabled     */
    float    open_window_threshold_c; /* °C drop to trigger              */
    uint16_t open_window_window_min;  /* detection window in minutes     */
} thermostat_settings_t;

esp_err_t thermostat_settings_init(void);
esp_err_t thermostat_settings_get(thermostat_settings_t *s);
esp_err_t thermostat_settings_save(const thermostat_settings_t *s);

#ifdef __cplusplus
}
#endif

#endif // USER_SETTINGS_H
