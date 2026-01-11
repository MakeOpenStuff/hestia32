#include "thermostat.h"
#include <string.h>

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

static inline float get_deadband(const ThermostatConfig* config) {
		return (config->comfort_mode == COMFORT_MODE_COMFORT)
					 ? config->comfort_deadband
					 : config->eco_deadband;
}

static inline bool time_elapsed(uint32_t start_time, uint32_t now, uint32_t duration) {
		return (now - start_time) >= duration;
}

// ============================================================================
// THERMAL DOMAIN LOGIC
// ============================================================================

static void update_thermal_domain(
		const ThermostatConfig* config,
		const ThermostatInput* input,
		ThermostatState* state,
		ThermostatOutput* output
) {
		if (!input->sensors_valid) {
				state->thermal_state = THERMAL_STATE_IDLE;
				state->thermal_pending = false;
				output->heating_stage1 = false;
				output->heating_stage2 = false;
				output->cooling_stage1 = false;
				output->cooling_stage2 = false;
				return;
		}

		const float temp = input->temperature;
		const uint32_t now = input->now_seconds;
		const float deadband = get_deadband(config);
		const float heat_setpoint = config->heat_setpoint;
		const float cool_setpoint = config->cool_setpoint;
		const float stage2_threshold = deadband * 1.5f;

		const ThermalState prev_state = state->thermal_state;

		// State machine
		switch (state->thermal_state) {
				case THERMAL_STATE_IDLE:
						// Check if thermal activation is needed
						bool heating_needed = config->heating_enabled &&
																 temp < (heat_setpoint - deadband) &&
																 time_elapsed(state->last_state_change_time, now,
																						config->hysteresis_period_sec);

						bool cooling_needed = config->cooling_enabled &&
																 temp > (cool_setpoint + deadband) &&
																 time_elapsed(state->last_state_change_time, now,
																						config->hysteresis_period_sec);

						if (heating_needed || cooling_needed) {
								// If fan is disabled (non-HVAC), activate thermal immediately
								if (!config->fan_enabled) {
										state->thermal_state = heating_needed ? THERMAL_STATE_HEATING
																													: THERMAL_STATE_COOLING;
										state->thermal_pending = false;
								}
								// If fan is enabled (HVAC system), ensure fan pre-run before thermal
								else if (!state->thermal_pending) {
										// Start thermal pending (fan will start in update_fan_domain)
										state->thermal_pending = true;
										state->thermal_pending_time = now;
								} else if (time_elapsed(state->thermal_pending_time, now,
																			 config->fan_pre_run_sec)) {
										// Fan pre-run complete, safe to activate thermal
										state->thermal_state = heating_needed ? THERMAL_STATE_HEATING
																													: THERMAL_STATE_COOLING;
										state->thermal_pending = false;
								}
						} else {
								state->thermal_pending = false;
						}
						break;

				case THERMAL_STATE_HEATING: {
						const bool min_cycle_met = time_elapsed(state->thermal_state_enter_time, now,
																										 config->min_cycle_time_sec);

						// Target reached
						if (temp >= heat_setpoint) {
								if (min_cycle_met) {
										state->thermal_state = THERMAL_STATE_IDLE;
										state->thermal_stop_time = now;  // Track when thermal stopped
								}
						}
						// Stage 2 check
						else if (config->heating_stage2_enabled &&
										 time_elapsed(state->thermal_state_enter_time, now,
																config->heat_stage2_delay_sec)) {
								const float deviation = heat_setpoint - temp;
								if (deviation > stage2_threshold) {
										state->thermal_state = THERMAL_STATE_HEATING_STAGE2;
								}
						}
						// Switch to cooling
						if (config->cooling_enabled &&
								temp > (cool_setpoint + deadband) &&
								time_elapsed(state->last_state_change_time, now,
													 config->hysteresis_period_sec) &&
								min_cycle_met) {
								state->thermal_state = THERMAL_STATE_COOLING;
						}
						break;
				}

				case THERMAL_STATE_HEATING_STAGE2: {
						const bool min_cycle_met = time_elapsed(state->thermal_state_enter_time, now,
																										 config->min_cycle_time_sec);

						// Target reached
						if (temp >= heat_setpoint) {
								if (min_cycle_met) {
										state->thermal_state = THERMAL_STATE_IDLE;
										state->thermal_stop_time = now;
								}
						}
						// Drop to stage 1 if deviation reduced
						else {
								const float deviation = heat_setpoint - temp;
								if (deviation <= stage2_threshold && min_cycle_met) {
										state->thermal_state = THERMAL_STATE_HEATING;
								}
						}
						// Switch to cooling
						if (config->cooling_enabled &&
								temp > (cool_setpoint + deadband) &&
								time_elapsed(state->last_state_change_time, now,
													 config->hysteresis_period_sec) &&
								min_cycle_met) {
								state->thermal_state = THERMAL_STATE_COOLING;
						}
						break;
				}

				case THERMAL_STATE_COOLING: {
						const bool min_cycle_met = time_elapsed(state->thermal_state_enter_time, now,
																										 config->min_cycle_time_sec);

						// Target reached
						if (temp <= cool_setpoint) {
								if (min_cycle_met) {
										state->thermal_state = THERMAL_STATE_IDLE;
										state->thermal_stop_time = now;
								}
						}
						// Stage 2 check
						else if (config->cooling_stage2_enabled &&
										 time_elapsed(state->thermal_state_enter_time, now,
																config->cool_stage2_delay_sec)) {
								const float deviation = temp - cool_setpoint;
								if (deviation > stage2_threshold) {
										state->thermal_state = THERMAL_STATE_COOLING_STAGE2;
								}
						}
						// Switch to heating
						if (config->heating_enabled &&
								temp < (heat_setpoint - deadband) &&
								time_elapsed(state->last_state_change_time, now,
													 config->hysteresis_period_sec) &&
								min_cycle_met) {
								state->thermal_state = THERMAL_STATE_HEATING;
						}
						break;
				}

				case THERMAL_STATE_COOLING_STAGE2: {
						const bool min_cycle_met = time_elapsed(state->thermal_state_enter_time, now,
																										 config->min_cycle_time_sec);

						// Target reached
						if (temp <= cool_setpoint) {
								if (min_cycle_met) {
										state->thermal_state = THERMAL_STATE_IDLE;
										state->thermal_stop_time = now;
								}
						}
						// Drop to stage 1 if deviation reduced
						else {
								const float deviation = temp - cool_setpoint;
								if (deviation <= stage2_threshold && min_cycle_met) {
										state->thermal_state = THERMAL_STATE_COOLING;
								}
						}
						// Switch to heating
						if (config->heating_enabled &&
								temp < (heat_setpoint - deadband) &&
								time_elapsed(state->last_state_change_time, now,
													 config->hysteresis_period_sec) &&
								min_cycle_met) {
								state->thermal_state = THERMAL_STATE_HEATING;
						}
						break;
				}
		}

		// Update state tracking only if changed
		if (state->thermal_state != prev_state) {
				state->last_state_change_time = now;

				const bool is_idle_transition = (prev_state == THERMAL_STATE_IDLE ||
																				 state->thermal_state == THERMAL_STATE_IDLE);
				const bool is_heat_to_cool = ((prev_state == THERMAL_STATE_HEATING ||
																			 prev_state == THERMAL_STATE_HEATING_STAGE2) &&
																			(state->thermal_state == THERMAL_STATE_COOLING ||
																			 state->thermal_state == THERMAL_STATE_COOLING_STAGE2));
				const bool is_cool_to_heat = ((prev_state == THERMAL_STATE_COOLING ||
																			 prev_state == THERMAL_STATE_COOLING_STAGE2) &&
																			(state->thermal_state == THERMAL_STATE_HEATING ||
																			 state->thermal_state == THERMAL_STATE_HEATING_STAGE2));

				if (is_idle_transition || is_heat_to_cool || is_cool_to_heat) {
						state->thermal_state_enter_time = now;
				}
		}

		// Map states to outputs (SINGLE SOURCE OF TRUTH)
		output->heating_stage1 = (state->thermal_state == THERMAL_STATE_HEATING ||
															state->thermal_state == THERMAL_STATE_HEATING_STAGE2);
		output->heating_stage2 = (state->thermal_state == THERMAL_STATE_HEATING_STAGE2);
		output->cooling_stage1 = (state->thermal_state == THERMAL_STATE_COOLING ||
															state->thermal_state == THERMAL_STATE_COOLING_STAGE2);
		output->cooling_stage2 = (state->thermal_state == THERMAL_STATE_COOLING_STAGE2);
}

