# Hestia32 Firmware - Functional Specification Document (FSD)

**Document Version:** 1.0
**Firmware Version:** 1.0.0
**Date:** January 14, 2026

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Project Overview](#2-project-overview)
3. [System Architecture](#3-system-architecture)
4. [Hardware Requirements](#4-hardware-requirements)
5. [Functional Requirements](#5-functional-requirements)
6. [Module Specifications](#6-module-specifications)
7. [User Interface](#7-user-interface)
8. [Data Management](#8-data-management)
9. [Security Considerations](#9-security-considerations)
10. [Development Environment](#10-development-environment)
11. [Build and Deployment](#11-build-and-deployment)
12. [Testing Strategy](#12-testing-strategy)
13. [Future Roadmap](#13-future-roadmap)
14. [Appendices](#14-appendices)

---

## 1. Executive Summary

Hestia32 is an ESP32-based firmware platform designed for IoT applications with a focus on HVAC/thermostat control and user interaction through a graphical touchscreen display. The system features WiFi provisioning without hardcoded credentials, Over-The-Air (OTA) firmware updates and a modern LVGL-based user interface on a 3.5" ILI9488 display with resistive touch support.

**Key Features:**
- Web-based WiFi provisioning (captive portal)
- Secure OTA firmware updates with dual partition support
- LVGL-driven graphical UI with touch calibration
- Advanced multi-stage thermostat control logic (planned)
- Factory reset capability via BOOT button
- NVS-based persistent storage for credentials and calibration
- Support for ESP32 (original) and ESP32-C5 (RISC-V)

---

## 2. Project Overview

### 2.1 Purpose

Hestia32 provides a complete firmware solution for ESP32-based smart home devices, specifically targeting thermostat and HVAC control applications. The firmware abstracts common IoT requirements (WiFi management, OTA updates, display handling) into reusable modules while providing a robust thermostat control engine.

### 2.2 Target Applications

- Smart thermostats
- HVAC control systems
- Multi-stage heating/cooling systems
- Humidity control systems
- General-purpose ESP32 IoT devices with display

### 2.3 Supported Hardware Platforms

| Platform | Architecture | Status | Build System |
|----------|-------------|--------|--------------|
| ESP32 (NodeMCU-32S) | Xtensa LX6 | Supported | PlatformIO |
| ESP32-C5 | RISC-V | Supported | ESP-IDF v5.5 |

### 2.4 Design Philosophy

- **Modularity:** Independent functional modules (WiFi, OTA, Display, Thermostat)
- **Zero Configuration:** No hardcoded WiFi credentials; provision via web interface
- **Safety First:** Sensor failure detection, mutual exclusion in thermostat logic
- **User Experience:** Touch calibration wizard, responsive UI, clear feedback
- **Maintainability:** Clean code structure, comprehensive test coverage

---

## 3. System Architecture

### 3.1 High-Level Architecture

```
┌────────────────────────────────────────────────────────┐
│                     Application Layer                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   Main App   │  │  Display UI  │  │  Thermostat  │  │
│  │   (main.c)   │  │              │  │              │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└────────────────────────────────────────────────────────┘
                           │
┌────────────────────────────────────────────────────────┐
│                   Service Layer                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ WiFi Manager │  │ OTA Manager  │  │   Display    │  │
│  │              │  │              │  │   Manager    │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│  ┌──────────────────────────────────────────────────┐  │
│  │         WiFi Provisioning Module                 │  │
│  │  (Web Server + Captive Portal + DNS Server)      │  │
│  └──────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────┘
                           │
┌────────────────────────────────────────────────────────┐
│                    Hardware Abstraction Layer          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   ESP WiFi   │  │  SPI Master  │  │   GPIO/PWM   │  │
│  │     Stack    │  │              │  │              │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │  LCD Driver  │  │Touch Driver  │  │    NVS       │  │
│  │  (ILI9488)   │  │  (XPT2046)   │  │   Storage    │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└────────────────────────────────────────────────────────┘
                           │
┌────────────────────────────────────────────────────────┐
│                    FreeRTOS + ESP-IDF                  │
│              Task Scheduling | Memory Management       │
└────────────────────────────────────────────────────────┘
```

### 3.2 Boot Sequence

```
1. Power On / Reset
   └─> NVS Initialization
       └─> Factory Reset Check (BOOT button held?)
           ├─> Yes: Erase NVS → Restart
           └─> No: Continue

2. Touch Calibration Check
   └─> Has calibration data in NVS?
       ├─> No: Run calibration wizard → Save to NVS → Restart
       └─> Yes: Load calibration

3. WiFi Provisioning Check
   └─> Is device provisioned?
       ├─> No: Start AP mode → Launch web server → Wait for config
       └─> Yes: Continue

4. Display Initialization
   └─> Initialize SPI + ILI9488 + XPT2046
       └─> Start LVGL task
           └─> Create main UI

5. WiFi Connection
   └─> Load credentials from NVS
       └─> Connect to WiFi (5 retries)

6. OTA Initialization
   └─> Verify boot partition
       └─> Mark as valid if pending

7. Main Loop
   └─> Application logic (LVGL task handles UI updates)
```

### 3.3 Task Priority Structure

| Task | Priority | Purpose | Core Affinity |
|------|----------|---------|---------------|
| WiFi/Network Tasks | 20-23 | ESP-IDF WiFi stack (system) | Any |
| DNS Server | 5 | Captive portal DNS resolution | Any |
| LVGL Task | 3 | UI rendering and touch handling | Any |
| Display Provisioning | 2 | Low-priority UI during setup | Any |
| Main Task | 1 | Initialization, then suspended | Any |

---

## 4. Hardware Requirements

### 4.1 ESP32-C5 Configuration

**Microcontroller:**
- ESP32-C5 (RISC-V 32-bit, single core)
- Flash: 4MB minimum (dual OTA partitions)
- RAM: 512KB SRAM

**Display Module:**
- Controller: ILI9488
- Size: 3.5 inches
- Resolution: 320×480 pixels (portrait)
- Interface: SPI (write-only, 40MHz)
- Panel Type: IPS
- Backlight: PWM-controlled via GPIO 13

**Touch Controller:**
- Type: XPT2046 (resistive)
- Interface: SPI (shared bus, 1MHz)
- Pressure sensing: Active low IRQ (GPIO 15)

### 4.2 Pin Mapping (ESP32-C5)

| Function | GPIO | Notes |
|----------|------|-------|
| **Display SPI** |||
| MOSI (SDI) | 7 | Shared with touch |
| SCLK | 6 | Shared with touch |
| CS | 10 | Display chip select |
| DC (RS) | 8 | Data/Command select |
| RESET | 9 | Hardware reset |
| Backlight | 13 | PWM capable |
| **Touch SPI** |||
| T_CS | 14 | Touch chip select |
| T_DIN | 7 | Shared MOSI |
| T_CLK | 6 | Shared SCLK |
| T_DO (MISO) | 2 | **Dedicated** (display MISO disconnected) |
| T_PEN (IRQ) | 15 | Active low pen detect |
| **System** |||
| BOOT Button | 28 | Factory reset (hold 5s on boot) |
| UART0 TX | 11 | **Reserved** (do not use) |
| UART0 RX | 12 | **Reserved** (do not use) |

**Critical Notes:**
- Display SDO/MISO **must remain disconnected**; GPIO 2 is dedicated to touch controller
- GPIO 11/12 reserved for UART0 (serial console)
- SPI2_HOST used for both display and touch (different CS pins)

### 4.3 Power Requirements

- Supply voltage: 3.3V regulated
- Typical current: 150-200mA (backlight on)
- Backlight off: 20-30mA
- Recommended: 100µF capacitor near display VCC/GND

### 4.4 ESP32 (Original) Configuration

For NodeMCU-32S boards using PlatformIO, pin assignments differ. Refer to `display_config.h` for specific GPIO mappings. The BOOT button is typically GPIO 0.

---

## 5. Functional Requirements

### 5.1 FR-001: WiFi Provisioning

**Priority:** Critical
**Status:** Implemented

**Description:** Device shall support web-based WiFi configuration without requiring hardcoded credentials.

**Acceptance Criteria:**
1. On first boot (no saved credentials), device creates open AP with SSID "HESTIA32"
2. Captive portal redirects all HTTP requests to 192.168.4.1
3. Web interface displays WiFi network scan results
4. User can input: WiFi SSID, password, device name, OTA server URL
5. Configuration saved to NVS persistent storage
6. Device restarts and connects to configured WiFi automatically

**Dependencies:** NVS, ESP WiFi stack, HTTP server

---

### 5.2 FR-002: Factory Reset

**Priority:** High
**Status:** Implemented

**Description:** User shall be able to erase all stored settings and return device to provisioning mode.

**Acceptance Criteria:**
1. Hold BOOT button during power-on/reset
2. Continue holding for 5 seconds
3. System erases NVS (WiFi credentials + touch calibration)
4. Device restarts in provisioning mode
5. Serial console logs "Factory reset confirmed!"

**Dependencies:** GPIO, NVS

---

### 5.3 FR-003: OTA Firmware Updates

**Priority:** Critical
**Status:** Implemented (update mechanism; version check optional)

**Description:** Device shall support secure over-the-air firmware updates with rollback protection.

**Acceptance Criteria:**
1. Dual OTA partition support (ota_0, ota_1)
2. HTTPS download from configurable URL
3. Automatic rollback on boot failure (ESP-IDF built-in)
4. Mark successful boot as valid to prevent rollback
5. Optional: Version comparison before download

**Dependencies:** ESP HTTPS OTA library, partition table

---

### 5.4 FR-004: Display and Touch Interface

**Priority:** Critical
**Status:** Implemented

**Description:** Device shall provide graphical user interface with calibrated resistive touch input.

**Acceptance Criteria:**
1. Initialize ILI9488 display (320×480 @ 40MHz SPI)
2. Initialize XPT2046 touch controller (1MHz SPI)
3. Run automatic calibration wizard on first boot
4. User touches 5 crosshairs (corners + center) within 60 seconds
5. Calibration data saved to NVS
6. Touch input mapped correctly to screen coordinates
7. LVGL renders UI at 25+ FPS

**Dependencies:** SPI, LVGL, NVS

---

### 5.5 FR-005: Touch Calibration

**Priority:** High
**Status:** Implemented

**Description:** System shall automatically detect axis orientation and mapping through user calibration.

**Acceptance Criteria:**
1. Calibration wizard displays 5 targets (4 corners + center)
2. 60-second overall timeout; 15 seconds per target
3. System detects axis swap, X flip, Y flip automatically
4. Calibration margins (5%) added to prevent edge clipping
5. If timeout occurs, use default calibration values
6. Calibration data: x_min, x_max, y_min, y_max, swap_xy, flip_x, flip_y

**Dependencies:** Touch hardware, NVS

---

### 5.6 FR-006: Thermostat Control (Planned)

**Priority:** High
**Status:** Not Integrated (module complete, not used in main.c)

**Description:** Device shall provide multi-stage heating/cooling control with advanced features.

**Acceptance Criteria:**
1. Support up to 4 simultaneous domains (heating, cooling, fan, humidity, hot water)
2. Multi-stage heating/cooling with configurable delays
3. Comfort/Eco modes with different deadbands
4. Open window detection (rapid temperature drop suspends heating)
5. Hysteresis protection (minimum 5 minutes between state changes)
6. Minimum cycle time (3 minutes) to protect equipment
7. Fan pre-run (30s before thermal) and post-run (60s after)
8. Mutual exclusion: heating and cooling never active simultaneously
9. Emergency shutdown on sensor failure

**Dependencies:** Temperature/humidity sensors (future hardware)

---

### 5.7 FR-007: Persistent Configuration Storage

**Priority:** Critical
**Status:** Implemented

**Description:** All user settings shall persist across power cycles and firmware updates.

**Acceptance Criteria:**
1. WiFi credentials stored in NVS namespace "wifi_config"
2. Touch calibration stored in NVS namespace "touch_cal"
3. Factory reset erases all NVS data
4. NVS partition: 24KB (0x6000 bytes)

**Dependencies:** NVS Flash API

---

## 6. Module Specifications

### 6.1 WiFi Provisioning Module

**Files:** `wifi_provisioning.c/h`, `provisioning_html.h`

**Responsibilities:**
- Create WiFi AP (SSID: "HESTIA32", open network)
- Run HTTP server on 192.168.4.1
- Implement captive portal DNS server (port 53)
- Serve HTML configuration form
- Handle WiFi network scanning
- Parse and save user configuration to NVS
- Restart device after successful configuration

**APIs:**
```c
void wifi_prov_init(void);
bool wifi_prov_is_provisioned(void);
void wifi_prov_start_ap(void);
esp_err_t wifi_prov_get_config(wifi_config_data_t *config);
void wifi_prov_reset(void);
```

**Configuration Structure:**
```c
typedef struct {
    char ssid[32];
    char password[64];
    char node_name[32];
    char ota_url[128];
} wifi_config_data_t;
```

**HTTP Endpoints:**
- `GET /` - Configuration form
- `GET /scan` - WiFi network scan (JSON)
- `POST /save` - Save configuration
- `GET /detect` - Captive portal redirect

**DNS Server:**
- Listens on UDP port 53
- Responds to all queries with 192.168.4.1
- Enables captive portal on mobile devices

---

### 6.2 WiFi Manager Module

**Files:** `wifi_manager.c/h`

**Responsibilities:**
- Initialize ESP WiFi stack (STA mode)
- Connect to configured WiFi network
- Implement retry logic (default 5 retries)
- Provide connection status

**APIs:**
```c
esp_err_t wifi_init(void);
esp_err_t wifi_connect(const char *ssid, const char *password, int max_retry);
void wifi_disconnect(void);
```

**Event Handling:**
- `WIFI_EVENT_STA_START` → Initiate connection
- `WIFI_EVENT_STA_DISCONNECTED` → Retry logic
- `IP_EVENT_STA_GOT_IP` → Connection successful

---

### 6.3 OTA Manager Module

**Files:** `ota_manager.c/h`

**Responsibilities:**
- Verify current boot partition
- Mark valid firmware after successful boot
- Download and install OTA updates via HTTPS
- Automatic rollback on failure

**APIs:**
```c
esp_err_t ota_init(void);
esp_err_t ota_update_from_url(const char *url);
const char* ota_get_version(void);
esp_err_t ota_check_and_update(const char *url);
```

**Partition Table:**
```
nvs       | 0x9000  | 24KB   | Configuration storage
otadata   | 0xF000  | 8KB    | OTA boot state
phy_init  | 0x11000 | 4KB    | PHY calibration
ota_0     | 0x20000 | 1.8MB  | Primary firmware
ota_1     | 0x1F0000| 1.8MB  | Secondary firmware
```

**Update Flow:**
1. Download firmware from URL
2. Write to inactive partition (ota_0 or ota_1)
3. Mark new partition as boot target
4. Restart device
5. On first boot: validate partition → mark as good OR rollback

---

### 6.4 Display Manager Module

**Files:** `display_manager.cpp/h`, `display_config.h`

**Responsibilities:**
- Initialize ILI9488 LCD via SPI (write-only, 40MHz)
- Initialize XPT2046 touch controller via SPI (1MHz)
- Configure PWM backlight control
- Initialize LVGL graphics library
- Implement LVGL flush callback (RGB565 → RGB666 conversion)
- Implement touch input callback with calibration mapping
- Run calibration wizard on first boot
- Save/load calibration from NVS

**APIs:**
```c
esp_err_t display_init(void);
bool display_has_calibration(void);
void display_create_ui(bool skip_calibration, bool provisioning_mode);
void display_start_lvgl_task(void);
void display_update(void);  // Legacy, deprecated
void display_clear_screen(void);
```

**LVGL Configuration:**
- Double buffering (80 line height each)
- RGB565 color mode
- SPI DMA transfer
- Tick period: 2ms
- Target: 25+ FPS

**Touch Calibration Process:**
1. Display 5 crosshair targets sequentially
2. User touches each target
3. Average 8 samples per touch
4. Calculate axis mapping from corner relationships:
   - Compare horizontal/vertical movement to determine if raw X/Y need swapping
   - Detect if X or Y axis needs flipping
5. Save min/max ranges with 5% margin
6. Store to NVS: x_min, x_max, y_min, y_max, swap_xy, flip_x, flip_y

**Touch Input Flow:**
```
XPT2046 raw (0-4095) → Apply calibration clamp → Map to 0-319 / 0-479
→ Apply axis swap (if needed) → Apply flips (if needed) → LVGL coordinates
```

---

### 6.5 Display UI Module

**Files:** `display_ui.cpp/h`

**Responsibilities:**
- Create provisioning mode UI (simple text instructions)
- Create main UI (colorful tile grid, animated bar, FPS counter)
- Manage touch indicator dot

**APIs:**
```c
void display_ui_create_provisioning(lv_obj_t *scr);
void display_ui_create_main(lv_obj_t *scr, lv_obj_t **touch_dot,
                           volatile uint32_t *flush_count);
```

**Provisioning UI:**
- Black background
- White text with connection instructions
- SSID: "HESTIA32"
- URL: "192.168.4.1"

**Main UI:**
- 4×6 grid of colorful rounded tiles (8 colors cycled)
- Animated white bar sliding horizontally (6-second loop)
- FPS counter (top-left corner)
- Touch indicator dot (16×16 magenta circle, follows touch)

---

### 6.6 Thermostat Module (Future Integration)

**Files:** `thermostat.c/h`

**Status:** Module complete, comprehensive tests pass, not integrated into main.c

**Responsibilities:**
- Multi-domain HVAC control (heating, cooling, fan, humidity, hot water)
- Multi-stage operation with time delays
- Hysteresis and minimum cycle time protection
- Fan coordination (pre-run/post-run)
- Open window detection and heating suspension
- Sensor failure safety
- Temperature unit conversion (Celsius/Fahrenheit)

**Architecture:**
```c
ThermostatConfig  → Configuration (setpoints, delays, enables)
ThermostatInput   → Sensor readings + time + user overrides
ThermostatState   → Internal state machine variables
ThermostatOutput  → GPIO control signals (heating, cooling, fan, etc.)
```

**Key Features:**

**1. Thermal Domain:**
- States: IDLE, HEATING, HEATING_STAGE2, COOLING, COOLING_STAGE2
- Stage 2 activation: Time delay (5-10 min) AND temperature deviation (>0.75°C)
- Minimum cycle time: 180 seconds (equipment protection)
- Hysteresis period: 300 seconds (prevents rapid switching)
- Deadbands: Comfort (0.5°C), Eco (1.5°C)

**2. Fan Domain:**
- Pre-run: Fan starts 30 seconds before thermal activation
- Post-run: Fan continues 60 seconds after thermal stops
- Manual override support
- Automatic coordination with thermal state

**3. Open Window Detection:**
- Monitors 3-sample temperature history
- Detects rapid drop (≥2°C over 3 minutes)
- Suspends heating for 15 minutes
- Prevents false triggers (gradual changes ignored)

**4. Humidity Domain:**
- Humidification or dehumidification modes
- 5% humidity deadband
- Independent from thermal operation

**5. Hot Water Domain:**
- Demand-driven (on/off control)
- No complex logic required

**APIs:**
```c
void thermostat_config_init(ThermostatConfig* config);
bool thermostat_config_validate(const ThermostatConfig* config);
void thermostat_state_init(ThermostatState* state, uint32_t now_seconds);
void thermostat_update(const ThermostatConfig* config,
                      const ThermostatInput* input,
                      ThermostatState* state,
                      ThermostatOutput* output);
float thermostat_c_to_f(float celsius);
float thermostat_f_to_c(float fahrenheit);
```

**Test Coverage:** 22 comprehensive tests including:
- Configuration validation
- Hysteresis protection
- Comfort/Eco deadbands
- Stage 2 activation logic
- Mutual exclusion (heating vs cooling)
- Sensor failure safety
- Fan timing (pre-run, post-run, manual override)
- HVAC system integration scenarios
- Open window detection
- 24-hour simulation

**Safety Features:**
- Immediate shutdown on sensor failure
- Heating and cooling mutual exclusion
- Equipment protection via minimum cycle times
- Configuration validation prevents invalid states

---

### 6.7 Boost Feature (Heating, Cooling, Hot Water)

**Status:** Implemented and Tested

**Description:**
The Boost feature allows temporary override of normal thermostat logic for rapid activation of heating, cooling, or hot water. When activated, the selected domain runs at maximum output (stage 1 only) for a configurable duration, after which normal operation resumes automatically. Each domain maintains independent boost state with per-domain timers.

**Implementation Details:**
- **API Functions:**
  - `thermostat_boost_activate(state, domain, now, duration_sec)` - Activate boost
  - `thermostat_boost_cancel(state, domain)` - Cancel active boost
  - `thermostat_boost_is_active(state, domain, now)` - Check boost status

- **Boost Behavior:**
  - Heating/Cooling: Forces stage 1 output ON; stage 2 always disabled during boost
  - Hot Water: Forces output ON regardless of demand signal
  - Duration: User-configurable per activation (in seconds)
  - Expiration: Automatic when current time ≥ boost_end_time
  - State tracking: boost_active flag, boost_end_time, boost_last_duration per domain

**Safety Features:**
1. **Sensor Failure Override:** Boost is disabled if sensors_valid = false
2. **Mutual Exclusion:** If both heating and cooling boosts are active, only the most recently activated boost runs (or both disabled if equal end times)
3. **Configuration Respect:** Boost won't activate if domain is disabled in config (heating_enabled, cooling_enabled, hot_water_enabled)
4. **Open Window Detection:** Heating boost is cancelled if open window is detected
5. **Normal Logic Resumption:** After boost expires or is cancelled, normal thermostat logic takes over immediately

**Test Coverage (24/24 tests pass):**

*Test 23 - Basic Functionality (5 scenarios):*
1. Boost activation for heating, cooling, and hot water
2. Override normal logic (forces output ON despite unfavorable conditions)
3. Stage 2 disabled during boost
4. Boost expiration after timeout
5. Manual boost cancellation

*Test 24 - Comprehensive Edge Cases (15 scenarios):*
1. Stage 2 config enabled but stays off during boost
2. Reactivating boost after expiration
3. Overlapping boost activations (updates end time)
4. Invalid domain names (graceful handling)
5. Boost with duration=0 (immediate expiration)
6. Sensor failure safety override
7. Normal logic resumption after expiration
8. Fan interaction during boost
9. Multiple simultaneous boosts (non-conflicting domains)
10. Mutual exclusion - heating vs cooling (latest wins)
11. Mutual exclusion - equal end times (both disabled)
12. Selective cancellation (cancel one, others remain active)
13. Exact boundary expiration (at end_time, not after)
14. Boost + open window detection (boost cancelled)
15. Boost respects domain enabled flags

**Dependencies:** Thermostat module (complete), UI integration (pending), NVS persistence (pending)

---

## 7. User Interface

### 7.1 Provisioning Mode Interface

**When:** First boot or after factory reset (no WiFi credentials)

**Display:**
```
┌─────────────────────────────────┐
│                                 │
│     WiFi Setup Required         │
│                                 │
│     Connect to: HESTIA32        │
│     Open: 192.168.4.1           │
│                                 │
│     Device will restart         │
│     after configuration         │
│                                 │
└─────────────────────────────────┘
```

**Web Interface:**
1. User connects phone/laptop to "HESTIA32" WiFi
2. Browser automatically redirects to 192.168.4.1 (captive portal)
3. Web form displays:
   - Scan button → Shows available WiFi networks
   - WiFi SSID input (pre-filled from scan)
   - WiFi Password input
   - Node Name input (optional, default: "Hestia32")
   - OTA Server URL (optional)
   - Save button
4. After save: Device restarts and connects to WiFi

### 7.2 Calibration Wizard Interface

**When:** First boot (no touch calibration in NVS)

**Display:**
```
┌─────────────────────────────────┐
│ Touch target 1/5 (60s remaining)│
│                                 │
│  +                              │  ← Crosshair (20px)
│                                 │
│                                 │
└─────────────────────────────────┘
```

**Sequence:**
1. Top-left corner (10, 10)
2. Top-right corner (310, 10)
3. Bottom-right corner (310, 470)
4. Bottom-left corner (10, 470)
5. Center (160, 240)

**Timeouts:**
- Overall: 60 seconds
- Per target: 15 seconds
- If timeout: Use default calibration (200-3900 range)

**User Feedback:**
- Countdown timer updates every second
- Target advances immediately on touch
- Visual feedback on touch detection

### 7.3 Main Application Interface

**When:** After successful WiFi connection and calibration

**Display Elements:**

1. **Tile Grid:** 4×6 colorful tiles (background pattern)
2. **Animated Bar:** White vertical bar sliding left-to-right (6s loop)
3. **FPS Counter:** Top-left corner, updates every second
4. **Touch Indicator:** 16×16 magenta circle with white border, follows touch

**Color Palette:**
- Background: #101820 (dark blue-gray)
- Tiles: 8 colors cycling (#2364AA, #3DA5D9, #73BFB8, #FEC601, #EA7317, #6C2E2F, #8A2BE2, #2ECC71)
- Touch dot: #FF00FF (magenta)

**Performance:**
- Target: 25+ FPS
- LVGL task runs at 100Hz (10ms period)
- Flush callback triggered by LVGL as needed

---

## 8. Data Management

### 8.1 NVS (Non-Volatile Storage) Layout

**Partition:** 24KB at offset 0x9000

**Namespaces:**

| Namespace | Keys | Purpose |
|-----------|------|---------|
| `wifi_config` | ssid, password, node_name, ota_url | WiFi provisioning data |
| `touch_cal` | x_min, x_max, y_min, y_max, swap_xy, flip_x, flip_y | Touch calibration |

**Data Types:**
- Strings: null-terminated, max 128 bytes
- Integers: uint16_t (calibration), uint8_t (boolean flags)

### 8.2 Partition Table

**File:** `partitions_two_ota.csv`

```
# Name      Type    SubType   Offset   Size     Flags
nvs         data    nvs                0x6000   (24KB)
otadata     data    ota                0x2000   (8KB)
phy_init    data    phy                0x1000   (4KB)
ota_0       app     ota_0              0x1D0000 (1.8MB)
ota_1       app     ota_1              0x1D0000 (1.8MB)
```

**Total Flash Required:** 3.76MB (fits in 4MB flash)

### 8.3 Data Persistence Strategy

**WiFi Credentials:**
- Saved immediately after provisioning form submission
- Loaded on boot if device is provisioned
- Erased on factory reset

**Touch Calibration:**
- Saved immediately after calibration wizard completes
- Loaded on boot to skip recalibration
- Erased on factory reset

**OTA State:**
- Managed automatically by ESP-IDF OTA library
- Tracks active partition (ota_0 or ota_1)
- Handles rollback on boot failure

---

## 9. Security Considerations

### 9.1 Current Security Posture

**Implemented:**
- HTTPS OTA downloads (SSL/TLS encryption)
- No hardcoded WiFi credentials
- Provisioning AP requires physical proximity

**Limitations (Development Mode):**
1. Provisioning AP is **open** (no password)
2. No firmware signing/verification
3. No certificate validation for OTA server
4. HTTP provisioning interface (not HTTPS)
5. No authentication on web interface

### 9.2 Production Security Recommendations

**High Priority:**
1. **Enable provisioning AP password**
   - Set `PROV_AP_PASSWORD` to strong passphrase
   - Display password on device screen or label

2. **Implement HTTPS for provisioning**
   - Use self-signed certificate or Let's Encrypt
   - Prevents credential interception

3. **Add OTA server certificate validation**
   - Pin server certificate or use CA bundle
   - Prevents man-in-the-middle attacks

4. **Enable Flash Encryption**
   - Encrypt firmware and NVS data at rest
   - Protects credentials if device is stolen

**Medium Priority:**

5. **Add web interface authentication**
   - Username/password for provisioning
   - Session tokens to prevent CSRF

6. **Implement rate limiting**
   - Limit failed WiFi connection attempts
   - Prevent brute-force attacks

7. **Add audit logging**
   - Log OTA updates, factory resets, provisioning events
   - Store in external server or encrypted local storage

### 9.3 Security Testing Checklist

- [ ] Test provisioning with special characters in password
- [ ] Test factory reset completely erases credentials
- [ ] Verify bootloader can't be overwritten
- [ ] Test rollback after OTA failure

---

## 10. Development Environment

### 10.1 Prerequisites

**Common:**
- Git with submodule support
- Python 3.8+
- USB drivers for ESP32 board

**For ESP32 (PlatformIO):**
- [PlatformIO Core](https://platformio.org/) or PlatformIO IDE
- GCC toolchain (installed by PlatformIO)

**For ESP32-C5 (ESP-IDF):**
- ESP-IDF v5.5.2 or later (C5 support)
- CMake 3.16+
- Ninja build system
- RISC-V toolchain (installed by ESP-IDF)

**For Testing (Native):**
- GCC or Clang with C11 support
- Make (optional)

### 10.2 ESP-IDF Setup (ESP32-C5)

**One-time installation:**

```bash
# Clone ESP-IDF v5.5
cd ~/esp
git clone --recursive --branch v5.5.2 \
    https://github.com/espressif/esp-idf.git esp-idf-v5.5

# Install tools for ESP32-C5
cd ~/esp/esp-idf-v5.5
./install.sh esp32c5
```

**Per-session environment setup:**

```bash
# Source ESP-IDF environment
source ~/esp/esp-idf-v5.5/export.sh

# Or use project script
./setup-c5.sh

# Configure target (first time only)
idf.py set-target esp32c5
```

### 10.3 PlatformIO Setup (ESP32)

**Installation:**

```bash
# Install PlatformIO CLI
pip install platformio

# Or use PlatformIO IDE (VS Code extension)
```

**Configuration:** `platformio.ini`
```ini
[env:nodemcu-32s]
platform = espressif32
board = nodemcu-32s
framework = espidf
monitor_speed = 115200
board_build.partitions = partitions_two_ota.csv
```

### 10.4 Cloning the Repository

**Important:** Project uses LVGL as a git submodule

```bash
# Clone with submodules
git clone --recursive https://github.com/your-username/hestia32-firmware.git
cd hestia32-firmware

# If already cloned without --recursive:
git submodule update --init --recursive
```

### 10.5 VS Code Integration

**Recommended Extensions:**
- C/C++ (Microsoft)
- ESP-IDF (Espressif) for C5 development
- PlatformIO IDE for ESP32 development

**Tasks:** Available via [Tasks.json](./vscode/tasks.json)
- ESP-IDF: Build
- ESP-IDF: Flash
- ESP-IDF: Monitor
- ESP-IDF: Build and Flash
- ESP-IDF: Build, Flash and Monitor
- ESP-IDF: Clean
- ESP-IDF: Menuconfig

---

## 11. Build and Deployment

### 11.1 Building for ESP32-C5

```bash
# Set up environment
source ~/esp/esp-idf-v5.5/export.sh

# Build firmware
idf.py --preview build

# View build output
ls build/hestia32.bin
```

**Build outputs:**
- `build/hestia32.bin` - Application firmware
- `build/bootloader/bootloader.bin` - Bootloader
- `build/partition_table/partition-table.bin` - Partition table
- `build/ota_data_initial.bin` - OTA data initialization

### 11.2 Flashing to ESP32-C5

**Full flash (first time):**

```bash
idf.py --preview -p /dev/ttyACM0 flash
```

**Application only (faster):**

```bash
idf.py --preview -p /dev/ttyACM0 app-flash
```

**Erase everything:**

```bash
idf.py --preview -p /dev/ttyACM0 erase-flash
```

**Serial port detection:**
- Linux: `/dev/ttyUSB0` or `/dev/ttyACM0`
- macOS: `/dev/cu.usbserial-*`
- Windows: `COM3`, `COM4`, etc.

### 11.3 Building for ESP32 (PlatformIO)

```bash
# Build
pio run -e nodemcu-32s

# Upload
pio run -e nodemcu-32s -t upload

# Monitor
pio device monitor
```

### 11.4 Monitoring Serial Output

**ESP-IDF:**

```bash
idf.py --preview -p /dev/ttyACM0 monitor

# Or combined flash + monitor:
idf.py --preview -p /dev/ttyACM0 flash monitor
```

**PlatformIO:**

```bash
pio device monitor -b 115200
```

**Expected output on first boot:**

```
I (123) main: Hestia32 ESP32-C5 Application
I (125) main: Firmware Version: 1.0.0
I (130) main: NVS initialized
I (135) main: Checking BOOT button on GPIO 28...
I (140) main: BOOT button GPIO 28 level: 1 (released)
I (145) display: No calibration found - running wizard first
I (150) display: Starting touch calibration (60 second timeout)
I (155) display: Touch target 1/5 (60s remaining)
...
```

---

## 12. Testing Strategy

### 12.1 Unit Testing (Thermostat Module)

**Location:** `tests/test_thermostat.c`

**Execution:**

```bash
# Using Makefile
make test

# Manual
gcc -Wall -Wextra -std=c11 -I. src/thermostat.c tests/test_thermostat.c -o test_thermostat -lm
./test_thermostat
```

**Test Suite:** 22 comprehensive tests

1. Configuration validation
2. Hysteresis protection (prevents rapid switching)
3. Comfort deadband (±0.5°C)
4. Eco deadband (±1.5°C)
5. Stage 2 heating (10min delay + 0.75°C deviation)
6. Stage 2 cooling (5min delay + 0.75°C deviation)
7. Heating/cooling mutual exclusion
8. Sensor failure safety (immediate shutdown)
9. Temperature unit conversion
10. Fan pre-run (30s before thermal)
11. Fan post-run (60s after thermal)
12. Fan manual override
13. Hysteresis + HVAC (fan coordination)
14. Comfort deadband + HVAC
15. Eco deadband + HVAC
16. Stage 2 heating + HVAC
17. Stage 2 cooling + HVAC
18. Mutual exclusion + HVAC
19. Sensor failure + HVAC
20. Open window detection (2°C drop over 3 minutes)
21. Open window false positive prevention
22. 24-hour heating cycle simulation

**Output:**
```
=== TEST 1: Configuration Validation ===
Testing: thermostat_config_validate() with various invalid configs
✓ PASSED: Configuration Validation

=== TEST 22: 24-Hour Heating Cycle Simulation ===
Simulating 24 hours (86400 seconds)...
✓ PASSED: 24-Hour Heating Cycle Simulation

=== SUMMARY ===
Total Tests: 22
Passed: 22
Failed: 0
Success Rate: 100.00%
```

### 12.2 Integration Testing

**Manual test procedures:**

**Test 1: WiFi Provisioning**
1. Flash firmware with erased NVS
2. Verify AP "HESTIA32" appears
3. Connect to AP
4. Verify captive portal redirect
5. Complete provisioning form
6. Verify device restarts and connects
7. Check serial logs for WiFi IP address

**Test 2: Touch Calibration**
1. Flash firmware with erased calibration
2. Verify calibration wizard appears
3. Touch all 5 targets
4. Verify UI responds to touch correctly
5. Factory reset and repeat
6. Verify calibration persists after reboot

**Test 3: Factory Reset**
1. Provision device with WiFi
2. Calibrate touchscreen
3. Power off
4. Hold BOOT button, power on
5. Hold for 3+ seconds
6. Verify "Factory reset confirmed!" in logs
7. Verify device enters provisioning mode
8. Verify calibration wizard runs

**Test 4: OTA Update**
1. Build firmware v1.0.0
2. Flash to device
3. Build firmware v1.0.1 (increment APP_VERSION)
4. Upload v1.0.1 to HTTPS server
5. Configure OTA URL via provisioning
6. Trigger update (manual or automatic)
7. Verify update completes
8. Verify device boots v1.0.1
9. Check logs for "OTA update successful"

**Test 5: Display Performance**
1. Flash firmware
2. Complete setup
3. Verify main UI renders
4. Check FPS counter (should be 25+)
5. Test touch responsiveness (dot follows)
6. Verify animated bar moves smoothly
7. Run for 1 hour, check for crashes/leaks

### 12.3 Hardware Validation

**Electrical tests:**
- [ ] Verify 3.3V on display VCC
- [ ] Check SPI signals with logic analyzer (MOSI, SCLK, CS, DC)
- [ ] Measure backlight PWM signal (5kHz)
- [ ] Test touch IRQ goes low on press
- [ ] Verify MISO line is connected only to touch controller

**Physical tests:**
- [ ] Touch all corners and center
- [ ] Verify no dead zones on display
- [ ] Test at different viewing angles (IPS should be uniform)
- [ ] Check backlight uniformity
- [ ] Test button press responsiveness

### 12.4 Stress Testing

**Network stress:**
- Disconnect WiFi during operation
- Verify auto-reconnect works
- Test provisioning with weak signal
- Test with 50+ WiFi networks visible

**Power stress:**
- Test rapid power cycles
- Verify NVS data integrity
- Test brownout protection
- Measure power consumption

**Memory stress:**
- Run device for 24+ hours
- Monitor free heap via logs
- Check for memory leaks
- Verify no task stack overflows

### 12.5 Test Automation (Future)

**Recommended framework:** Unity + CMock

**Automated tests to implement:**
1. WiFi provisioning API mocking
2. NVS read/write simulation
3. Display callback verification
4. OTA state machine testing
5. Touch calibration algorithm validation

---

## 13. Future Roadmap

### 13.1 Short-term (Next Release - v1.1.0)

**Thermostat Integration:**
- [ ] Wire thermostat module into main.c
- [ ] Add temperature/humidity sensor support (BME280, SHT45)
- [ ] Create settings UI for setpoint adjustment
- [ ] Add relay control for heating/cooling outputs
- [ ] Display current temperature and mode on screen

**Security Enhancements:**
- [ ] Add password to provisioning AP
- [ ] Enable HTTPS for web provisioning
- [ ] Add firmware version display in UI

**UI Improvements:**
- [ ] Design thermostat control screen
- [ ] Add WiFi status indicator
- [ ] Design settings control
- [ ] Show current time (NTP sync)
- [ ] Add screen brightness adjustment

### 13.2 Medium-term (v1.2.0 - v1.3.0)

**Advanced Thermostat Features:**
- [ ] Schedule programming (7-day schedule)
- [ ] Remote control via MQTT or HTTP API
- [ ] Historical data logging (temperature trends)
- [ ] Vacation mode (extended away settings)
- [ ] Smart learning (adaptive scheduling)

**Connectivity:**
- [ ] MQTT support (Home Assistant and OpenHAB integration with auto discovery)
- [ ] RESTful API for remote control
- [ ] WebSocket for real-time updates
- [ ] Bluetooth provisioning alternative
- [ ] Zigbee support
- [ ] Thread support

**Hardware Expansion:**
- [ ] Support for additional sensors (air quality, motion)
- [ ] External relay board compatibility
- [ ] Support for larger displays (480×800)

### 13.3 Long-term (v2.0.0+)

**Machine Learning:**
- [ ] Occupancy detection
- [ ] Predictive heating/cooling
- [ ] Anomaly detection (equipment failure)
- [ ] Energy optimization recommendations

**Professional Features:**
- [ ] Multi-zone support (up to 8 zones)
- [ ] Commercial HVAC compatibility
- [ ] BACnet/Modbus protocol support
- [ ] Professional installer mode

---

## 14. Appendices

### Appendix A: Acronyms and Abbreviations

| Acronym | Definition |
|---------|------------|
| AP | Access Point |
| BACnet | Building Automation and Control Networks |
| CS | Chip Select |
| DC | Data/Command (Display) |
| DHCP | Dynamic Host Configuration Protocol |
| DNS | Domain Name System |
| ESP-IDF | Espressif IoT Development Framework |
| FPS | Frames Per Second |
| FSD | Functional Specification Document |
| GPIO | General Purpose Input/Output |
| HTTPS | Hypertext Transfer Protocol Secure |
| HVAC | Heating, Ventilation, and Air Conditioning |
| IDF | IoT Development Framework |
| ILI9488 | Display controller IC |
| IPS | In-Plane Switching (LCD technology) |
| IRQ | Interrupt Request |
| JSON | JavaScript Object Notation |
| LEDC | LED Controller (PWM) |
| LVGL | Light and Versatile Graphics Library |
| MISO | Master In, Slave Out (SPI) |
| MOSI | Master Out, Slave In (SPI) |
| MQTT | Message Queuing Telemetry Transport |
| NTP | Network Time Protocol |
| NVS | Non-Volatile Storage |
| OTA | Over-The-Air |
| PHY | Physical Layer |
| PWM | Pulse Width Modulation |
| RGB | Red, Green, Blue |
| RISC-V | Reduced Instruction Set Computer - V |
| RST | Reset |
| SCLK | Serial Clock (SPI) |
| SPI | Serial Peripheral Interface |
| SSID | Service Set Identifier (WiFi network name) |
| TLS | Transport Layer Security |
| UART | Universal Asynchronous Receiver/Transmitter |
| UI | User Interface |
| URL | Uniform Resource Locator |
| USB | Universal Serial Bus |
| XPT2046 | Touch controller IC |

### Appendix B: File Structure

```
hestia32-firmware/
├── src/                          # Source code
│   ├── main.c                    # Application entry point
│   ├── config.h                  # Configuration constants
│   ├── wifi_manager.c/h          # WiFi connection management
│   ├── wifi_provisioning.c/h     # Web-based provisioning
│   ├── provisioning_html.h       # Embedded HTML template
│   ├── ota_manager.c/h           # OTA update handling
│   ├── display_manager.cpp/h     # Display and touch drivers
│   ├── display_config.h          # Display pin configuration
│   ├── display_ui.cpp/h          # LVGL UI creation
│   ├── thermostat.c/h            # Thermostat control logic
│   └── CMakeLists.txt            # Build configuration
├── tests/                        # Test suite
│   └── power_test.c/h            # Power consumption tests
│   ├── rgb_led_test.c/h          # RGB LED tests
│   └── test_thermostat.c         # Thermostat unit tests
├── components/                   # External components
│   ├── esp_lcd_ili9488/          # ILI9488 display driver
│   └── lvgl/                     # LVGL library (submodule)
├── build/                        # Build output (generated)
├── CMakeLists.txt                # Top-level ESP-IDF config
├── platformio.ini                # PlatformIO configuration
├── partitions_two_ota.csv        # Partition table
├── sdkconfig.defaults            # ESP-IDF default settings
├── setup-c5.sh                   # ESP32-C5 environment script
├── Makefile                      # Test build automation
├── README.md                     # Project documentation
├── QUICK_START.md                # Quick reference guide
├── WIRING.md                     # Hardware wiring guide
└── Hestia32-FSD.md        # This document
```

### Appendix C: Key Configuration Parameters

**File:** `src/config.h`

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| PROV_AP_SSID | "HESTIA32" | Provisioning AP name |
| PROV_AP_PASSWORD | "" | AP password (empty = open) |
| FACTORY_RESET_GPIO | 28 (C5) / 0 (ESP32) | BOOT button GPIO |
| WIFI_MAX_RETRY | 5 | WiFi connection retry count |
| OTA_CHECK_INTERVAL_MS | 300000 | OTA check interval (5 min) |
| APP_VERSION | "1.0.0" | Firmware version string |
| LOOP_DELAY_MS | 10000 | Main loop delay (deprecated) |

**File:** `src/display_config.h`

| Parameter | Value | Description |
|-----------|-------|-------------|
| TFT_WIDTH | 320 | Display width (pixels) |
| TFT_HEIGHT | 480 | Display height (pixels) |
| LCD_PIXEL_CLOCK_HZ | 20MHz | SPI clock (display) |
| TOUCH_SPEED | 1MHz | SPI clock (touch) |
| LVGL_TICK_PERIOD_MS | 2 | LVGL tick rate |
| LVGL_BUFFER_HEIGHT | 80 | Line buffer height |

### Appendix D: Useful Commands Reference

**ESP-IDF (ESP32-C5):**

```bash
# Environment setup
source ~/esp/esp-idf-v5.5/export.sh

# Build commands
idf.py --preview build                    # Build firmware
idf.py --preview clean                    # Clean build
idf.py --preview fullclean                # Full clean (includes config)
idf.py --preview menuconfig               # Configuration menu

# Flashing
idf.py --preview -p PORT flash            # Flash all
idf.py --preview -p PORT app-flash        # Flash app only
idf.py --preview -p PORT erase-flash      # Erase everything

# Monitoring
idf.py --preview -p PORT monitor          # Serial monitor
idf.py --preview -p PORT flash monitor    # Flash + monitor

# Size analysis
idf.py --preview size                     # Show binary sizes
idf.py --preview size-components          # Size by component
idf.py --preview size-files               # Size by file
```

**PlatformIO (ESP32):**

```bash
# Build commands
pio run -e nodemcu-32s                    # Build firmware
pio run -e nodemcu-32s -t clean           # Clean build

# Flashing
pio run -e nodemcu-32s -t upload          # Upload firmware
pio run -e nodemcu-32s -t erase           # Erase flash

# Monitoring
pio device monitor -b 115200              # Serial monitor
pio run -e nodemcu-32s -t upload -t monitor  # Upload + monitor

# Dependencies
pio lib install                           # Install libraries
```

**Testing:**

```bash
# Thermostat tests
make test                                 # Run all tests
make clean                                # Clean test build

# Manual test build
gcc -Wall -Wextra -std=c11 -I. src/thermostat.c tests/test_thermostat.c -o test_thermostat -lm
./test_thermostat
```

### Appendix E: Troubleshooting Guide

**Problem:** Device won't enter provisioning mode

**Solution:**
1. Check serial logs for "Device provisioned" message
2. Perform factory reset (hold BOOT 5 seconds)
3. Verify NVS partition is not corrupted: `idf.py erase-flash`

---

**Problem:** Touch not responding or inaccurate

**Solution:**
1. Verify MISO line connected only to touch (not display)
2. Run factory reset to trigger recalibration
3. Check touch IRQ pin goes low on press
4. Verify SPI signals with logic analyzer

---

**Problem:** Display shows garbage or wrong colors

**Solution:**
1. Check VCC is 3.3V (not 5V)
2. Verify SPI connections (MOSI, SCLK, CS, DC, RST)
3. Add 100µF capacitor near display VCC
4. Reduce SPI clock speed (change `LCD_PIXEL_CLOCK_HZ`)

---

**Problem:** WiFi won't connect after provisioning

**Solution:**
1. Check WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)
2. Verify password is correct (check for special characters)
3. Check router allows new device connections
4. Try factory reset and re-provision
5. Check serial logs for connection error codes

---

**Problem:** OTA update fails

**Solution:**
1. Verify OTA URL is accessible (try in browser)
2. Check server uses HTTPS (not HTTP)
3. Verify firmware binary is valid (`idf.py build`)
4. Check available space in target partition
5. Monitor serial logs for specific error codes

---

**Problem:** Device crashes or reboots randomly

**Solution:**
1. Check power supply is stable (min 500mA)
2. Monitor heap usage via serial logs
3. Look for task stack overflows
4. Run in debug mode with panic handler enabled
5. Test with minimal UI (comment out animations)

---

**Problem:** Factory reset not working

**Solution:**
1. Verify BOOT button GPIO matches hardware (28 for C5, 0 for ESP32)
2. Check button actually pulls GPIO low (test with multimeter)
3. Increase hold time (currently 5 seconds)
4. Check serial logs for "BOOT button level: 0"

---

### Appendix F: References and Resources

**ESP-IDF Documentation:**
- Official Docs: https://docs.espressif.com/projects/esp-idf/en/latest/
- API Reference: https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/
- ESP32-C5 Datasheet: https://www.espressif.com/en/products/socs/esp32-c5

**LVGL Documentation:**
- Official Site: https://lvgl.io/
- Documentation: https://docs.lvgl.io/
- GitHub: https://github.com/lvgl/lvgl

**ILI9488 Resources:**
- Datasheet: ILI9488 V0.2 2014/10/08
- Driver: https://github.com/espressif/esp-iot-solution/tree/master/components/lcd/ili9488

**XPT2046 Resources:**
- Datasheet: XPT2046 Touch Screen Controller
- Arduino Library: https://github.com/PaulStoffregen/XPT2046_Touchscreen

**PlatformIO:**
- Official Site: https://platformio.org/
- ESP32 Platform: https://docs.platformio.org/en/latest/platforms/espressif32.html

**Community Resources:**
- ESP32 Forum: https://esp32.com/
- LVGL Forum: https://forum.lvgl.io/
- GitHub Issues: https://github.com/your-username/hestia32-firmware/issues

---

## Document Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | Initial | Original | Initial FSD with comprehensive analysis, actual implementation details, thermostat specification, testing strategy |

---

**End of Document**
