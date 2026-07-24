# Hestia32 Settings Enhancement - Issue Tracking

## Status Legend
- ✅ **DONE** - Implemented and verified
- 🔄 **IN PROGRESS** - Currently being worked on
- ⏸️ **PENDING** - Not yet started
- 🐛 **BUG** - Critical bug requiring immediate fix

---

## Critical Bugs

### 🐛 #12: Boot Flow Broken
**Name:** WiFi Provisioning Loop
**Status:** ✅ DONE
**Description:** Device unconditionally starts WiFi provisioning screen even when already provisioned. Factory reset also blocked during this state.
**Root Cause:** Boot flow logic missing WiFi provisioning check. Code structure corrupted during earlier edits.
**Fix:** Added `if (!wifi_prov_is_provisioned())` check before starting provisioning mode. Now only provisions when needed.
**Files Modified:** `src/main.c` (lines 305-330)

### ✅ #16: SHT45 Initialization and Error Handling
**Name:** SHT45/Relay Manager
**Status:** ✅ FIXED v4.1
**Description:** Series of cascading issues:
1. I2C driver install errors
2. Relay manager not initialized errors
3. "SHT45 not initialized" error flooding

**Root Causes:**
- **Phase 1 (I2C conflict):** Both TCA9555 and SHT45 tried to install I2C driver on same port → ESP_ERR_INVALID_STATE
- **Phase 2 (Missing init):** relay_manager_init() was never called in boot sequence
- **Phase 3 (Rollback damage):** Git rollback to fix compilation errors removed sensor_available flag, causing unconditional sht45_read() calls

**Fixes:**
- **v3:** SHT45 handles ESP_ERR_INVALID_STATE gracefully when I2C driver already installed
- **v4:** Added relay_manager_init() before SHT45 initialization
- **v4.1:** Re-added sensor_available flag to prevent reads when sensor init fails

**Files Modified:**
- `src/core/sensor_sht45.c` (lines 36-90: handle shared I2C driver)
- `src/main.c` (lines 253-261: relay manager init; lines 272-277: sensor_available flag; lines 299-301: conditional sensor read)

**Expected Logs:**
```
Initializing relay manager...
Relay manager initialized successfully
Initializing SHT45 sensor...
Using existing I2C driver (already initialized)
SHT45 sensor initialized successfully
```
**Name:** NTP Crash
**Status:** ✅ DONE
**Description:** Device crashes on boot with "assert failed: tcpip_callback" when WiFi is already provisioned. NTP initialization tries to use TCP/IP stack before it's ready.
**Root Cause:** `esp_sntp_setoperatingmode()` called at boot before WiFi/network stack initialized. This is a manifestation of issue #14.
**Fix:** Commented out NTP initialization calls (esp_sntp_setoperatingmode, esp_sntp_setservername, esp_sntp_init) until proper WiFi-aware init is implemented.
**Files Modified:** `src/main.c` (lines 365-369)
**Related:** Issue #14 will properly implement NTP init in WiFi connected event handler.

---

## UI/UX Enhancements - Locale Settings

### ✅ #1: Temperature Unit Label Visibility
**Name:** C Label Missing
**Status:** 🔧 FIXED (needs testing)
**Description:** "C" label touching the toggle switch while F has healthy margin.
**Fix:** Increased C label spacing from -106px to -120px (14px more margin from switch).
**Files Modified:** `src/core/ui/ui_settings_locale.cpp` (line 105)

### ✅ #2: Temperature Switch Color Behavior
**Name:** Switch Color
**Status:** ✅ VERIFIED FIXED (by user)
**Description:** In dark theme, switch becomes grey in C position, blue in F position.
**Root Cause:** LVGL switch widget uses grey color for unchecked indicator by default, making it hard to see in dark theme.
**Fix:** Added explicit background styling for LV_PART_MAIN (track background) using surface_variant color, and styled indicator for both checked and unchecked states.
**Files Modified:** `src/core/ui/ui_settings_locale.cpp` (lines 98-106)
**Name:** Switch Color
**Status:** 🔧 FIXED v3 (needs testing)
**Description:** In dark theme, switch becomes grey in C position, blue in F position.
**Previous Fix v2 Failed:** Only styled indicator part, but switch main background still had grey/blue default behavior.
**New Fix v3:** Explicitly style both indicator states and knob:
- Indicator (checked/F): Primary color
- Indicator (unchecked/C): Text secondary color (visible in dark theme)
- Knob: Dark grey (0x404040) - visible on both backgrounds
**Files Modified:** `src/core/ui/ui_settings_locale.cpp` (lines 95-98: added state-specific styling)
**Expected:** Both C and F positions clearly visible in dark theme

### ✅ #3: Real-Time Temperature Updates
**Name:** Temp Unit Callback
**Status:** 🔧 FIXED (needs testing)
**Description:** Units in current temp and Settings>Thermostat do not update when temperature unit is changed.
**Fix:** Added extensive debug logging throughout the refresh chain to diagnose the issue. Logs show unit conversion, cached data state, and all display updates.
**Files Modified:**
- `src/core/ui/ui_settings_locale.cpp` (lines 29-31: added debug logging)
- `src/core/ui/ui_main.cpp` (lines 1018-1067: added debug logging throughout refresh function)

