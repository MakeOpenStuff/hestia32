/**
 * @file thermostat.h
 * @brief Thermostat control logic with multi-stage heating/cooling
 *
 * NOTE: This module is not currently integrated into main.c
 *       It will be used in future releases for HVAC control
 *
 * Features:
 * - Multi-stage heating and cooling
 * - Comfort/Eco modes with configurable deadbands
 * - Open window detection
 * - Delay timers for compressor and stage changes
 * - Emergency shutoff and fault detection
 */

#ifndef THERMOSTAT_H
#define THERMOSTAT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// CONSTANTS
// ============================================================================

#define THERMOSTAT_VERSION "1.0.0"

// Default timing constants (seconds)
#define DEFAULT_HYSTERESIS_PERIOD_SEC       300   // 5 minutes between state changes
#define DEFAULT_HEAT_STAGE2_DELAY_SEC       600   // 10 minutes before engaging heat stage 2
#define DEFAULT_COOL_STAGE2_DELAY_SEC       300   // 5 minutes before engaging cool stage 2 (compressor less sensitive)
#define DEFAULT_MIN_CYCLE_TIME_SEC          180   // 3 minutes minimum run time

// Fan timing defaults
#define DEFAULT_FAN_PRE_RUN_SEC 30    // Fan starts 30s before thermal
#define DEFAULT_FAN_POST_RUN_SEC 60   // Fan continues 60s after thermal

// Open window detection defaults
#define DEFAULT_OPEN_WINDOW_TEMP_DROP_C     2.0f  // 2°C drop triggers detection
#define DEFAULT_OPEN_WINDOW_TIME_WINDOW_SEC 180   // Measure drop over 3 minutes
#define DEFAULT_OPEN_WINDOW_SUSPEND_SEC     900   // Suspend heating for 15 minutes

// Open window detection defaults
#define DEFAULT_OPEN_WINDOW_TEMP_DROP_C     2.0f  // 2°C drop triggers detection
#define DEFAULT_OPEN_WINDOW_TIME_WINDOW_SEC 180   // Measure drop over 3 minutes
#define DEFAULT_OPEN_WINDOW_SUSPEND_SEC     900   // Suspend heating for 15 minutes

// Default temperature thresholds (Celsius)
#define DEFAULT_COMFORT_DEADBAND_C          0.5f
#define DEFAULT_ECO_DEADBAND_C              1.5f

// ============================================================================
// ENUMERATIONS
// ============================================================================

typedef enum {
		TEMP_UNIT_CELSIUS,
		TEMP_UNIT_FAHRENHEIT
} TempUnit;

typedef enum {
		COMFORT_MODE_COMFORT,
		COMFORT_MODE_ECO
} ComfortMode;

typedef enum {
		HUMIDITY_MODE_HUMIDIFY,
		HUMIDITY_MODE_DEHUMIDIFY
} HumidityMode;

// ============================================================================
// CONFIGURATION
// ============================================================================


// Boost feature configuration (per domain)
typedef struct {
	uint32_t duration_sec;      // Last-set boost duration (seconds)
} BoostConfig;

typedef struct {
	TempUnit temp_unit;
	ComfortMode comfort_mode;
	HumidityMode humidity_mode;

	// Boost configuration for each domain
	BoostConfig boost_heating;
	BoostConfig boost_cooling;
	BoostConfig boost_hot_water;

	// Domain enable flags (max 4 can be enabled)
	bool heating_enabled;
	bool heating_stage2_enabled;     // Requires heating_enabled
	bool cooling_enabled;
	bool cooling_stage2_enabled;     // Requires cooling_enabled
	bool fan_enabled;
	bool humidity_control_enabled;
	bool hot_water_enabled;

	// Open window detection
	bool open_window_detection_enabled;
	float open_window_temp_drop_threshold;   // Temperature drop in degrees to trigger detection
	uint32_t open_window_time_window_sec;    // Time window to measure the drop (e.g., 3 minutes)
	uint32_t open_window_suspend_time_sec;   // How long to suspend heating after detection (e.g., 15 minutes)

		// Setpoints (in configured unit)
		float heat_setpoint;
		float cool_setpoint;
		float humidity_setpoint;

		// Timing parameters (seconds)
		uint32_t hysteresis_period_sec;
		uint32_t heat_stage2_delay_sec;  // Delay before engaging heating stage 2
		uint32_t cool_stage2_delay_sec;  // Delay before engaging cooling stage 2
		uint32_t min_cycle_time_sec;

		// Fan timing
		uint32_t fan_pre_run_sec;   // Fan starts this many seconds before heating/cooling
		uint32_t fan_post_run_sec;  // Fan continues this many seconds after heating/cooling stops
	// ...existing fields...
	// Boost state for each domain
	bool boost_active_heating;
	bool boost_active_cooling;
	bool boost_active_hot_water;
	uint32_t boost_end_time_heating;
	uint32_t boost_end_time_cooling;
	uint32_t boost_end_time_hot_water;
	uint32_t boost_last_duration_heating;
	uint32_t boost_last_duration_cooling;
	uint32_t boost_last_duration_hot_water;

		// Deadband thresholds (in configured unit)
		float comfort_deadband;
		float eco_deadband;
} ThermostatConfig;

