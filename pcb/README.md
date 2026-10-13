# PCB Variants

This folder contains PCB Gerber files ready for manufacturing, Bill of Materials (BOM) and notes for Hestia32 hardware variants.

## Variant Matrix

There are 4 possible PCB variants from two feature axes:

- Board target: `DevKit` or `XIAO`
- Product profile: `EU` or `HVAC`

Resulting variants:

| Variant | Folder | BOM | Interactive BOM |
|---------|--------|-----|-----------------|
| XIAO-HVAC | [XIAO-HVAC/](XIAO-HVAC/) | [BOM](https://github.com/MakeOpenStuff/hestia32/blob/develop/pcb/XIAO-HVAC/BOM_Hestia32-HVAC_XIAO.csv) | [iBOM](https://htmlpreview.github.io/?https://github.com/MakeOpenStuff/hestia32/blob/develop/pcb/XIAO-HVAC/iBOM_Hestia32-HVAC_XIAO.html) |
| XIAO-EU | [XIAO-EU/](XIAO-EU/) | [BOM](https://github.com/MakeOpenStuff/hestia32/blob/develop/pcb/XIAO-EU/BOM_Hestia32-EU_XIAO.csv) | [iBOM](https://htmlpreview.github.io/?https://github.com/MakeOpenStuff/hestia32/blob/develop/pcb/XIAO-EU/iBOM_Hestia32-EU_XIAO.html) |
| DevKit-HVAC | [DevKit-HVAC/](DevKit-HVAC/) | [BOM](https://github.com/MakeOpenStuff/hestia32/blob/develop/pcb/DevKit-HVAC/BOM_Hestia32-HVAC_DevKit.csv) | [iBOM](https://htmlpreview.github.io/?https://github.com/MakeOpenStuff/hestia32/blob/develop/pcb/DevKit-HVAC/iBOM_Hestia32-HVAC_DevKit.html) |
| DevKit-EU | [DevKit-EU/](DevKit-EU/) | [BOM](https://github.com/MakeOpenStuff/hestia32/blob/develop/pcb/DevKit-EU/BOM_Hestia32-EU_DevKit.csv) | [iBOM](https://htmlpreview.github.io/?https://github.com/MakeOpenStuff/hestia32/blob/develop/pcb/DevKit-EU/iBOM_Hestia32-EU_DevKit.html) |

## History

### XIAO-HVAC
- `v1.7`:
  - Make LCD horizontally centered
- `v1.6`:
  - Optimise thermal properties (remove copper fill around sensor and add air gap)
- `v1.5`:
  - No funtional change
  - Added LCD SDO (MISO) test point
  - Changed label 3V3 to XIAO_3V3 to avoid users driving heavy loads
- `v1.4`:
  - Split versioning between HVAC and EU PCB designs
  - Use LM2596HVS instead of LM2596S

### XIAO-EU
- `v1.7`:
  - Make LCD horizontally centered
- `v1.6`:
  - Optimise thermal properties (remove copper fill around sensor and add air gap)
- `v1.5`:
  - Fixed GND test point
  - Increase track width for AC NETs
- `v1.4`:
  - No funtional change
  - Added LCD SDO (MISO) test point
  - Changed label 3V3 to XIAO_3V3 to avoid users driving heavy loads

### Shared for both XIAO-HVAC and XIAO-EU
- `v1.3`:
  - XIAO-HVAC: Used coil relays instead of solid-state relays
  - XIAO-HVAC: Used LM2596 for power supply
  - XIAO-HVAC: Improved filtering
  - XIAO-HVAC: Supported heat pumps
  - XIAO-EU: Used coil relays instead of solid-state relays
  - XIAO-EU: Improved filtering
  - XIAO-EU: Supported heat pumps
- `v1.2`:
  - Improve sensor location
	- Improve via stitching
	- Add extra info on silkscreen
- `v1.1`:
  - Fix I2C pull up resistors
  - Renumerate components prefixes without gaps
  - Add unpopulated additional GPIOs pins from port expander (P07)
  - Increase clearance around holes in case user uses large metal screw or washer
  - Add extra information on top silkscreen
- `v1.0`:
  - First complete PCB design release for the Hestia32 XIAO variant.

### DevKit-HVAC
- `v1.0`: Initial design based off XIAO-HVAC v1.7. Not tested.

### DevKit-EU
- `v1.0`: Initial design based off XIAO-EU v1.7. Not tested.

## TODO (Future Improvements)

- [x] Develop XIAO-EU
  - [x] Schematic
  - [x] BOM
  - [x] Gerber
  - [x] Use coil relays instead of solid-state relays
  - [x] Support heat pumps
  - [x] Improve filtering
- [x] Develop XIAO-HVAC
  - [x] Schematic
  - [x] BOM
  - [x] Gerber
  - [x] Use coil relays instead of solid-state relays
  - [x] Support heat pumps
  - [x] Use LM2596 for power supply
  - [x] Improve filtering
- [x] Renumber components prefixes without gaps
- [x] Add unpopulated additional GPIOs pins from port expander
- [x] Increase clearance around holes in case user uses large metal screw or washer
- [x] Develop DevKit-EU
  - [x] Schematic
  - [x] BOM
  - [x] Gerber
  - [x] Use coil relays instead of solid-state relays
  - [x] Support heat pumps
  - [x] Improve filtering
- [x] Develop DevKit-HVAC
  - [x] Schematic
  - [x] BOM
  - [x] Gerber
  - [x] Use coil relays instead of solid-state relays
  - [x] Support heat pumps
  - [x] Use LM2596 for power supply
  - [x] Improve filtering
- [ ] Manufacture Hestia32 Development Board