// ============================================================================
// FAN DOMAIN LOGIC
// ============================================================================

static void update_fan_domain(
		const ThermostatConfig* config,
		const ThermostatInput* input,
		ThermostatState* state,
		const ThermostatOutput* thermal_output,
		ThermostatOutput* output
) {
		if (!config->fan_enabled) {
				output->fan = false;
				state->fan_running = false;
				return;
		}

		const bool thermal_active = (thermal_output->heating_stage1 || thermal_output->cooling_stage1);
		const uint32_t now = input->now_seconds;

		// Fan should run if:
		// 1. Thermal is currently active
		// 2. Thermal is pending (pre-run period)
		// 3. Within post-run period after thermal stopped
		// 4. User override

		bool fan_needed = false;

		if (thermal_active) {
				fan_needed = true;
		} else if (state->thermal_pending) {
				// Thermal wants to activate, run fan for pre-run
				fan_needed = true;
		} else if (state->thermal_stop_time > 0 &&
							 !time_elapsed(state->thermal_stop_time, now, config->fan_post_run_sec)) {
				// Within post-run period
				fan_needed = true;
		} else if (input->fan_override) {
				// User manual override
				fan_needed = true;
		}

		// Track state changes
		if (fan_needed && !state->fan_running) {
				state->fan_start_time = now;
				state->fan_running = true;
		} else if (!fan_needed && state->fan_running) {
				state->fan_running = false;
				// Clear thermal_stop_time after post-run completes
				if (state->thermal_stop_time > 0 &&
						time_elapsed(state->thermal_stop_time, now, config->fan_post_run_sec)) {
						state->thermal_stop_time = 0;
				}
		}

		output->fan = state->fan_running;
}

