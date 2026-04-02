# Hestia32 ESP32-C5 Display Wiring Guide

This file was originally written for ESP32-C5-DevKitC-1 wiring. XIAO uses a different pin map and now has its own section below.

## ESP32-C5-DevKitC-1 Pinout (30-pin)

```
										ESP32-C5-DevKitC-1 (30-pin)

				 LEFT SIDE (15 pins)         RIGHT SIDE (15 pins)
		┌─────────────┐              ┌─────────────┐
		│ 3V3         │              │         GND │
		│ RTS         │              │         TX0 │ (GPIO 11)
		│ 2           │ Touch MISO   │         RX0 │ (GPIO 12)
		│ 3           │ Relay 3      │         24  │
		│ 0           │ Relay 1      │         23  │
		│ 1           │ Relay 2      │         15  │ Touch IRQ/PEN
		│ 6           │ SPI CLK      │         27  │
		│ 7           │ SPI MOSI     │         4   │ SHT45 SCL
		│ 8           │ Display DC   │         5   │ SHT45 SDA
		│ 9           │ Display RST  │         NC  │
		│ 10          │ Display CS   │         28  │
		│ 26          │ Relay 4      │         GND │
		│ 25          │              │         14  │ Touch CS
		│ 5V          │              │         13  │ Backlight
		│ GND         │              │         GND │
		└─────────────┘              └─────────────┘
											 USB-C
```

### Pin Usage Summary
- **Power Rails**: 3.3V (left top), 5V (left bottom), GND (multiple)
- **Display**: GPIO 6-10, 13 (SPI + backlight)
- **Touch**: GPIO 2, 14, 15 (shared SPI on 6-7)
- **SHT45 Sensor**: GPIO 4 (SCL), GPIO 5 (SDA) - I2C
- **Relays**: GPIO 0, 1, 3, 26
- **Reserved**: TX0/RX0 (GPIO 11/12 - UART0), GPIO 28 (BOOT), RTS

---

## XIAO ESP32-C5 Wiring (Current Firmware)

### XIAO ESP32-C5 Pinout (14-pin)

```
							 XIAO ESP32-C5 (top view)

								 USB-C CONNECTOR (top)
				LEFT SIDE (7 pins)         RIGHT SIDE (7 pins)
	 ┌─────────────┐               ┌─────────────┐
	 │ D0 / GPIO1  │ Display DC    │ 5V          │ VBUS
	 │ D1 / GPIO0  │ Backlight     │ GND         │ GND
	 │ D2 / GPIO25 │ Display RST   │ 3V3         │ 3V3
	 │ D3 / GPIO7  │ Display CS    │ D10 / GPIO10│ Display MOSI + Touch MOSI
	 │ D4 / GPIO23 │ I2C SDA       │ D9  / GPIO9 │ Touch MISO
	 │ D5 / GPIO24 │ I2C SCL       │ D8  / GPIO8 │ Display SCK + Touch SCK
	 │ D6 / GPIO11 │ Touch IRQ/PEN │ D7  / GPIO12│ Touch CS
	 └─────────────┘               └─────────────┘
								 ANTENNA SIDE (bottom)
```

//TODO: Pinout for both boards need verification and cleanup for useability
### XIAO Pin Usage Summary
- **Display**: GPIO 8,10,7,1,25
- **Touch**: GPIO 12,11,8,9
- **I2C (TCA9555/SHT45)**: GPIO 23,24
- **Power**: 5V, GND

### Important
- XIAO does not share the DevKit pin numbering layout. Do not wire XIAO using the DevKit table above.
- In firmware, XIAO display mapping is defined in `src/core/display_config.h` under `#ifdef CONFIG_BOARD_TYPE_XIAO`.
- **XIAO Relays**: Controlled via TCA9555 I2C GPIO expander pins 0-3, same active-LOW logic (expander pin LOW = relay ON)

### XIAO Relay Configuration (via TCA9555)
```
TCA9555 Pin 0 → Relay 1 control input (active-LOW)
TCA9555 Pin 1 → Relay 2 control input (active-LOW)
TCA9555 Pin 2 → Relay 3 control input (active-LOW)
TCA9555 Pin 3 → Relay 4 control input (active-LOW)
```
Wire each TCA9555 output pin to SSR control negative, with SSR control positive to +5V (same as DevKit).

