// ============================================================================
// THERMOSTAT CORE TEST SUITE
// ============================================================================
//
// Comprehensive test suite for the thermostat control logic.
// Tests all features including thermal control, stage 2 heating/cooling,
// humidity control, fan operation, hot water, and safety mechanisms.
//
// BUILD & RUN:
//   make test              # Build and run all tests
//   ./test_thermostat # Run tests directly
//
// Or manually:
//   gcc -Wall -Wextra -std=c11 -I. src/thermostat.c tests/test_thermostat.c -o test_thermostat -lm && ./test_thermostat
//
// COVERAGE:
//   - Configuration validation
//   - Hysteresis and min_cycle_time protection
//   - Comfort vs ECO deadband modes
//   - Stage 2 heating/cooling with deviation logic
//   - Heat/cool mutual exclusion
//   - Fan control (thermal-dependent + override)
//   - Humidity control (humidify/dehumidify modes)
//   - Hot water demand-driven control
//   - Sensor failure safety shutdown
//   - Temperature unit conversion (Celsius/Fahrenheit)
//   - Real-world simulation scenario
//
// OUTPUT:
//   - Color-coded test results (green=pass, red=fail)
//   - Related configuration display for each test
//   - Detailed step-by-step execution traces
//   - Final summary table with pass/fail counts
//
// REQUIREMENTS:
//   - ANSI color terminal support (optional, for colored output)
//   - C11 compiler with standard library
//   - thermostat.h and thermostat.c source files
//
// ============================================================================

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include "../src/thermostat.h"

// ANSI color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"

//TODO: Tests need verification they cover correctly all cases
// Test result tracking
#define MAX_TESTS 20
#define MAX_NAME_LEN 64

static struct {
		char name[MAX_NAME_LEN];
		bool passed;
} test_results[MAX_TESTS];

static int total_tests = 0;
static int passed_tests = 0;

// Forward declarations
static void print_test_summary(void);
static void simulate_scenario(void);

// Test helper macros
#define TEST_START(num, ...) do { \
		printf("\n" COLOR_CYAN "=== TEST %d: " __VA_ARGS__ " ===" COLOR_RESET "\n", num); \
		strncpy(test_results[total_tests].name, "" __VA_ARGS__, MAX_NAME_LEN - 1); \
		test_results[total_tests].name[MAX_NAME_LEN - 1] = '\0'; \
		test_results[total_tests].passed = false; \
} while(0)

#define TEST_PASS(num, ...) do { \
		printf(COLOR_GREEN "✓ TEST %d PASSED: " __VA_ARGS__ COLOR_RESET "\n", num); \
		test_results[total_tests].passed = true; \
		total_tests++; \
		passed_tests++; \
} while(0)

#define TEST_FAIL(num, ...) do { \
		printf(COLOR_RED "✗ TEST %d FAILED: " __VA_ARGS__ COLOR_RESET "\n", num); \
		test_results[total_tests].passed = false; \
		total_tests++; \
} while(0)

// Helper to print related configuration
static void print_related_config(const char* title, const ThermostatConfig* config, const char* params[], int count) {
		printf(COLOR_BLUE "Related Configuration (%s):" COLOR_RESET "\n", title);
		for (int i = 0; i < count; i++) {
				const char* param = params[i];
				if (strcmp(param, "temp_unit") == 0) {
						printf("  Temperature Unit: %s\n",
									 config->temp_unit == TEMP_UNIT_CELSIUS ? "Celsius" : "Fahrenheit");
				} else if (strcmp(param, "comfort_mode") == 0) {
						printf("  Comfort Mode: %s\n",
									 config->comfort_mode == COMFORT_MODE_COMFORT ? "Comfort" : "ECO");
				} else if (strcmp(param, "heat_setpoint") == 0) {
						printf("  Heat Setpoint: %.1f°%s\n",
									 config->heat_setpoint,
									 config->temp_unit == TEMP_UNIT_CELSIUS ? "C" : "F");
				} else if (strcmp(param, "cool_setpoint") == 0) {
						printf("  Cool Setpoint: %.1f°%s\n",
									 config->cool_setpoint,
									 config->temp_unit == TEMP_UNIT_CELSIUS ? "C" : "F");
				} else if (strcmp(param, "comfort_deadband") == 0) {
						printf("  Comfort Deadband: %.1f°\n", config->comfort_deadband);
				} else if (strcmp(param, "eco_deadband") == 0) {
						printf("  ECO Deadband: %.1f°\n", config->eco_deadband);
				} else if (strcmp(param, "hysteresis_period_sec") == 0) {
						printf("  Hysteresis Period: %u seconds\n", config->hysteresis_period_sec);
				} else if (strcmp(param, "heat_stage2_delay_sec") == 0) {
						printf("  Heat Stage 2 Delay: %u seconds\n", config->heat_stage2_delay_sec);
				} else if (strcmp(param, "cool_stage2_delay_sec") == 0) {
						printf("  Cool Stage 2 Delay: %u seconds\n", config->cool_stage2_delay_sec);
				} else if (strcmp(param, "min_cycle_time_sec") == 0) {
						printf("  Min Cycle Time: %u seconds\n", config->min_cycle_time_sec);
				} else if (strcmp(param, "heating_enabled") == 0) {
						printf("  Heating Enabled: %s\n", config->heating_enabled ? "Yes" : "No");
				} else if (strcmp(param, "heating_stage2_enabled") == 0) {
						printf("  Heating Stage 2 Enabled: %s\n", config->heating_stage2_enabled ? "Yes" : "No");
				} else if (strcmp(param, "cooling_enabled") == 0) {
						printf("  Cooling Enabled: %s\n", config->cooling_enabled ? "Yes" : "No");
				} else if (strcmp(param, "cooling_stage2_enabled") == 0) {
						printf("  Cooling Stage 2 Enabled: %s\n", config->cooling_stage2_enabled ? "Yes" : "No");
				} else if (strcmp(param, "fan_enabled") == 0) {
						printf("  Fan Enabled: %s\n", config->fan_enabled ? "Yes" : "No");
				} else if (strcmp(param, "humidity_control_enabled") == 0) {
						printf("  Humidity Control Enabled: %s\n", config->humidity_control_enabled ? "Yes" : "No");
				} else if (strcmp(param, "hot_water_enabled") == 0) {
						printf("  Hot Water Enabled: %s\n", config->hot_water_enabled ? "Yes" : "No");
				} else if (strcmp(param, "humidity_mode") == 0) {
						printf("  Humidity Mode: %s\n",
									 config->humidity_mode == HUMIDITY_MODE_HUMIDIFY ? "Humidify" : "Dehumidify");
				} else if (strcmp(param, "humidity_setpoint") == 0) {
						printf("  Humidity Setpoint: %.1f%%\n", config->humidity_setpoint);
				}
		}
}

// Helper to print full configuration
static void print_full_config(const ThermostatConfig* config) {
		printf(COLOR_BLUE "Full Configuration:" COLOR_RESET "\n");
		printf("  Temperature Unit: %s\n",
					 config->temp_unit == TEMP_UNIT_CELSIUS ? "Celsius" : "Fahrenheit");
		printf("  Comfort Mode: %s\n",
					 config->comfort_mode == COMFORT_MODE_COMFORT ? "Comfort" : "ECO");
		printf("  Heat Setpoint: %.1f°%s\n",
					 config->heat_setpoint,
					 config->temp_unit == TEMP_UNIT_CELSIUS ? "C" : "F");
		printf("  Cool Setpoint: %.1f°%s\n",
					 config->cool_setpoint,
					 config->temp_unit == TEMP_UNIT_CELSIUS ? "C" : "F");
		printf("  Comfort Deadband: %.1f°\n", config->comfort_deadband);
		printf("  ECO Deadband: %.1f°\n", config->eco_deadband);
		printf("  Hysteresis Period: %u seconds\n", config->hysteresis_period_sec);
		printf("  Heat Stage 2 Delay: %u seconds\n", config->heat_stage2_delay_sec);
		printf("  Cool Stage 2 Delay: %u seconds\n", config->cool_stage2_delay_sec);
		printf("  Min Cycle Time: %u seconds\n", config->min_cycle_time_sec);
		printf("  Heating Enabled: %s\n", config->heating_enabled ? "Yes" : "No");
		printf("  Heating Stage 2 Enabled: %s\n", config->heating_stage2_enabled ? "Yes" : "No");
		printf("  Cooling Enabled: %s\n", config->cooling_enabled ? "Yes" : "No");
		printf("  Cooling Stage 2 Enabled: %s\n", config->cooling_stage2_enabled ? "Yes" : "No");
		printf("  Fan Enabled: %s\n", config->fan_enabled ? "Yes" : "No");
		printf("  Humidity Control Enabled: %s\n", config->humidity_control_enabled ? "Yes" : "No");
		printf("  Hot Water Enabled: %s\n", config->hot_water_enabled ? "Yes" : "No");
		if (config->humidity_control_enabled) {
				printf("  Humidity Mode: %s\n",
							 config->humidity_mode == HUMIDITY_MODE_HUMIDIFY ? "Humidify" : "Dehumidify");
				printf("  Humidity Setpoint: %.1f%%\n", config->humidity_setpoint);
		}
}

// ============================================================================
// TEST 1: Configuration Validation - Domain Dependencies
// ============================================================================

