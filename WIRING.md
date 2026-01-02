# Hestia32 ESP32-C5 Display Wiring Guide

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