### XIAO Display/Touch Pin Mapping

| Signal | XIAO GPIO | Notes |
|--------|-----------|-------|
| Display SCK | GPIO 8 | Seeed D8 |
| Display MOSI | GPIO 10 | Seeed D10 |
| Display MISO (unused) | Not connected - Display is write-only |
| Display CS | GPIO 7 | Seeed D3 |
| Display DC | GPIO 1 | Seeed D0 |
| Display RST | GPIO 25 | Seeed D2 - Required for reliable startup on XIAO |
| Backlight | GPIO 0 | Seeed D1, PWM-controlled with gradual boot ramp |
| Touch SCK | GPIO 8 | Seeed D8 |
| Touch CS | GPIO 12 | Seeed D7 |
| Touch IRQ/PEN | GPIO 11 | Seeed D6 |
| Touch MOSI | GPIO 10 | Seeed D10 |
| Touch MISO | GPIO 9 | Seeed D9 |

---

## ILI9488 3.5" IPS Display Connection

### Display Module Specifications
- Controller: ILI9488
- Size: 3.5 inches
- Resolution: 320x480 pixels
- Interface: SPI
- Touch: XPT2046 (Resistive)
- Panel Type: IPS

### Pin Connections

| Display Pin | ESP32-C5 GPIO | Notes |
|-------------|---------------|-------|
| **Power** |||
| VCC | 3.3V | Power supply |
| GND | GND | Ground |
| **Display SPI** |||
| CS | GPIO 10 | Chip Select |
| RESET/RST | GPIO 9 | Hardware reset (NOT GPIO 11 - conflicts with U0TXD) |
| DC/RS | GPIO 8 | Data/Command select (NOT GPIO 12 - conflicts with U0RXD) |
| SDI/MOSI | GPIO 7 | SPI Data Out |
| SCK | GPIO 6 | SPI Clock |
| SDO/MISO | Not Connected | Leave disconnected - conflicts with touch controller |
| LED/BL | GPIO 13 | Backlight (PWM capable) |
| **Touch Controller (XPT2046)** |||
| T_CS | GPIO 14 | Touch Chip Select |
| T_PEN | GPIO 15 | Touch Pen Detect (active low) |
| T_DIN | GPIO 7 | Shared with MOSI |
| T_CLK | GPIO 6 | Shared with SCK |
| T_DO | GPIO 2 | Dedicated to touch (display MISO must be disconnected) |

### SPI Bus Configuration
- **SPI Host**: SPI2_HOST
- **Write Speed**: 40 MHz
- **Read Speed**: 16 MHz
- **Touch Speed**: 1 MHz
- **Mode**: SPI Mode 0 (CPOL=0, CPHA=0)

### Notes
1. **GPIO 11/12 Reserved**: These are U0TXD and U0RXD (UART0) - do not use for display
2. **Backlight Control**: GPIO 13 supports PWM for brightness adjustment
3. **CRITICAL - MISO Conflict**: Display SDO/MISO **must be disconnected**. The display and touch controllers cannot share GPIO 2. The display doesn't need MISO for normal operation (write-only), so GPIO 2 is dedicated to touch T_DO only.
4. **Touch PEN Pin**: Active low signal, triggers when pen is pressed. Same as IRQ/PENIRQ on other modules.
5. **Touch Calibration**: Automatic calibration wizard runs on boot. Touch all 5 crosshairs (corners + center) to calibrate. Calibration data is computed from corner samples with axis swap/flip detection.
6. **Level Shifting**: Not needed - ILI9488 modules typically have 3.3V logic

### Power Consumption
- Typical: ~150-200mA @ 3.3V (with backlight)
- Backlight off: ~20-30mA

### Optional: Add a capacitor
- 100µF capacitor between VCC and GND near the display recommended
- Helps stabilize power supply during screen updates

---

## SHT45 Temperature & Humidity Sensor