static void test_1_config_validation(void) {
		TEST_START(1, "Configuration Validation - Domain Dependencies");

		printf(COLOR_YELLOW "Testing: Configuration constraints for stage 2 dependencies and setpoint ordering\n" COLOR_RESET);
		printf("This test ensures stage 2 modes require stage 1, and heat < cool setpoints.\n\n");

		ThermostatConfig config;
		thermostat_config_init(&config);

		const char* related[] = {"heating_enabled", "heating_stage2_enabled",
														 "cooling_enabled", "cooling_stage2_enabled",
														 "heat_setpoint", "cool_setpoint"};
		print_related_config("Domain Dependencies", &config, related, 6);
		print_full_config(&config);

		// Valid default config
		assert(thermostat_config_validate(&config));
		printf("  ✓ Default config is valid\n");

		// Stage 2 without stage 1 should fail
		config.heating_stage2_enabled = true;
		config.heating_enabled = false;
		assert(!thermostat_config_validate(&config));
		printf("  ✓ Heating stage 2 without stage 1 correctly rejected\n");

		// Reset
		thermostat_config_init(&config);
		config.cooling_stage2_enabled = true;
		config.cooling_enabled = false;
		assert(!thermostat_config_validate(&config));
		printf("  ✓ Cooling stage 2 without stage 1 correctly rejected\n");

		// Heat setpoint >= cool setpoint should fail
		thermostat_config_init(&config);
		config.heat_setpoint = 25.0f;
		config.cool_setpoint = 20.0f;
		assert(!thermostat_config_validate(&config));
		printf("  ✓ Invalid setpoint order (heat >= cool) correctly rejected\n");

		// Equal setpoints should also fail
		config.heat_setpoint = 22.0f;
		config.cool_setpoint = 22.0f;
		assert(!thermostat_config_validate(&config));
		printf("  ✓ Equal setpoints correctly rejected\n");

		TEST_PASS(1, "Configuration Validation - Domain Dependencies");
}

// ============================================================================
// TEST 2: Hysteresis Period - Prevents Short Cycling
// ============================================================================