// ============================================================================
// HUMIDITY DOMAIN LOGIC
// ============================================================================

static void update_humidity_domain(
		const ThermostatConfig* config,
		const ThermostatInput* input,
		ThermostatState* state,
		ThermostatOutput* output
) {
		output->humidifier = false;
		output->dehumidifier = false;

		if (!config->humidity_control_enabled || !input->sensors_valid) {
				state->humidity_active = false;
				return;
		}

		const float humidity = input->humidity;
		const float setpoint = config->humidity_setpoint;
		const float deadband = 5.0f; // 5% humidity deadband

		bool should_activate;

		if (config->humidity_mode == HUMIDITY_MODE_HUMIDIFY) {
				if (humidity < (setpoint - deadband)) {
						should_activate = true;
				} else if (humidity > setpoint) {
						should_activate = false;
				} else {
						should_activate = state->humidity_active; // Maintain state in deadband
				}

				if (should_activate) {
						output->humidifier = true;
						if (!state->humidity_active) {
								state->humidity_start_time = input->now_seconds;
						}
				}
		} else { // DEHUMIDIFY
				if (humidity > (setpoint + deadband)) {
						should_activate = true;
				} else if (humidity < setpoint) {
						should_activate = false;
				} else {
						should_activate = state->humidity_active;
				}

				if (should_activate) {
						output->dehumidifier = true;
						if (!state->humidity_active) {
								state->humidity_start_time = input->now_seconds;
						}
				}
		}

		state->humidity_active = should_activate;
}