### Sensor Specifications
- Measures: Temperature, Humidity, Pressure
- Interface: I2C
- I2C Address: 0x76 (default) or 0x77

### Pin Connections

| SHT45 Pin | ESP32-C5 GPIO | Notes |
|------------|---------------|-------|
| VCC | 3.3V | Power supply |
| GND | GND | Ground |
| SDA | GPIO 5 | I2C Data |
| SCL | GPIO 4 | I2C Clock |

### I2C Configuration
- **I2C Port**: I2C_NUM_0
- **Speed**: 100 kHz (standard mode)
- **Pull-ups**: Internal pull-ups enabled

### Notes
1. SHT45 uses I2C interface with 7-bit addressing (address 0x44)
2. Default I2C address is 0x76, alternative is 0x77 (check your module)
3. Built-in driver handles calibration data and compensation automatically

---

## Solid State Relays (SSR)

### Relay Configuration
Four SSR outputs for HVAC control (heating, cooling, fan, etc.)

### Pin Connections

| Relay | ESP32-C5 GPIO | Function | Notes |
|-------|---------------|----------|-------|
| Relay 1 | GPIO 0 | Output 1 | Active LOW - GPIO LOW = Relay ON |
| Relay 2 | GPIO 1 | Output 2 | Active LOW - GPIO LOW = Relay ON |
| Relay 3 | GPIO 3 | Output 3 | Active LOW - GPIO LOW = Relay ON |
| Relay 4 | GPIO 26 | Output 4 | Active LOW - GPIO LOW = Relay ON |

### Relay Control
- **Logic Level**: Active LOW (GPIO LOW = relay ON, GPIO HIGH = relay OFF)
- **Wiring**: SSR control + pin → +5V, control - pin → ESP32 GPIO
- **Control Type**: Direct GPIO output (DevKit) or TCA9555 I2C expander (XIAO)
- **Switching**: Solid state (no mechanical contacts)
- **Recommended SSR**: OMRON G3MB-202P or similar with 5V input

### Why Active-LOW?
- **Fail-safe**: GPIOs default HIGH during boot → relays stay OFF
- **Safety**: System crash or reset keeps outputs disabled
- **Current sinking**: GPIOs sink current better than sourcing

### Notes
1. SSRs should have 3.3V-compatible control inputs
2. SSR load side handles AC/DC high voltage - ensure proper isolation
3. GPIOs selected to avoid conflicts with display (6-10, 13-15) and UART (11-12)
4. Adjacent GPIO numbers (0,1,3,26) simplify PCB routing

### PCB Wiring Example (per SSR)
```
                            OMRON G3MB-202P
+5V ──────────────────────► [+] Control Input
                            [-] Control Input ──► ESP32 GPIO (0, 1, 3, or 26)

GND ───────────────────────────────────────────► Ground
```

When GPIO = LOW (0V): Current flows (+5V → SSR LED → GPIO), SSR conducts → **Relay ON**
When GPIO = HIGH (3.3V): Insufficient voltage (1.7V) across LED → **Relay OFF**
On boot/reset: GPIO defaults HIGH → All relays safe (OFF)

---

## GPIO Summary

### Reserved/In Use
| GPIO | Function | Notes |
|------|----------|-------|
| 0 | Relay 1 | SSR control |
| 1 | Relay 2 | SSR control |
| 2 | Touch MISO | XPT2046 data out |
| 3 | Relay 3 | SSR control |
| 4 | SHT45 SDA | I2C data |
| 5 | SHT45 SCL | I2C clock |
| 6 | SPI Clock | Shared display/touch |
| 7 | SPI MOSI | Shared display/touch |
| 8 | Display DC | Data/Command |
| 9 | Display RST | Reset |
| 10 | Display CS | Chip Select |
| 11 | UART0 TX | Reserved (serial) |
| 12 | UART0 RX | Reserved (serial) |
| 13 | Backlight | PWM control |
| 14 | Touch CS | Touch chip select |
| 15 | Touch IRQ | Touch interrupt |
| 26 | Relay 4 | SSR control |

### Available GPIOs
Check ESP32-C5 pinout under /docs/esp32-c5/ for additional available GPIOs not listed above.
