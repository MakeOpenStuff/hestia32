# Hestia32 ESP32-C5 Display Wiring Guide

## ESP32-C5-DevKitC-1 Pinout (30-pin)

```
                    ESP32-C5-DevKitC-1 (30-pin)

         LEFT SIDE (15 pins)         RIGHT SIDE (15 pins)
    ┌─────────────┐              ┌─────────────┐
    │ 3V3         │              │         GND │
    │ RTS         │              │         TX0 │ (GPIO 11)
    │ 2           │ Touch MISO   │         RX0 │ (GPIO 12)
    │ 3           │ Relay 3      │         24  │ BME280 SDA
    │ 0           │ Relay 1      │         23  │
    │ 1           │ Relay 2      │         15  │ Touch IRQ
    │ 6           │ SPI CLK      │         27  │
    │ 7           │ SPI MOSI     │         4   │
    │ 8           │ Display DC   │         5   │ BME280 SCL
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
- **BME280**: GPIO 4, 5 (I2C)
- **Relays**: GPIO 0, 1, 3, 26
- **Reserved**: TX0/RX0 (GPIO 11/12 - UART0), GPIO 28 (BOOT), RTS

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

## BME280 Environmental Sensor

### Sensor Specifications
- Measures: Temperature, Humidity, Pressure
- Interface: I2C
- I2C Address: 0x76 (default) or 0x77

### Pin Connections

| BME280 Pin | ESP32-C5 GPIO | Notes |
|------------|---------------|-------|
| VCC | 3.3V | Power supply |
| GND | GND | Ground |
| SDA | GPIO 4 | I2C Data |
| SCL | GPIO 5 | I2C Clock |

### I2C Configuration
- **I2C Port**: I2C_NUM_0
- **Speed**: 100 kHz (standard mode)
- **Pull-ups**: Internal pull-ups enabled

### Notes
1. BME280 uses I2C interface with 7-bit addressing
2. Default I2C address is 0x76, alternative is 0x77 (check your module)
3. Built-in driver handles calibration data and compensation automatically

---

## Solid State Relays (SSR)

### Relay Configuration
Four SSR outputs for HVAC control (heating, cooling, fan, etc.)

### Pin Connections

| Relay | ESP32-C5 GPIO | Function | Notes |
|-------|---------------|----------|-------|
| Relay 1 | GPIO 0 | Output 1 | Active HIGH to trigger SSR |
| Relay 2 | GPIO 1 | Output 2 | Active HIGH to trigger SSR |
| Relay 3 | GPIO 3 | Output 3 | Active HIGH to trigger SSR |
| Relay 4 | GPIO 26 | Output 4 | Active HIGH to trigger SSR |

### Relay Control
- **Logic Level**: 3.3V (HIGH = relay ON)
- **Control Type**: Direct GPIO output
- **Switching**: Solid state (no mechanical contacts)

### Notes
1. SSRs should have 3.3V-compatible control inputs
2. SSR load side handles AC/DC high voltage - ensure proper isolation
3. GPIOs selected to avoid conflicts with display (6-10, 13-15) and UART (11-12)
4. Adjacent GPIO numbers (0,1,3,26) simplify PCB routing

---

## GPIO Summary

### Reserved/In Use
| GPIO | Function | Notes |
|------|----------|-------|
| 0 | Relay 1 | SSR control |
| 1 | Relay 2 | SSR control |
| 2 | Touch MISO | XPT2046 data out |
| 3 | Relay 3 | SSR control |
| 4 | BME280 SDA | I2C data |
| 5 | BME280 SCL | I2C clock |
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