// ============================================================================
// HOT WATER DOMAIN LOGIC
// ============================================================================

static void update_hot_water_domain(
		const ThermostatConfig* config,
		const ThermostatInput* input,
		ThermostatState* state,
		ThermostatOutput* output
) {
		if (!config->hot_water_enabled) {
				output->hot_water = false;
				state->hot_water_active = false;
				return;
		}

		// Hot water is purely demand-driven
		const bool demand = input->hot_water_demand;

		if (demand && !state->hot_water_active) {
				state->hot_water_start_time = input->now_seconds;
				state->hot_water_active = true;
		} else if (!demand && state->hot_water_active) {
				state->hot_water_active = false;
		}

		output->hot_water = state->hot_water_active;
}

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

void thermostat_config_init(ThermostatConfig* config) {
		memset(config, 0, sizeof(ThermostatConfig));

		config->temp_unit = TEMP_UNIT_CELSIUS;
		config->comfort_mode = COMFORT_MODE_COMFORT;
		config->humidity_mode = HUMIDITY_MODE_HUMIDIFY;

		config->heating_enabled = true;
		config->cooling_enabled = true;
		config->fan_enabled = true;

		config->heat_setpoint = 20.0f;
		config->cool_setpoint = 24.0f;
		config->humidity_setpoint = 50.0f;

		config->hysteresis_period_sec = DEFAULT_HYSTERESIS_PERIOD_SEC;
		config->heat_stage2_delay_sec = DEFAULT_HEAT_STAGE2_DELAY_SEC;
		config->cool_stage2_delay_sec = DEFAULT_COOL_STAGE2_DELAY_SEC;
		config->min_cycle_time_sec = DEFAULT_MIN_CYCLE_TIME_SEC;

		// Fan timing (NEW)
		config->fan_pre_run_sec = DEFAULT_FAN_PRE_RUN_SEC;
		config->fan_post_run_sec = DEFAULT_FAN_POST_RUN_SEC;

		config->comfort_deadband = DEFAULT_COMFORT_DEADBAND_C;
		config->eco_deadband = DEFAULT_ECO_DEADBAND_C;
}

bool thermostat_config_validate(const ThermostatConfig* config) {
		// Count enabled domains
		int enabled_count = config->heating_enabled + config->heating_stage2_enabled +
											 config->cooling_enabled + config->cooling_stage2_enabled +
											 config->fan_enabled + config->humidity_control_enabled +
											 config->hot_water_enabled;

		if (enabled_count > 4) {
				return false; // Max 4 domains can be enabled
		}

		// Stage 2 requires stage 1
		if ((config->heating_stage2_enabled && !config->heating_enabled) ||
				(config->cooling_stage2_enabled && !config->cooling_enabled)) {
				return false;
		}

		// Setpoint sanity checks
		if (config->heating_enabled && config->cooling_enabled &&
				config->heat_setpoint >= config->cool_setpoint) {
				return false;
		}

		if (config->humidity_setpoint < 0.0f || config->humidity_setpoint > 100.0f) {
				return false;
		}

		return true;
}

void thermostat_state_init(ThermostatState* state, uint32_t now_seconds) {
		memset(state, 0, sizeof(ThermostatState));
		state->thermal_state = THERMAL_STATE_IDLE;
		state->last_state_change_time = now_seconds;
}

void thermostat_update(
		const ThermostatConfig* config,
		const ThermostatInput* input,
		ThermostatState* state,
		ThermostatOutput* output
) {
		// Clear output
		memset(output, 0, sizeof(ThermostatOutput));

		// Update each domain independently
		update_thermal_domain(config, input, state, output);
		update_fan_domain(config, input, state, output, output);
		update_humidity_domain(config, input, state, output);
		update_hot_water_domain(config, input, state, output);
}

float thermostat_c_to_f(float celsius) {
		return (celsius * 9.0f / 5.0f) + 32.0f;
}

float thermostat_f_to_c(float fahrenheit) {
		return (fahrenheit - 32.0f) * 5.0f / 9.0f;
}