// ...existing code...
// ============================================================================
// INPUT
// ============================================================================

typedef struct {
		// Sensor readings
		float temperature;          // In configured unit
		float humidity;             // Relative humidity (0-100%)
		bool sensors_valid;         // False on sensor failure

		// Time (monotonic, externally supplied)
		uint32_t now_seconds;

		// User overrides
		bool fan_override;          // Force fan on
		bool hot_water_demand;      // Hot water requested
} ThermostatInput;

// ============================================================================
// STATE (Runtime)
// ============================================================================

typedef enum {
		THERMAL_STATE_IDLE,
		THERMAL_STATE_HEATING,
		THERMAL_STATE_HEATING_STAGE2,
		THERMAL_STATE_COOLING,
		THERMAL_STATE_COOLING_STAGE2
} ThermalState;

typedef struct {
		// Thermal domain
		ThermalState thermal_state;
		uint32_t thermal_state_enter_time;
		uint32_t last_state_change_time;

		// Fan domain
		bool fan_running;
		uint32_t fan_start_time;

		// Humidity domain
		bool humidity_active;
		uint32_t humidity_start_time;

		// Hot water domain
		bool hot_water_active;
		uint32_t hot_water_start_time;

		// Fan timing tracking
		bool thermal_pending;           // Thermal wants to activate (waiting for fan pre-run)
		uint32_t thermal_pending_time;  // When thermal activation was requested
		uint32_t thermal_stop_time;     // When thermal stopped (for post-run)

	// Open window detection
	bool open_window_detected;      // Currently in open window mode
	uint32_t open_window_detect_time;   // When open window was detected
	float temperature_history[3];    // Last 3 temperature readings for trend detection
	uint32_t temperature_history_times[3];  // Timestamps for temperature readings
	uint8_t temperature_history_index;  // Current position in circular buffer

	// Boost state for each domain
	bool boost_active_heating;
	bool boost_active_cooling;
	bool boost_active_hot_water;
	uint32_t boost_end_time_heating;
	uint32_t boost_end_time_cooling;
	uint32_t boost_end_time_hot_water;
	uint32_t boost_last_duration_heating;
	uint32_t boost_last_duration_cooling;
	uint32_t boost_last_duration_hot_water;
} ThermostatState;

// ============================================================================
// OUTPUT
// ============================================================================

typedef struct {
		// Thermal outputs
		bool heating_stage1;
		bool heating_stage2;
		bool cooling_stage1;
		bool cooling_stage2;

		// Other outputs
		bool fan;
		bool humidifier;        // If humidity_mode == HUMIDIFY
		bool dehumidifier;      // If humidity_mode == DEHUMIDIFY
		bool hot_water;
} ThermostatOutput;

// API
void thermostat_update(const ThermostatConfig* config,
					  const ThermostatInput* input,
					  ThermostatState* state,
					  ThermostatOutput* output);

// Boost control API
void thermostat_boost_activate(ThermostatState* state, const char* domain, uint32_t now, uint32_t duration_sec);
void thermostat_boost_cancel(ThermostatState* state, const char* domain);
bool thermostat_boost_is_active(const ThermostatState* state, const char* domain, uint32_t now);

// ============================================================================
// API
// ============================================================================

/**
 * Initialize configuration with safe defaults
 */
void thermostat_config_init(ThermostatConfig* config);

/**
 * Validate configuration
 * Returns true if valid, false otherwise
 */
bool thermostat_config_validate(const ThermostatConfig* config);

/**
 * Initialize state (call once at startup)
 */
void thermostat_state_init(ThermostatState* state, uint32_t now_seconds);

/**
 * Main update function - call periodically
 * Must be deterministic and non-blocking
 */
void thermostat_update(
		const ThermostatConfig* config,
		const ThermostatInput* input,
		ThermostatState* state,
		ThermostatOutput* output
);

/**
 * Utility: Convert Celsius to Fahrenheit
 */
float thermostat_c_to_f(float celsius);

/**
 * Utility: Convert Fahrenheit to Celsius
 */
float thermostat_f_to_c(float fahrenheit);

#ifdef __cplusplus
}
#endif

#endif // THERMOSTAT_H