### ✅ #4: Model-Based Temperature Defaults
**Name:** Model Temp Default
**Status:** 🔧 FIXED v3 (needs testing)
**Description:** Changing to HVAC shows Fahrenheit in settings UI, but main UI displays Celsius after reboot.
**Previous Fix v2 Failed:** 100ms delay wasn't enough or NVS read on boot had issues.
**New Fix v3:** Extended delay to 200ms, added read-back verification before reboot, and added boot-time logging to track temp unit value:
- Save temp unit to NVS
- Delay 200ms for NVS commit
- Read back and verify value matches
- Log verification result before reboot
- On next boot, log what temp unit was loaded from NVS
**Files Modified:**
- `src/core/ui/ui_settings_model.cpp` (lines 36-56: added verification and logging)
- `src/main.c` (lines 379-383: added boot-time temp unit logging)
**Expected Logs:**
```
Model changed to HVAC, setting temp unit to Fahrenheit (result: ESP_OK)
Verification: temp unit is now Fahrenheit (expected Fahrenheit)
Rebooting to apply model change...
[after reboot]
Temperature unit at boot: Fahrenheit
```
---

## UI/UX Enhancements - System Settings

### ⏸️ #5: Touch Pointer Visual Improvements
**Name:** Pointer Visuals
**Status:** ⏸️ PENDING
**Description:** Touch debug pointer currently disabled. Need themed appearance: 10px size, 70% opacity, themed color, 1px border.
**Implementation Plan:** Modify `display_manager.cpp` touch cursor creation (line 728), use theme primary/accent color, size 10px, LV_OPA_70, add border.
**Files To Modify:** `src/core/display_manager.cpp`

### ⏸️ #6: Touch Recalibration Button
**Name:** Recalibration Button
**Status:** ⏸️ PENDING
**Description:** Button exists in System settings but shows "coming soon" message. Need to wire to calibration wizard.
**Implementation Plan:** Call existing `run_touch_calibration()` function or create wrapper `display_manager_recalibrate()`.
**Files To Modify:**
- `src/core/ui/ui_settings_system.cpp` (on_recalibrate function)
- `src/core/display_manager.cpp` (expose calibration function)

### ⏸️ #7: OTA Install Implementation
**Name:** OTA Install
**Status:** ⏸️ PENDING
**Description:** OTA check works (queries GitHub API), but install button is stub.
**Implementation Plan:** Implement OTA download and flash sequence using `esp_https_ota` API.
**Files To Modify:** `src/core/ui/ui_settings_system.cpp`

---

## WiFi & Connectivity Enhancements

### ⏸️ #8: WiFi Standalone Provisioning
**Name:** WiFi Standalone
**Status:** ⏸️ PENDING
**Description:** WiFi provisioning currently coupled with calibration wizard. Should be accessible independently from Settings.
**Implementation Plan:** Add WiFi provisioning UI accessible from Connectivity settings, allow re-provisioning without factory reset.
**Files To Create/Modify:** New WiFi settings UI

### ⏸️ #9: WiFi Skip Button (First Boot)
**Name:** WiFi Skip
**Status:** ⏸️ PENDING
**Description:** On first boot, user should be able to skip WiFi provisioning to configure device locally first.
**Implementation Plan:** Add "Skip" button to provisioning UI that saves a flag and continues to main UI without WiFi.
**Files To Modify:** `src/core/ui/ui_provisioning.cpp`

---

## Time & NTP Enhancements

### ⏸️ #10: Show/Hide Time Toggle
**Name:** Time Visibility
**Status:** ⏸️ PENDING
**Description:** Time display should have show/hide toggle in Time settings.
**Implementation Plan:** Add toggle in time settings UI that controls visibility of time display in main UI. `device_config` already has `show_time` field.
**Files To Modify:**
- Time settings UI (add toggle)
- `src/core/ui/ui_main.cpp` (honor show_time flag)

### ⏸️ #11: Manual Time Entry
**Name:** Manual Time
**Status:** ⏸️ PENDING
**Description:** When NTP is disabled, user should be able to set time manually.
**Implementation Plan:** Create time picker UI (hour/minute spinners) that sets system time via `settimeofday()`.
**Files To Create/Modify:** New manual time entry UI

### ⏸️ #13: First-Boot NTP Prompt
**Name:** NTP Prompt
**Status:** ⏸️ PENDING
**Description:** On first boot, prompt user for NTP enable/disable, defaulting to "No".
**Implementation Plan:** Add NTP prompt screen during first boot sequence before main UI, save preference to NVS.
**Files To Create/Modify:** New first-boot NTP prompt UI

### ⏸️ #14: NTP WiFi-Aware Initialization
**Name:** NTP WiFi Check
**Status:** 🔄 IN PROGRESS (partially fixed with #15)
**Description:** NTP currently initializ3 fixes for #2, #4, #16)
**Binary Size:** 1,596,016 bytes (1.60 MB)
**Free Space:** 16% (303,504 bytes)
**Flash Command:** `idf.py -p /dev/ttyACM0 flash monitor`

**Recent Changes:**
- Switch visibility: Styled indicator states + knob color for dark theme
- HVAC temp unit: Added read-back verification and boot logging
- I2C driver: Handle ESP_ERR_INVALID_STATE when driver already installed by TCA9555
## Build Status

**Last Successful Build:** 2026-07-17 (v2 fixes for #2, #4, #16)
**Binary Size:** 1,596,336 bytes (1.60 MB)
**Free Space:** 16% (303,184 bytes)
**Flash Command:** `idf.py -p /dev/ttyACM0 flash monitor`

**Recent Changes:**
- Fixed switch visibility by removing main background override (use LVGL defaults)
- Fixed HVAC temp unit with 100ms delay + logging after NVS write
- Fixed SHT45 spam by using init result instead of sht45_is_available() probe

---

## Summary

**Total Issues:** 16
**Completed:** 2 (#12, #15)
**Fixed (needs testing):** 5 (#1, #2, #3, #4, #16)
**Pending:** 9
**Critical Bugs:** 3 (all fixed)

**Next Priority:** Test current fixes → #5 Touch Pointer Visuals → #6 Recalibration Button → #10 Time Visibility