static void test_2_hysteresis_period(void) {
		TEST_START(2, "Hysteresis Period - Prevents Short Cycling");

		printf(COLOR_YELLOW "Testing: Hysteresis prevents rapid state changes between thermal modes\n" COLOR_RESET);
		printf("The system should wait 'hysteresis_period_sec' before switching between IDLE/HEAT/COOL.\n\n");
		printf(COLOR_BLUE "Note: min_cycle_time_sec protects equipment once running\n" COLOR_RESET);

		ThermostatConfig config;
		thermostat_config_init(&config);
		config.heat_setpoint = 20.0f;
		config.comfort_deadband = 0.5f;
		config.hysteresis_period_sec = 300;  // 5 minutes
		config.min_cycle_time_sec = 180;     // 3 minutes
		config.fan_enabled = false;          // Disable fan - test thermal logic only
		config.cooling_enabled = false;

		const char* related[] = {"hysteresis_period_sec", "min_cycle_time_sec", "heat_setpoint", "comfort_deadband", "fan_enabled"};
		print_related_config("Hysteresis", &config, related, 5);
		print_full_config(&config);

		ThermostatState state;
		ThermostatInput input = {0};
		ThermostatOutput output;

		// Initialize state at a time in the past to ensure hysteresis blocks initial activation
		thermostat_state_init(&state, 0);
		state.last_state_change_time = 0;
		input.sensors_valid = true;

		// Start at 18°C (below setpoint - deadband = 19.5°C)
		// At t=0, last state change was at t=0, so hysteresis not met
		input.temperature = 18.0f;
		input.now_seconds = 0;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		printf("  t=%4us, temp=%.1f°C: heating=%d " COLOR_YELLOW "(blocked by hysteresis)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1);

		// Still blocked before hysteresis expires
		input.now_seconds = 299;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		printf("  t=%4us, temp=%.1f°C: heating=%d " COLOR_YELLOW "(1 sec before hysteresis expires)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1);

		// Hysteresis expires - heating activates immediately (fan disabled)
		input.now_seconds = 300;
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		printf("  t=%4us, temp=%.1f°C: heating=%d " COLOR_GREEN "(hysteresis expired, heating ON)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1);

		// Temperature rises to setpoint before min_cycle_time
		input.temperature = 20.0f;
		input.now_seconds = 400;  // Only 100s after heating started (< 180s min_cycle_time)
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);  // Should STAY ON due to min_cycle_time!
		printf("  t=%4us, temp=%.1f°C: heating=%d " COLOR_YELLOW "(target reached, but min_cycle_time not met)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1);

		// After min_cycle_time, heating can turn off
		input.now_seconds = 481;  // 181s after heating started (> 180s min_cycle_time)
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		printf("  t=%4us, temp=%.1f°C: heating=%d " COLOR_GREEN "(min_cycle_time met, heating OFF)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1);

		// Drop temperature again immediately - should be blocked by hysteresis
		input.temperature = 18.0f;
		input.now_seconds = 550;  // Only 69s after turning off
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		printf("  t=%4us, temp=%.1f°C: heating=%d " COLOR_YELLOW "(blocked by hysteresis after state change)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1);

		// After hysteresis expires, can restart immediately (fan disabled)
		input.now_seconds = 782;  // 301s after turning off (> 300s hysteresis)
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		printf("  t=%4us, temp=%.1f°C: heating=%d " COLOR_GREEN "(hysteresis expired, heating restarted)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1);

		TEST_PASS(2, "Hysteresis Period - Prevents Short Cycling");
}

// ============================================================================
// TEST 3: Comfort Deadband - Temperature Threshold
// ============================================================================

static void test_3_comfort_deadband(void) {
		TEST_START(3, "Comfort Deadband - Temperature Threshold");

		printf(COLOR_YELLOW "Testing: Comfort mode deadband creates activation threshold around setpoint\n" COLOR_RESET);
		printf("Heating activates at (setpoint - deadband), cooling at (setpoint + deadband).\n\n");

		ThermostatConfig config;
		thermostat_config_init(&config);
		config.comfort_mode = COMFORT_MODE_COMFORT;
		config.heat_setpoint = 20.0f;
		config.cool_setpoint = 24.0f;
		config.comfort_deadband = 0.5f;
		config.hysteresis_period_sec = 0;
		config.min_cycle_time_sec = 0;  // Disable for clean threshold testing
		config.fan_enabled = false;     // Disable fan - test thermal logic only

		const char* related[] = {"comfort_mode", "comfort_deadband", "heat_setpoint", "cool_setpoint", "min_cycle_time_sec", "fan_enabled"};
		print_related_config("Deadband", &config, related, 6);
		print_full_config(&config);

		ThermostatState state;
		ThermostatInput input = {0};
		ThermostatOutput output;

		thermostat_state_init(&state, 0);
		input.sensors_valid = true;

		// Test heating activation threshold
		// At 19.6°C: above (20.0 - 0.5 = 19.5°C), should NOT activate
		input.temperature = 19.6f;
		input.now_seconds = 10;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		printf("  temp=%.1f°C (above threshold 19.5°C): heating=%d " COLOR_GREEN "(correctly OFF)" COLOR_RESET "\n",
					 input.temperature, output.heating_stage1);

		// At 19.4°C: below threshold, should activate immediately (fan disabled)
		input.temperature = 19.4f;
		input.now_seconds = 20;
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		printf("  temp=%.1f°C (below threshold 19.5°C): heating=%d " COLOR_GREEN "(correctly ON)" COLOR_RESET "\n",
					 input.temperature, output.heating_stage1);

		// Return to setpoint - heating turns off (min_cycle_time disabled)
		input.temperature = 20.0f;
		input.now_seconds = 30;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		printf("  temp=%.1f°C (at setpoint): heating=%d " COLOR_GREEN "(turns OFF)" COLOR_RESET "\n",
					 input.temperature, output.heating_stage1);

		// Test cooling activation threshold
		// At 24.4°C: below (24.0 + 0.5 = 24.5°C), should NOT activate
		input.temperature = 24.4f;
		input.now_seconds = 40;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.cooling_stage1);
		printf("  temp=%.1f°C (below threshold 24.5°C): cooling=%d " COLOR_GREEN "(correctly OFF)" COLOR_RESET "\n",
					 input.temperature, output.cooling_stage1);

		// At 24.6°C: above threshold, should activate immediately (fan disabled)
		input.temperature = 24.6f;
		input.now_seconds = 50;
		thermostat_update(&config, &input, &state, &output);
		assert(output.cooling_stage1);
		printf("  temp=%.1f°C (above threshold 24.5°C): cooling=%d " COLOR_GREEN "(correctly ON)" COLOR_RESET "\n",
					 input.temperature, output.cooling_stage1);

		TEST_PASS(3, "Comfort Deadband - Temperature Threshold");
}

// ============================================================================
// TEST 4: ECO Deadband - Wider Temperature Band
// ============================================================================

static void test_4_eco_deadband(void) {
		TEST_START(4, "ECO Deadband - Wider Temperature Band");

		printf(COLOR_YELLOW "Testing: ECO mode uses larger deadband for energy savings\n" COLOR_RESET);
		printf("Wider deadband means fewer heating/cooling cycles but larger temp swings.\n\n");

		ThermostatConfig config;
		thermostat_config_init(&config);
		config.comfort_mode = COMFORT_MODE_ECO;
		config.heat_setpoint = 20.0f;
		config.cool_setpoint = 24.0f;
		config.eco_deadband = 1.5f;  // Larger than comfort (0.5)
		config.hysteresis_period_sec = 0;
		config.min_cycle_time_sec = 0;  // Disable for clean threshold testing
		config.fan_enabled = false;          // Disable fan - test thermal logic only

		const char* related[] = {"comfort_mode", "eco_deadband", "heat_setpoint", "cool_setpoint", "min_cycle_time_sec"};
		print_related_config("ECO Mode", &config, related, 5);
		print_full_config(&config);

		ThermostatState state;
		ThermostatInput input = {0};
		ThermostatOutput output;

		thermostat_state_init(&state, 0);
		input.sensors_valid = true;

		// Heating threshold in ECO: 20.0 - 1.5 = 18.5°C
		// At 19.0°C: should NOT activate (above threshold)
		input.temperature = 19.0f;
		input.now_seconds = 10;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		printf("  temp=%.1f°C (above ECO threshold 18.5°C): heating=%d " COLOR_GREEN "(wider deadband, no activation)" COLOR_RESET "\n",
					 input.temperature, output.heating_stage1);

		// At 18.0°C: below threshold, should activate
		input.temperature = 18.0f;
		input.now_seconds = 20;
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		printf("  temp=%.1f°C (below ECO threshold 18.5°C): heating=%d " COLOR_GREEN "(correctly ON)" COLOR_RESET "\n",
					 input.temperature, output.heating_stage1);

		// Return to setpoint - heating turns off
		input.temperature = 20.0f;
		input.now_seconds = 30;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		printf("  temp=%.1f°C (at setpoint): heating=%d " COLOR_GREEN "(turns OFF)" COLOR_RESET "\n",
					 input.temperature, output.heating_stage1);

		// Cooling threshold in ECO: 24.0 + 1.5 = 25.5°C
		// At 25.0°C: should NOT activate (below threshold)
		input.temperature = 25.0f;
		input.now_seconds = 40;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.cooling_stage1);
		printf("  temp=%.1f°C (below ECO threshold 25.5°C): cooling=%d " COLOR_GREEN "(wider deadband, no activation)" COLOR_RESET "\n",
					 input.temperature, output.cooling_stage1);

		// At 26.0°C: above threshold, should activate
		input.temperature = 26.0f;
		input.now_seconds = 50;
		thermostat_update(&config, &input, &state, &output);
		assert(output.cooling_stage1);
		printf("  temp=%.1f°C (above ECO threshold 25.5°C): cooling=%d " COLOR_GREEN "(correctly ON)" COLOR_RESET "\n",
					 input.temperature, output.cooling_stage1);

		TEST_PASS(4, "ECO Deadband - Wider Temperature Band");
}

// ============================================================================
// TEST 5: Heat Stage 2 Delay - Gradual Power Increase
// ============================================================================

static void test_5_heat_stage2_delay(void) {
		TEST_START(5, "Heat Stage 2 Delay - Gradual Power Increase");

		printf(COLOR_YELLOW "Testing: Stage 2 heating activates after delay + deviation threshold\n" COLOR_RESET);
		printf("Prevents unnecessary high-power operation for minor temperature drops.\n\n");

		ThermostatConfig config;
		thermostat_config_init(&config);
		config.heating_stage2_enabled = true;
		config.heat_setpoint = 20.0f;
		config.comfort_deadband = 0.5f;
		config.heat_stage2_delay_sec = 300;  // 5 minutes
		config.hysteresis_period_sec = 0;
		config.min_cycle_time_sec = 0;
		config.fan_enabled = false;          // Disable fan - test thermal logic only
		config.cooling_enabled = false;

		const char* related[] = {"heating_stage2_enabled", "heat_stage2_delay_sec", "heat_setpoint", "comfort_deadband", "fan_enabled"};
		print_related_config("Stage 2 Heat", &config, related, 5);
		print_full_config(&config);

		ThermostatState state;
		ThermostatInput input = {0};
		ThermostatOutput output;

		// Initialize state at time 100 to avoid t=0 edge cases
		thermostat_state_init(&state, 100);
		input.sensors_valid = true;
		input.now_seconds = 100;

		// Start heating at 18°C (2°C below setpoint)
		input.temperature = 18.0f;
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		assert(!output.heating_stage2);
		printf("  t=%4us, temp=%.1f°C: stage1=%d, stage2=%d " COLOR_GREEN "(stage 1 only)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.heating_stage2);

		// Wait 4 minutes - still stage 1 only
		input.now_seconds = 340;  // 240s later
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		assert(!output.heating_stage2);
		printf("  t=%4us, temp=%.1f°C: stage1=%d, stage2=%d " COLOR_YELLOW "(delay not met yet)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.heating_stage2);

		// After 5 minutes with large deviation - stage 2 activates
		input.now_seconds = 401;  // 301s after start
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		assert(output.heating_stage2);
		printf("  t=%4us, temp=%.1f°C: stage1=%d, stage2=%d " COLOR_GREEN "(delay + deviation met, stage 2 ON)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.heating_stage2);

		// Temperature rises, but still below setpoint
		input.temperature = 19.3f;
		input.now_seconds = 500;
		thermostat_update(&config, &input, &state, &output);
		// Deviation now only 0.7°C (< stage2_threshold of 0.75°C), should drop to stage 1
		assert(output.heating_stage1);
		assert(!output.heating_stage2);
		printf("  t=%4us, temp=%.1f°C: stage1=%d, stage2=%d " COLOR_GREEN "(deviation reduced, back to stage 1)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.heating_stage2);

		TEST_PASS(5, "Heat Stage 2 Delay - Gradual Power Increase");
}

// ============================================================================
// TEST 6: Cool Stage 2 Delay - Compressor Protection
// ============================================================================

static void test_6_cool_stage2_delay(void) {
		TEST_START(6, "Cool Stage 2 Delay - Gradual Power Increase");

		printf(COLOR_YELLOW "Testing: Stage 2 cooling activates after delay + deviation threshold\n" COLOR_RESET);
		printf("Prevents unnecessary high-power operation for minor temperature rises.\n\n");

		ThermostatConfig config;
		thermostat_config_init(&config);
		config.cooling_stage2_enabled = true;
		config.cool_setpoint = 24.0f;
		config.comfort_deadband = 0.5f;
		config.cool_stage2_delay_sec = 300;  // 5 minutes
		config.hysteresis_period_sec = 0;
		config.min_cycle_time_sec = 0;
		config.fan_enabled = false;          // Disable fan - test thermal logic only
		config.heating_enabled = false;

		const char* related[] = {"cooling_stage2_enabled", "cool_stage2_delay_sec", "cool_setpoint", "comfort_deadband", "fan_enabled"};
		print_related_config("Stage 2 Cool", &config, related, 5);
		print_full_config(&config);

		ThermostatState state;
		ThermostatInput input = {0};
		ThermostatOutput output;

		// Initialize state at time 100 to avoid t=0 edge cases
		thermostat_state_init(&state, 100);
		input.sensors_valid = true;
		input.now_seconds = 100;

		// Start cooling at 26°C (2°C above setpoint)
		input.temperature = 26.0f;
		thermostat_update(&config, &input, &state, &output);
		assert(output.cooling_stage1);
		assert(!output.cooling_stage2);
		printf("  t=%4us, temp=%.1f°C: stage1=%d, stage2=%d " COLOR_GREEN "(stage 1 only)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.cooling_stage1, output.cooling_stage2);

		// Wait 4 minutes - still stage 1 only
		input.now_seconds = 340;  // 240s later
		thermostat_update(&config, &input, &state, &output);
		assert(output.cooling_stage1);
		assert(!output.cooling_stage2);
		printf("  t=%4us, temp=%.1f°C: stage1=%d, stage2=%d " COLOR_YELLOW "(delay not met yet)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.cooling_stage1, output.cooling_stage2);

		// After 5 minutes with large deviation - stage 2 activates
		input.now_seconds = 401;  // 301s after start
		thermostat_update(&config, &input, &state, &output);
		assert(output.cooling_stage1);
		assert(output.cooling_stage2);
		printf("  t=%4us, temp=%.1f°C: stage1=%d, stage2=%d " COLOR_GREEN "(delay + deviation met, stage 2 ON)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.cooling_stage1, output.cooling_stage2);

		// Temperature drops, but still above setpoint
		input.temperature = 24.7f;
		input.now_seconds = 500;
		thermostat_update(&config, &input, &state, &output);
		// Deviation now only 0.7°C (< stage2_threshold of 0.75°C), should drop to stage 1
		assert(output.cooling_stage1);
		assert(!output.cooling_stage2);
		printf("  t=%4us, temp=%.1f°C: stage1=%d, stage2=%d " COLOR_GREEN "(deviation reduced, back to stage 1)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.cooling_stage1, output.cooling_stage2);

		TEST_PASS(6, "Cool Stage 2 Delay - Gradual Power Increase");
}

// ============================================================================
// TEST 7: Heat/Cool Setpoints - Mutual Exclusion
// ============================================================================

static void test_7_setpoint_mutual_exclusion(void) {
		TEST_START(7, "Heat/Cool Setpoints - Mutual Exclusion");

		printf(COLOR_YELLOW "Testing: Heating and cooling never run simultaneously\n" COLOR_RESET);
		printf("System switches between heating/cooling based on temperature thresholds.\n\n");

		ThermostatConfig config;
		thermostat_config_init(&config);
		config.heat_setpoint = 20.0f;
		config.cool_setpoint = 24.0f;
		config.comfort_deadband = 0.5f;
		config.hysteresis_period_sec = 0;
		config.min_cycle_time_sec = 0;
		config.fan_enabled = false;          // Disable fan - test thermal logic only

		const char* related[] = {"heat_setpoint", "cool_setpoint", "comfort_deadband", "hysteresis_period_sec", "fan_enabled"};
		print_related_config("Mutual Exclusion", &config, related, 5);
		print_full_config(&config);

		ThermostatState state;
		ThermostatInput input = {0};
		ThermostatOutput output;

		thermostat_state_init(&state, 100);
		input.sensors_valid = true;
		input.now_seconds = 100;

		// Cold: heating should activate
		input.temperature = 18.0f;
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		assert(!output.cooling_stage1);
		printf("  temp=%.1f°C: heat=%d, cool=%d " COLOR_GREEN "(heating only)" COLOR_RESET "\n",
					 input.temperature, output.heating_stage1, output.cooling_stage1);

		// Hot: cooling should activate (heating stops)
		input.temperature = 26.0f;
		input.now_seconds = 200;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(output.cooling_stage1);
		printf("  temp=%.1f°C: heat=%d, cool=%d " COLOR_GREEN "(cooling only)" COLOR_RESET "\n",
					 input.temperature, output.heating_stage1, output.cooling_stage1);

		// Neutral zone: both off
		input.temperature = 22.0f;
		input.now_seconds = 300;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(!output.cooling_stage1);
		printf("  temp=%.1f°C: heat=%d, cool=%d " COLOR_GREEN "(both off in neutral zone)" COLOR_RESET "\n",
					 input.temperature, output.heating_stage1, output.cooling_stage1);

		// Verify no simultaneous operation at any point
		assert(!(output.heating_stage1 && output.cooling_stage1));
		printf("\n" COLOR_GREEN "✓ Mutual exclusion verified: heating and cooling never run together" COLOR_RESET "\n");

		TEST_PASS(7, "Heat/Cool Setpoints - Mutual Exclusion");
}

// ============================================================================
// TEST 8: Fan Enabled - Dependency and Override
// ============================================================================

static void test_8_fan_control(void) {
		TEST_START(8, "Fan Control - Pre-run, Post-run, and Override");

		printf(COLOR_YELLOW "Testing: Fan pre-run ensures airflow before thermal, post-run distributes residual heat/cool\n" COLOR_RESET);
		printf("Manual override allows fan to run independently.\n\n");

		ThermostatConfig config;
		thermostat_config_init(&config);
		config.fan_enabled = true;
		config.heat_setpoint = 20.0f;
		config.comfort_deadband = 0.5f;
		config.hysteresis_period_sec = 0;
		config.min_cycle_time_sec = 0;
		config.cooling_enabled = false;
		config.fan_pre_run_sec = 30;         // Real pre-run value
		config.fan_post_run_sec = 60;        // Real post-run value

		const char* related[] = {"fan_enabled", "fan_pre_run_sec", "fan_post_run_sec", "heat_setpoint"};
		print_related_config("Fan Control", &config, related, 4);
		print_full_config(&config);

		ThermostatState state;
		ThermostatInput input = {0};
		ThermostatOutput output;

		thermostat_state_init(&state, 100);
		input.sensors_valid = true;
		input.now_seconds = 100;
		input.fan_override = false;

		// === PRE-RUN PHASE ===
		// Cold temperature - thermal pending, fan starts immediately
		input.temperature = 18.0f;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);  // Heating not active yet
		assert(output.fan);              // Fan starts immediately for pre-run
		printf("  t=%4us, temp=%.1f°C: heat=%d, fan=%d " COLOR_YELLOW "(pre-run: fan ON, heating pending)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.fan);

		// 29 seconds later - still in pre-run
		input.now_seconds = 129;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(output.fan);
		printf("  t=%4us, temp=%.1f°C: heat=%d, fan=%d " COLOR_YELLOW "(pre-run: 1s before activation)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.fan);

		// 30 seconds - pre-run complete, heating activates
		input.now_seconds = 130;
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		assert(output.fan);
		printf("  t=%4us, temp=%.1f°C: heat=%d, fan=%d " COLOR_GREEN "(pre-run complete, heating + fan both ON)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.fan);

		// === POST-RUN PHASE ===
		// Temperature reaches setpoint - heating stops, fan continues
		input.temperature = 20.0f;
		input.now_seconds = 200;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(output.fan);  // Fan still running for post-run
		printf("  t=%4us, temp=%.1f°C: heat=%d, fan=%d " COLOR_YELLOW "(post-run: heating OFF, fan continues)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.fan);

		// 59 seconds after heating stopped
		input.now_seconds = 259;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(output.fan);
		printf("  t=%4us, temp=%.1f°C: heat=%d, fan=%d " COLOR_YELLOW "(post-run: 1s before stopping)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.fan);

		// 60 seconds - post-run complete, fan stops
		input.now_seconds = 260;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(!output.fan);
		printf("  t=%4us, temp=%.1f°C: heat=%d, fan=%d " COLOR_GREEN "(post-run complete, both OFF)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.fan);

		// === MANUAL OVERRIDE ===
		// Fan override - fan runs without heating
		input.fan_override = true;
		input.now_seconds = 300;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(output.fan);
		printf("  t=%4us, fan_override=%d: heat=%d, fan=%d " COLOR_GREEN "(manual override active)" COLOR_RESET "\n",
					 input.now_seconds, input.fan_override, output.heating_stage1, output.fan);

		// Disable override - fan stops immediately
		input.fan_override = false;
		input.now_seconds = 400;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(!output.fan);
		printf("  t=%4us, fan_override=%d: heat=%d, fan=%d " COLOR_GREEN "(override disabled, fan stops)" COLOR_RESET "\n",
					 input.now_seconds, input.fan_override, output.heating_stage1, output.fan);

		TEST_PASS(8, "Fan Control - Pre-run, Post-run, and Override");
}

// ============================================================================
// TEST 9: Humidity Control - Setpoint and Mode
// ============================================================================

static void test_9_humidity_control(void) {
		TEST_START(9, "Humidity Control - Setpoint and Mode");

		printf(COLOR_YELLOW "Testing: Humidity control operates independently with humidify/dehumidify modes\n" COLOR_RESET);
		printf("System activates humidifier or dehumidifier based on mode and setpoint.\n\n");
		printf(COLOR_BLUE "Humidity has 5%% deadband: activates at ±5%%, deactivates at setpoint\n" COLOR_RESET);

		ThermostatConfig config;
		thermostat_config_init(&config);
		config.humidity_control_enabled = true;
		config.humidity_mode = HUMIDITY_MODE_HUMIDIFY;
		config.humidity_setpoint = 50.0f;
		config.heating_enabled = false;
		config.cooling_enabled = false;

		const char* related[] = {"humidity_control_enabled", "humidity_mode", "humidity_setpoint"};
		print_related_config("Humidity", &config, related, 3);
		print_full_config(&config);

		ThermostatState state;
		ThermostatInput input = {0};
		ThermostatOutput output;

		thermostat_state_init(&state, 0);
		input.now_seconds = 0;
		input.sensors_valid = true;
		input.temperature = 20.0f;

		// Humidify mode: Low humidity → humidifier on
		// Activates below (50 - 5 = 45%)
		input.humidity = 40.0f;
		thermostat_update(&config, &input, &state, &output);
		assert(output.humidifier);
		assert(!output.dehumidifier);
		printf("  humidity=%.1f%% (< 45%% threshold): humidifier=%d " COLOR_GREEN "(correctly ON)" COLOR_RESET "\n",
					 input.humidity, output.humidifier);

		// In deadband (45-50%), maintains previous state (still on)
		input.humidity = 47.0f;
		thermostat_update(&config, &input, &state, &output);
		assert(output.humidifier);
		printf("  humidity=%.1f%% (in deadband 45-50%%): humidifier=%d " COLOR_YELLOW "(maintains state)" COLOR_RESET "\n",
					 input.humidity, output.humidifier);

		// Above setpoint → humidifier off
		input.humidity = 52.0f;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.humidifier);
		printf("  humidity=%.1f%% (> 50%% setpoint): humidifier=%d " COLOR_GREEN "(deactivates)" COLOR_RESET "\n",
					 input.humidity, output.humidifier);

		// Back in deadband from above, maintains off state
		input.humidity = 47.0f;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.humidifier);
		printf("  humidity=%.1f%% (in deadband, from above): humidifier=%d " COLOR_YELLOW "(maintains OFF)" COLOR_RESET "\n",
					 input.humidity, output.humidifier);

		// Switch to dehumidify mode
		printf("\n  " COLOR_CYAN "Switching to dehumidify mode" COLOR_RESET "\n");
		config.humidity_mode = HUMIDITY_MODE_DEHUMIDIFY;
		state.humidity_active = false; // Reset state

		// Activates above (50 + 5 = 55%)
		input.humidity = 60.0f;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.humidifier);
		assert(output.dehumidifier);
		printf("  humidity=%.1f%% (> 55%% threshold): dehumidifier=%d " COLOR_GREEN "(correctly ON)" COLOR_RESET "\n",
					 input.humidity, output.dehumidifier);

		// In deadband (50-55%), maintains on
		input.humidity = 53.0f;
		thermostat_update(&config, &input, &state, &output);
		assert(output.dehumidifier);
		printf("  humidity=%.1f%% (in deadband 50-55%%): dehumidifier=%d " COLOR_YELLOW "(maintains state)" COLOR_RESET "\n",
					 input.humidity, output.dehumidifier);

		// Below setpoint → dehumidifier off
		input.humidity = 48.0f;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.dehumidifier);
		printf("  humidity=%.1f%% (< 50%% setpoint): dehumidifier=%d " COLOR_GREEN "(deactivates)" COLOR_RESET "\n",
					 input.humidity, output.dehumidifier);

		// Humidity disabled → nothing runs
		printf("\n  " COLOR_CYAN "Disabling humidity control" COLOR_RESET "\n");
		config.humidity_control_enabled = false;
		input.humidity = 30.0f;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.humidifier);
		assert(!output.dehumidifier);
		printf("  humidity=%.1f%%, control disabled: humidifier=%d, dehumidifier=%d " COLOR_GREEN "(respects config)" COLOR_RESET "\n",
					 input.humidity, output.humidifier, output.dehumidifier);

		TEST_PASS(9, "Humidity Control - Setpoint and Mode");
}

// ============================================================================
// TEST 10: Hot Water - Independent Demand-Driven
// ============================================================================

static void test_10_hot_water(void) {
		TEST_START(10, "Hot Water - Independent Demand-Driven");

		printf(COLOR_YELLOW "Testing: Hot water operates independently based on demand signal\n" COLOR_RESET);
		printf("Completely decoupled from HVAC - purely demand-driven.\n\n");

		ThermostatConfig config;
		thermostat_config_init(&config);
		config.hot_water_enabled = true;
		config.heating_enabled = false;
		config.cooling_enabled = false;

		const char* related[] = {"hot_water_enabled"};
		print_related_config("Hot Water", &config, related, 1);
		print_full_config(&config);

		ThermostatState state;
		ThermostatInput input = {0};
		ThermostatOutput output;

		thermostat_state_init(&state, 0);
		input.now_seconds = 0;
		input.sensors_valid = true;
		input.temperature = 20.0f;
		input.hot_water_demand = false;

		// No demand → off
		thermostat_update(&config, &input, &state, &output);
		assert(!output.hot_water);
		printf("  demand=%d: hot_water=%d " COLOR_GREEN "(correctly OFF)" COLOR_RESET "\n",
					 input.hot_water_demand, output.hot_water);

		// Demand asserted → on
		input.hot_water_demand = true;
		thermostat_update(&config, &input, &state, &output);
		assert(output.hot_water);
		printf("  demand=%d: hot_water=%d " COLOR_GREEN "(activated)" COLOR_RESET "\n",
					 input.hot_water_demand, output.hot_water);

		// Demand removed → off
		input.hot_water_demand = false;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.hot_water);
		printf("  demand=%d: hot_water=%d " COLOR_GREEN "(deactivated)" COLOR_RESET "\n",
					 input.hot_water_demand, output.hot_water);

		// Disabled in config → never runs
		config.hot_water_enabled = false;
		input.hot_water_demand = true;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.hot_water);
		printf("  demand=%d, hot_water disabled: hot_water=%d " COLOR_GREEN "(respects config)" COLOR_RESET "\n",
					 input.hot_water_demand, output.hot_water);

		TEST_PASS(10, "Hot Water - Independent Demand-Driven");
}

// ============================================================================
// TEST 11: Sensor Failure - Safe Shutdown
// ============================================================================

static void test_11_sensor_failure(void) {
		TEST_START(11, "Sensor Failure Safety - Automatic Shutdown");

		printf(COLOR_YELLOW "Testing: All outputs shut down when sensors fail\n" COLOR_RESET);
		printf("Safety mechanism prevents equipment damage from bad sensor data.\n\n");

		ThermostatConfig config;
		thermostat_config_init(&config);
		config.heat_setpoint = 20.0f;
		config.comfort_deadband = 0.5f;
		config.hysteresis_period_sec = 0;
		config.min_cycle_time_sec = 0;
		config.fan_enabled = false;          // Disable fan - test thermal logic only
		config.cooling_enabled = false;

		const char* related[] = {"sensors_valid", "heating_enabled", "fan_enabled"};
		print_related_config("Sensor Failure", &config, related, 3);
		print_full_config(&config);

		ThermostatState state;
		ThermostatInput input = {0};
		ThermostatOutput output;

		thermostat_state_init(&state, 100);
		input.sensors_valid = true;
		input.now_seconds = 100;

		// Start with cold temperature - heating activates
		input.temperature = 18.0f;
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		printf("  sensors_valid=%d, temp=%.1f°C: heating=%d " COLOR_GREEN "(normal operation)" COLOR_RESET "\n",
					 input.sensors_valid, input.temperature, output.heating_stage1);

		// Sensor fails - all outputs should immediately shut down
		input.sensors_valid = false;
		input.now_seconds = 200;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(!output.heating_stage2);
		assert(!output.cooling_stage1);
		assert(!output.cooling_stage2);
		assert(!output.fan);
		assert(!output.humidifier);
		assert(!output.dehumidifier);
		assert(!output.hot_water);
		printf("  sensors_valid=%d: all outputs=%d " COLOR_GREEN "(safety shutdown)" COLOR_RESET "\n",
					 input.sensors_valid, 0);

		// Sensor recovers - system can restart
		input.sensors_valid = true;
		input.temperature = 18.0f;
		input.now_seconds = 300;
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		printf("  sensors_valid=%d, temp=%.1f°C: heating=%d " COLOR_GREEN "(recovery, normal operation)" COLOR_RESET "\n",
					 input.sensors_valid, input.temperature, output.heating_stage1);

		TEST_PASS(11, "Sensor Failure Safety - Automatic Shutdown");
}

// ============================================================================
// TEST 12: Temperature Unit - Celsius vs Fahrenheit
// ============================================================================

static void test_12_temperature_unit(void) {
		TEST_START(12, "Temperature Unit - Celsius vs Fahrenheit");

		printf(COLOR_YELLOW "Testing: System works correctly in both Celsius and Fahrenheit modes\n" COLOR_RESET);
		printf("Deadband and setpoint logic should work identically in both units.\n\n");

		ThermostatConfig config;
		ThermostatInput input = {0};
		ThermostatOutput output;
		ThermostatState state;

		// ========== Test Celsius ==========
		printf(COLOR_CYAN "Testing Celsius mode:\n" COLOR_RESET);
		thermostat_config_init(&config);
		config.temp_unit = TEMP_UNIT_CELSIUS;
		config.heat_setpoint = 20.0f;
		config.comfort_deadband = 0.5f;
		config.hysteresis_period_sec = 0;
		config.min_cycle_time_sec = 0;
		config.fan_enabled = false;          // Disable fan - test thermal logic only
		config.cooling_enabled = false;

		const char* related_c[] = {"temp_unit", "heat_setpoint", "comfort_deadband", "fan_enabled"};
		print_related_config("Celsius", &config, related_c, 4);

		thermostat_state_init(&state, 100);
		input.sensors_valid = true;
		input.now_seconds = 100;

		// Below threshold - heating activates
		input.temperature = 19.4f;
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		printf("  Celsius: temp=%.1f°C (threshold 19.5°C): heating=%d " COLOR_GREEN "(ON)" COLOR_RESET "\n",
					 input.temperature, output.heating_stage1);

		// Temperature reaches setpoint - heating off
		input.temperature = 20.0f;  // At setpoint, not 19.6
		input.now_seconds = 200;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		printf("  Celsius: temp=%.1f°C (at setpoint): heating=%d " COLOR_GREEN "(OFF)" COLOR_RESET "\n",
					 input.temperature, output.heating_stage1);

		// ========== Test Fahrenheit ==========
		printf("\n" COLOR_CYAN "Testing Fahrenheit mode:\n" COLOR_RESET);
		thermostat_config_init(&config);
		config.temp_unit = TEMP_UNIT_FAHRENHEIT;
		config.heat_setpoint = 68.0f;  // ~20°C
		config.comfort_deadband = 1.0f;  // ~0.5°C
		config.hysteresis_period_sec = 0;
		config.min_cycle_time_sec = 0;
		config.fan_enabled = false;
		config.cooling_enabled = false;

		const char* related_f[] = {"temp_unit", "heat_setpoint", "comfort_deadband", "fan_enabled"};
		print_related_config("Fahrenheit", &config, related_f, 4);

		thermostat_state_init(&state, 300);
		input.now_seconds = 300;

		// Below threshold (68.0 - 1.0 = 67.0) - heating activates
		input.temperature = 66.9f;
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		printf("  Fahrenheit: temp=%.1f°F (threshold 67.0°F): heating=%d " COLOR_GREEN "(ON)" COLOR_RESET "\n",
					 input.temperature, output.heating_stage1);

		// Temperature reaches setpoint - heating off
		input.temperature = 68.0f;  // At setpoint, not 67.1
		input.now_seconds = 400;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		printf("  Fahrenheit: temp=%.1f°F (at setpoint): heating=%d " COLOR_GREEN "(OFF)" COLOR_RESET "\n",
					 input.temperature, output.heating_stage1);

		printf("\n" COLOR_GREEN "✓ Both temperature units work correctly" COLOR_RESET "\n");

		TEST_PASS(12, "Temperature Unit - Celsius vs Fahrenheit");
}

// ============================================================================
// TEST 13: Fan Pre-run and Post-run Timing
// ============================================================================

static void test_13_fan_timing(void) {
		TEST_START(13, "Fan Pre-run and Post-run Timing");

		printf(COLOR_YELLOW "Testing: Fan starts before and continues after heating/cooling\n" COLOR_RESET);
		printf("Pre-run ensures airflow is established before thermal element activates.\n");
		printf("Post-run distributes residual heat/cool and prevents temperature stratification.\n\n");

		ThermostatConfig config;
		thermostat_config_init(&config);
		config.heat_setpoint = 20.0f;
		config.fan_pre_run_sec = 30;   // 30 seconds pre-run
		config.fan_post_run_sec = 60;  // 60 seconds post-run
		config.hysteresis_period_sec = 0;
		config.min_cycle_time_sec = 0;
		config.cooling_enabled = false;

		const char* related[] = {"fan_enabled", "fan_pre_run_sec", "fan_post_run_sec",
														 "heat_setpoint", "heating_enabled"};
		print_related_config("Fan Timing", &config, related, 5);
		print_full_config(&config);

		ThermostatState state;
		ThermostatInput input = {0};
		ThermostatOutput output;

		thermostat_state_init(&state, 0);
		input.sensors_valid = true;
		input.now_seconds = 0;

		// Temperature below setpoint - thermal wants to activate
		input.temperature = 18.0f;
		thermostat_update(&config, &input, &state, &output);

		// Fan should start immediately, but heating not yet
		assert(output.fan);
		assert(!output.heating_stage1);
		assert(state.thermal_pending);
		printf("  t=%4us: fan=%d, heating=%d " COLOR_YELLOW "(fan pre-run started, thermal pending)" COLOR_RESET "\n",
					 input.now_seconds, output.fan, output.heating_stage1);

		// 15 seconds later - still in pre-run
		input.now_seconds = 15;
		thermostat_update(&config, &input, &state, &output);
		assert(output.fan);
		assert(!output.heating_stage1);
		printf("  t=%4us: fan=%d, heating=%d " COLOR_YELLOW "(still in pre-run period)" COLOR_RESET "\n",
					 input.now_seconds, output.fan, output.heating_stage1);

		// 30 seconds - pre-run complete, heating activates
		input.now_seconds = 30;
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		assert(output.fan);
		printf("  t=%4us: fan=%d, heating=%d " COLOR_GREEN "(pre-run complete, heating activated)" COLOR_RESET "\n",
					 input.now_seconds, output.fan, output.heating_stage1);

		// Heating runs for a while
		input.now_seconds = 180;
		thermostat_update(&config, &input, &state, &output);
		assert(output.fan);
		assert(output.heating_stage1);
		printf("  t=%4us: fan=%d, heating=%d " COLOR_GREEN "(normal operation)" COLOR_RESET "\n",
					 input.now_seconds, output.fan, output.heating_stage1);

		// Temperature reaches setpoint - heating stops
		input.temperature = 20.0f;
		input.now_seconds = 200;
		thermostat_update(&config, &input, &state, &output);
		assert(output.fan);  // Fan continues!
		assert(!output.heating_stage1);
		printf("  t=%4us: fan=%d, heating=%d " COLOR_YELLOW "(heating stopped, fan post-run started)" COLOR_RESET "\n",
					 input.now_seconds, output.fan, output.heating_stage1);

		// 30 seconds into post-run
		input.now_seconds = 230;
		thermostat_update(&config, &input, &state, &output);
		assert(output.fan);
		assert(!output.heating_stage1);
		printf("  t=%4us: fan=%d, heating=%d " COLOR_YELLOW "(still in post-run period)" COLOR_RESET "\n",
					 input.now_seconds, output.fan, output.heating_stage1);

		// 60 seconds - post-run complete, fan stops
		input.now_seconds = 260;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(!output.fan);
		printf("  t=%4us: fan=%d, heating=%d " COLOR_GREEN "(post-run complete, fan stopped)" COLOR_RESET "\n",
					 input.now_seconds, output.fan, output.heating_stage1);

		// Test with cooling
		printf("\n" COLOR_CYAN "Testing with cooling" COLOR_RESET "\n");
		config.cooling_enabled = true;
		config.heating_enabled = false;
		thermostat_state_init(&state, 300);
		input.now_seconds = 300;
		input.temperature = 26.0f;  // Above cool setpoint

		thermostat_update(&config, &input, &state, &output);
		assert(output.fan);
		assert(!output.cooling_stage1);
		printf("  t=%4us: fan=%d, cooling=%d " COLOR_YELLOW "(fan pre-run for cooling)" COLOR_RESET "\n",
					 input.now_seconds, output.fan, output.cooling_stage1);

		input.now_seconds = 330;
		thermostat_update(&config, &input, &state, &output);
		assert(output.fan);
		assert(output.cooling_stage1);
		printf("  t=%4us: fan=%d, cooling=%d " COLOR_GREEN "(cooling activated after pre-run)" COLOR_RESET "\n",
					 input.now_seconds, output.fan, output.cooling_stage1);

		TEST_PASS(13, "Fan Pre-run and Post-run Timing");
}


// ----------------------------------------------------------------------------
// TEST 14: Hysteresis with HVAC (fan enabled)
// ----------------------------------------------------------------------------

static void test_14_hysteresis_hvac(void) {
		TEST_START(14, "Hysteresis Period - HVAC Integration");

		printf(COLOR_YELLOW "Testing: Hysteresis with fan coordination in HVAC system\n" COLOR_RESET);
		printf("Verifies state change delays work correctly with fan pre-run/post-run.\n\n");

		ThermostatConfig config;
		thermostat_config_init(&config);
		config.heat_setpoint = 20.0f;
		config.comfort_deadband = 0.5f;
		config.hysteresis_period_sec = 300;
		config.min_cycle_time_sec = 180;
		config.fan_enabled = true;           // HVAC mode
		config.fan_pre_run_sec = 30;
		config.fan_post_run_sec = 60;
		config.cooling_enabled = false;

		const char* related[] = {"hysteresis_period_sec", "min_cycle_time_sec", "fan_enabled", "fan_pre_run_sec"};
		print_related_config("Hysteresis HVAC", &config, related, 4);
		print_full_config(&config);

		ThermostatState state;
		ThermostatInput input = {0};
		ThermostatOutput output;

		thermostat_state_init(&state, 0);
		input.sensors_valid = true;

		// Cold temp at t=0 - blocked by hysteresis
		input.temperature = 18.0f;
		input.now_seconds = 0;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(!output.fan);
		printf("  t=%4us, temp=%.1f°C: heat=%d, fan=%d " COLOR_YELLOW "(blocked by hysteresis)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.fan);

		// t=300 - hysteresis expires, fan starts (pre-run begins)
		input.now_seconds = 300;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(output.fan);
		printf("  t=%4us, temp=%.1f°C: heat=%d, fan=%d " COLOR_YELLOW "(hysteresis OK, fan pre-run started)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.fan);

		// t=330 - pre-run complete, heating activates
		input.now_seconds = 330;
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		assert(output.fan);
		printf("  t=%4us, temp=%.1f°C: heat=%d, fan=%d " COLOR_GREEN "(heating + fan ON)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.fan);

		// Temp rises, but min_cycle_time not met
		input.temperature = 20.0f;
		input.now_seconds = 400;  // Only 70s after heating started
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);  // Still on due to min_cycle_time
		printf("  t=%4us, temp=%.1f°C: heat=%d " COLOR_YELLOW "(setpoint reached, min_cycle_time not met)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1);

		// After min_cycle_time, heating stops, post-run begins
		input.now_seconds = 511;  // 181s after heating started
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(output.fan);  // Fan continues for post-run
		printf("  t=%4us, temp=%.1f°C: heat=%d, fan=%d " COLOR_YELLOW "(heating OFF, fan post-run)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.fan);

		// Post-run completes
		input.now_seconds = 571;  // 60s after heating stopped
		thermostat_update(&config, &input, &state, &output);
		assert(!output.fan);
		printf("  t=%4us: fan=%d " COLOR_GREEN "(post-run complete)" COLOR_RESET "\n",
					 input.now_seconds, output.fan);

		TEST_PASS(14, "Hysteresis Period - HVAC Integration");
}

// ----------------------------------------------------------------------------
// TEST 15: Comfort Deadband with HVAC
// ----------------------------------------------------------------------------

static void test_15_comfort_deadband_hvac(void) {
		TEST_START(15, "Comfort Deadband - HVAC Integration");

		printf(COLOR_YELLOW "Testing: Temperature thresholds with HVAC fan coordination\n" COLOR_RESET);
		printf("Verifies deadband logic works with fan pre-run delays.\n\n");

		ThermostatConfig config;
		thermostat_config_init(&config);
		config.comfort_mode = COMFORT_MODE_COMFORT;
		config.heat_setpoint = 20.0f;
		config.cool_setpoint = 24.0f;
		config.comfort_deadband = 0.5f;
		config.hysteresis_period_sec = 0;
		config.min_cycle_time_sec = 0;
		config.fan_enabled = true;
		config.fan_pre_run_sec = 30;
		config.fan_post_run_sec = 60;

		const char* related[] = {"comfort_deadband", "fan_enabled", "fan_pre_run_sec", "fan_post_run_sec"};
		print_related_config("Deadband HVAC", &config, related, 4);
		print_full_config(&config);

		ThermostatState state;
		ThermostatInput input = {0};
		ThermostatOutput output;

		thermostat_state_init(&state, 100);
		input.sensors_valid = true;
		input.now_seconds = 100;

		// Above heating threshold - no activation
		input.temperature = 19.6f;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		printf("  temp=%.1f°C (above 19.5°C threshold): heat=%d " COLOR_GREEN "(correctly OFF)" COLOR_RESET "\n",
					 input.temperature, output.heating_stage1);

		// Below threshold - fan starts (pre-run)
		input.temperature = 19.4f;
		input.now_seconds = 200;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(output.fan);
		printf("  temp=%.1f°C (below 19.5°C threshold): heat=%d, fan=%d " COLOR_YELLOW "(fan pre-run)" COLOR_RESET "\n",
					 input.temperature, output.heating_stage1, output.fan);

		// Pre-run complete - heating activates
		input.now_seconds = 230;
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		printf("  temp=%.1f°C: heat=%d " COLOR_GREEN "(heating ON after pre-run)" COLOR_RESET "\n",
					 input.temperature, output.heating_stage1);

		// Test cooling threshold similarly
		input.temperature = 24.4f;
		input.now_seconds = 300;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.cooling_stage1);
		printf("  temp=%.1f°C (below 24.5°C threshold): cool=%d " COLOR_GREEN "(correctly OFF)" COLOR_RESET "\n",
					 input.temperature, output.cooling_stage1);

		input.temperature = 24.6f;
		input.now_seconds = 400;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.cooling_stage1);
		assert(output.fan);
		printf("  temp=%.1f°C (above 24.5°C threshold): cool=%d, fan=%d " COLOR_YELLOW "(fan pre-run)" COLOR_RESET "\n",
					 input.temperature, output.cooling_stage1, output.fan);

		input.now_seconds = 430;
		thermostat_update(&config, &input, &state, &output);
		assert(output.cooling_stage1);
		printf("  temp=%.1f°C: cool=%d " COLOR_GREEN "(cooling ON after pre-run)" COLOR_RESET "\n",
					 input.temperature, output.cooling_stage1);

		TEST_PASS(15, "Comfort Deadband - HVAC Integration");
}

// ----------------------------------------------------------------------------
// TEST 16: Eco Deadband with HVAC
// ----------------------------------------------------------------------------

static void test_16_eco_deadband_hvac(void) {
		TEST_START(16, "Eco Deadband - HVAC Integration");

		printf(COLOR_YELLOW "Testing: Wider deadband in ECO mode with HVAC coordination\n" COLOR_RESET);
		printf("ECO mode reduces cycling frequency while maintaining fan safety.\n\n");

		ThermostatConfig config;
		thermostat_config_init(&config);
		config.comfort_mode = COMFORT_MODE_ECO;
		config.heat_setpoint = 20.0f;
		config.eco_deadband = 1.0f;  // Wider than comfort
		config.hysteresis_period_sec = 0;
		config.min_cycle_time_sec = 0;
		config.fan_enabled = true;
		config.fan_pre_run_sec = 30;
		config.fan_post_run_sec = 60;
		config.cooling_enabled = false;

		const char* related[] = {"comfort_mode", "eco_deadband", "fan_enabled", "fan_pre_run_sec"};
		print_related_config("Eco HVAC", &config, related, 4);
		print_full_config(&config);

		ThermostatState state;
		ThermostatInput input = {0};
		ThermostatOutput output;

		thermostat_state_init(&state, 100);
		input.sensors_valid = true;
		input.now_seconds = 100;

		// Heating threshold in ECO: 20.0 - 1.0 = 19.0°C
		input.temperature = 19.1f;  // Above threshold
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		printf("  ECO mode, temp=%.1f°C (above 19.0°C threshold): heat=%d " COLOR_GREEN "(OFF)" COLOR_RESET "\n",
					 input.temperature, output.heating_stage1);

		// Below ECO threshold
		input.temperature = 18.9f;
		input.now_seconds = 200;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(output.fan);
		printf("  ECO mode, temp=%.1f°C (below 19.0°C threshold): heat=%d, fan=%d " COLOR_YELLOW "(pre-run)" COLOR_RESET "\n",
					 input.temperature, output.heating_stage1, output.fan);

		input.now_seconds = 230;
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		printf("  ECO mode, temp=%.1f°C: heat=%d " COLOR_GREEN "(heating ON)" COLOR_RESET "\n",
					 input.temperature, output.heating_stage1);

		TEST_PASS(16, "Eco Deadband - HVAC Integration");
}

// ----------------------------------------------------------------------------
// TEST 17: Stage 2 Heating with HVAC
// ----------------------------------------------------------------------------

static void test_17_heat_stage2_hvac(void) {
		TEST_START(17, "Heat Stage 2 - HVAC Integration");

		printf(COLOR_YELLOW "Testing: Stage 2 heating with fan coordination\n" COLOR_RESET);
		printf("Verifies gradual power increase works with HVAC timing.\n\n");

		ThermostatConfig config;
		thermostat_config_init(&config);
		config.heating_stage2_enabled = true;
		config.heat_setpoint = 20.0f;
		config.comfort_deadband = 0.5f;
		config.heat_stage2_delay_sec = 300;
		config.hysteresis_period_sec = 0;
		config.min_cycle_time_sec = 0;
		config.fan_enabled = true;
		config.fan_pre_run_sec = 30;
		config.fan_post_run_sec = 60;
		config.cooling_enabled = false;

		const char* related[] = {"heating_stage2_enabled", "heat_stage2_delay_sec", "fan_enabled", "fan_pre_run_sec"};
		print_related_config("Stage 2 HVAC", &config, related, 4);
		print_full_config(&config);

		ThermostatState state;
		ThermostatInput input = {0};
		ThermostatOutput output;

		thermostat_state_init(&state, 100);
		input.sensors_valid = true;
		input.now_seconds = 100;

		// Cold temp - fan pre-run starts
		input.temperature = 18.0f;
		thermostat_update(&config, &input, &state, &output);
		assert(output.fan);
		printf("  t=%4us, temp=%.1f°C: cool=%d, fan=%d " COLOR_YELLOW "(pre-run)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.cooling_stage1, output.fan);

		// Stage 1 activates
		input.now_seconds = 130;
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		assert(!output.heating_stage2);
		printf("  t=%4us, temp=%.1f°C: stage1=%d, stage2=%d " COLOR_GREEN "(stage 1 ON)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.heating_stage2);

		// Before stage 2 delay
		input.now_seconds = 400;  // 270s after stage 1 started
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		assert(!output.heating_stage2);
		printf("  t=%4us: stage2=%d " COLOR_YELLOW "(delay not met)" COLOR_RESET "\n",
					 input.now_seconds, output.heating_stage2);

		// After stage 2 delay + large deviation
		input.now_seconds = 431;  // 301s after stage 1 started
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		assert(output.heating_stage2);
		printf("  t=%4us, temp=%.1f°C: stage1=%d, stage2=%d " COLOR_GREEN "(stage 2 ON)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.heating_stage2);

		TEST_PASS(17, "Heat Stage 2 - HVAC Integration");
}

// ----------------------------------------------------------------------------
// TEST 18: Stage 2 Cooling with HVAC
// ----------------------------------------------------------------------------

static void test_18_cool_stage2_hvac(void) {
		TEST_START(18, "Cool Stage 2 - HVAC Integration");

		printf(COLOR_YELLOW "Testing: Stage 2 cooling with fan coordination\n" COLOR_RESET);
		printf("Verifies gradual power increase works with HVAC timing.\n\n");

		ThermostatConfig config;
		thermostat_config_init(&config);
		config.cooling_stage2_enabled = true;
		config.cool_setpoint = 24.0f;
		config.comfort_deadband = 0.5f;
		config.cool_stage2_delay_sec = 300;
		config.hysteresis_period_sec = 0;
		config.min_cycle_time_sec = 0;
		config.fan_enabled = true;
		config.fan_pre_run_sec = 30;
		config.fan_post_run_sec = 60;
		config.heating_enabled = false;

		const char* related[] = {"cooling_stage2_enabled", "cool_stage2_delay_sec", "fan_enabled", "fan_pre_run_sec"};
		print_related_config("Stage 2 Cool HVAC", &config, related, 4);
		print_full_config(&config);

		ThermostatState state;
		ThermostatInput input = {0};
		ThermostatOutput output;

		thermostat_state_init(&state, 100);
		input.sensors_valid = true;
		input.now_seconds = 100;

		// Hot temp - fan pre-run
		input.temperature = 26.0f;
		thermostat_update(&config, &input, &state, &output);
		assert(output.fan);
		printf("  t=%4us, temp=%.1f°C: cool=%d, fan=%d " COLOR_YELLOW "(pre-run)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.cooling_stage1, output.fan);

		// Stage 1 activates
		input.now_seconds = 130;
		thermostat_update(&config, &input, &state, &output);
		assert(output.cooling_stage1);
		assert(!output.cooling_stage2);
		printf("  t=%4us, temp=%.1f°C: stage1=%d, stage2=%d " COLOR_GREEN "(stage 1 ON)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.cooling_stage1, output.cooling_stage2);

		// After delay + deviation
		input.now_seconds = 431;
		thermostat_update(&config, &input, &state, &output);
		assert(output.cooling_stage1);
		assert(output.cooling_stage2);
		printf("  t=%4us, temp=%.1f°C: stage1=%d, stage2=%d " COLOR_GREEN "(stage 2 ON)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.cooling_stage1, output.cooling_stage2);

		TEST_PASS(18, "Cool Stage 2 - HVAC Integration");
}

// ----------------------------------------------------------------------------
// TEST 19: Setpoint Mutual Exclusion with HVAC
// ----------------------------------------------------------------------------

static void test_19_mutual_exclusion_hvac(void) {
		TEST_START(19, "Mutual Exclusion - HVAC Integration");

		printf(COLOR_YELLOW "Testing: Heat/cool mutual exclusion with fan coordination\n" COLOR_RESET);
		printf("Verifies no simultaneous operation even with fan timing.\n\n");

		ThermostatConfig config;
		thermostat_config_init(&config);
		config.heat_setpoint = 20.0f;
		config.cool_setpoint = 24.0f;
		config.comfort_deadband = 0.5f;
		config.hysteresis_period_sec = 0;
		config.min_cycle_time_sec = 0;
		config.fan_enabled = true;
		config.fan_pre_run_sec = 30;
		config.fan_post_run_sec = 60;

		const char* related[] = {"heat_setpoint", "cool_setpoint", "fan_enabled", "fan_pre_run_sec", "fan_post_run_sec"};
		print_related_config("Mutual Exclusion HVAC", &config, related, 5);
		print_full_config(&config);

		ThermostatState state;
		ThermostatInput input = {0};
		ThermostatOutput output;

		thermostat_state_init(&state, 100);
		input.sensors_valid = true;
		input.now_seconds = 100;

		// === HEATING CYCLE ===
		// Cold - heating pre-run starts
		input.temperature = 18.0f;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(!output.cooling_stage1);
		assert(output.fan);
		printf("  t=%4us, temp=%.1f°C: heat=%d, cool=%d, fan=%d " COLOR_YELLOW "(heating pre-run)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.cooling_stage1, output.fan);

		// Heating activates after pre-run
		input.now_seconds = 130;
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		assert(!output.cooling_stage1);
		printf("  t=%4us, temp=%.1f°C: heat=%d, cool=%d " COLOR_GREEN "(heating active)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.cooling_stage1);

		// Temperature rises to neutral zone - heating stops
		input.temperature = 22.0f;
		input.now_seconds = 200;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(!output.cooling_stage1);
		assert(output.fan);  // Heating post-run
		printf("  t=%4us, temp=%.1f°C: heat=%d, cool=%d, fan=%d " COLOR_YELLOW "(heating stopped, post-run)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.cooling_stage1, output.fan);

		// Post-run completes
		input.now_seconds = 260;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(!output.cooling_stage1);
		assert(!output.fan);
		printf("  t=%4us: heat=%d, cool=%d, fan=%d " COLOR_GREEN "(idle, all OFF)" COLOR_RESET "\n",
					 input.now_seconds, output.heating_stage1, output.cooling_stage1, output.fan);

		// === COOLING CYCLE ===
		// Temperature rises to hot - cooling pre-run starts
		input.temperature = 26.0f;
		input.now_seconds = 300;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(!output.cooling_stage1);
		assert(output.fan);  // Cooling pre-run
		printf("  t=%4us, temp=%.1f°C: heat=%d, cool=%d, fan=%d " COLOR_YELLOW "(cooling pre-run)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.cooling_stage1, output.fan);

		// Cooling activates after pre-run
		input.now_seconds = 330;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(output.cooling_stage1);
		printf("  t=%4us, temp=%.1f°C: heat=%d, cool=%d " COLOR_GREEN "(cooling active)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.cooling_stage1);

		// Temperature drops to neutral - cooling stops
		input.temperature = 22.0f;
		input.now_seconds = 400;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(!output.cooling_stage1);
		assert(output.fan);  // Cooling post-run
		printf("  t=%4us, temp=%.1f°C: heat=%d, cool=%d, fan=%d " COLOR_YELLOW "(cooling stopped, post-run)" COLOR_RESET "\n",
					 input.now_seconds, input.temperature, output.heating_stage1, output.cooling_stage1, output.fan);

		// Final state - all off
		input.now_seconds = 460;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(!output.cooling_stage1);
		assert(!output.fan);
		printf("  t=%4us: heat=%d, cool=%d, fan=%d " COLOR_GREEN "(idle, all OFF)" COLOR_RESET "\n",
					 input.now_seconds, output.heating_stage1, output.cooling_stage1, output.fan);

		printf("\n" COLOR_GREEN "✓ Mutual exclusion verified throughout all phases" COLOR_RESET "\n");

		TEST_PASS(19, "Mutual Exclusion - HVAC Integration");
}

// ----------------------------------------------------------------------------
// TEST 20: Sensor Failure with HVAC
// ----------------------------------------------------------------------------

static void test_20_sensor_failure_hvac(void) {
		TEST_START(20, "Sensor Failure - HVAC Integration");

		printf(COLOR_YELLOW "Testing: Safety shutdown works correctly with fan timing\n" COLOR_RESET);
		printf("All outputs including fan must stop on sensor failure.\n\n");

		ThermostatConfig config;
		thermostat_config_init(&config);
		config.heat_setpoint = 20.0f;
		config.comfort_deadband = 0.5f;
		config.hysteresis_period_sec = 0;
		config.min_cycle_time_sec = 0;
		config.fan_enabled = true;
		config.fan_pre_run_sec = 30;
		config.fan_post_run_sec = 60;
		config.cooling_enabled = false;

		const char* related[] = {"sensors_valid", "fan_enabled", "fan_post_run_sec"};
		print_related_config("Sensor Failure HVAC", &config, related, 3);
		print_full_config(&config);

		ThermostatState state;
		ThermostatInput input = {0};
		ThermostatOutput output;

		thermostat_state_init(&state, 100);
		input.sensors_valid = true;
		input.now_seconds = 100;

		// Start heating with fan
		input.temperature = 18.0f;
		thermostat_update(&config, &input, &state, &output);
		input.now_seconds = 130;
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		assert(output.fan);
		printf("  sensors_valid=%d: heat=%d, fan=%d " COLOR_GREEN "(normal operation)" COLOR_RESET "\n",
					 input.sensors_valid, output.heating_stage1, output.fan);

		// Sensor fails - immediate shutdown (ignores post-run)
		input.sensors_valid = false;
		input.now_seconds = 200;
		thermostat_update(&config, &input, &state, &output);
		assert(!output.heating_stage1);
		assert(!output.fan);  // Fan must stop immediately, no post-run
		assert(!output.heating_stage2);
		assert(!output.cooling_stage1);
		assert(!output.cooling_stage2);
		printf("  sensors_valid=%d: all outputs=%d " COLOR_GREEN "(immediate safety shutdown)" COLOR_RESET "\n",
					 input.sensors_valid, 0);

		// Recovery
		input.sensors_valid = true;
		input.temperature = 18.0f;
		input.now_seconds = 300;
		thermostat_update(&config, &input, &state, &output);
		input.now_seconds = 330;
		thermostat_update(&config, &input, &state, &output);
		assert(output.heating_stage1);
		printf("  sensors_valid=%d: heat=%d " COLOR_GREEN "(recovery successful)" COLOR_RESET "\n",
					 input.sensors_valid, output.heating_stage1);

		TEST_PASS(20, "Sensor Failure - HVAC Integration");
}

// ============================================================================
// SIMULATION SCENARIO
// ============================================================================

static void simulate_scenario(void) {
		printf("\n" COLOR_MAGENTA "========================================" COLOR_RESET "\n");
		printf(COLOR_MAGENTA "SIMULATION: 24-Hour Heating Cycle" COLOR_RESET "\n");
		printf(COLOR_MAGENTA "========================================" COLOR_RESET "\n\n");

		ThermostatConfig config;
		thermostat_config_init(&config);
		config.heat_setpoint = 20.0f;
		config.cooling_enabled = false;
		config.min_cycle_time_sec = 180;
		config.fan_pre_run_sec = 30;
		config.fan_post_run_sec = 60;

		ThermostatState state;
		ThermostatInput input = {0};
		ThermostatOutput output;

		thermostat_state_init(&state, 0);
		input.sensors_valid = true;

		// Simulate temperature changes over 24 hours
		float temperatures[] = {
				18.0f, 18.5f, 19.0f, 19.5f, 20.0f, 20.2f, 20.0f, 19.8f,
				19.5f, 19.0f, 18.5f, 18.0f, 17.5f, 17.0f, 17.5f, 18.0f,
				18.5f, 19.0f, 19.5f, 20.0f, 20.2f, 20.0f, 19.8f, 19.5f
		};

		for (int hour = 0; hour < 24; hour++) {
				input.temperature = temperatures[hour];
				input.now_seconds = hour * 3600;

				thermostat_update(&config, &input, &state, &output);

				const char* state_str = "IDLE";
				if (output.heating_stage2) state_str = "HEAT-2";
				else if (output.heating_stage1) state_str = "HEAT-1";

				printf("  Hour %2d: Temp=%.1f°C, State=%s, Fan=%d\n",
							 hour, input.temperature, state_str, output.fan);
		}

		printf("\n" COLOR_GREEN "Simulation complete!" COLOR_RESET "\n");
}

// ============================================================================
// TEST SUMMARY
// ============================================================================

static void print_test_summary(void) {
		printf("\n" COLOR_CYAN "========================================" COLOR_RESET "\n");
		printf(COLOR_CYAN "TEST SUMMARY" COLOR_RESET "\n");
		printf(COLOR_CYAN "========================================" COLOR_RESET "\n\n");

		printf("%-4s %-50s %s\n", "No.", "Test Name", "Result");
		printf("---- -------------------------------------------------- ------\n");

		for (int i = 0; i < total_tests; i++) {
				const char* result = test_results[i].passed ? COLOR_GREEN "PASS" COLOR_RESET
																										 : COLOR_RED "FAIL" COLOR_RESET;
				printf("%-4d %-50s %s\n", i + 1, test_results[i].name, result);
		}

		printf("\n");
		printf("Total Tests: %d\n", total_tests);
		printf("Passed:      " COLOR_GREEN "%d" COLOR_RESET "\n", passed_tests);
		printf("Failed:      " COLOR_RED "%d" COLOR_RESET "\n", total_tests - passed_tests);
		printf("Success Rate: %.2f%%\n", (float)passed_tests / total_tests * 100.0f);

		if (passed_tests == total_tests) {
				printf("\n" COLOR_GREEN "✓ ALL TESTS PASSED!" COLOR_RESET "\n\n");
		} else {
				printf("\n" COLOR_RED "✗ SOME TESTS FAILED" COLOR_RESET "\n\n");
		}
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
		printf(COLOR_CYAN "========================================" COLOR_RESET "\n");
		printf(COLOR_CYAN "Thermostat Core Test Suite" COLOR_RESET "\n");
		printf(COLOR_CYAN "========================================" COLOR_RESET "\n");

		test_1_config_validation();
		test_2_hysteresis_period();
		test_3_comfort_deadband();
		test_4_eco_deadband();
		test_5_heat_stage2_delay();
		test_6_cool_stage2_delay();
		test_7_setpoint_mutual_exclusion();
		test_8_fan_control();
		test_9_humidity_control();
		test_10_hot_water();
		test_11_sensor_failure();
		test_12_temperature_unit();
		test_13_fan_timing();
		test_14_hysteresis_hvac();
		test_15_comfort_deadband_hvac();
		test_16_eco_deadband_hvac();
		test_17_heat_stage2_hvac();
		test_18_cool_stage2_hvac();
		test_19_mutual_exclusion_hvac();
		test_20_sensor_failure_hvac();

		simulate_scenario();
		print_test_summary();

		return (passed_tests == total_tests) ? 0 : 1;
}