# PCB Variants

This folder contains PCB Gerber files ready for manufacturing, Bill of Materials (BOM) and notes for Hestia32 hardware variants.

## Variant Matrix

There are 4 possible PCB variants from two feature axes:

- Board target: `DevKit` or `XIAO`
- Product profile: `EU` or `HVAC`

Resulting variants:

| Variant | Folder | BOM |
|---------|--------|-----|
| DevKit-EU | DevKit-EU/ | - |
| DevKit-HVAC | DevKit-HVAC/ | - |
| XIAO-EU | [XIAO-EU/](XIAO-EU/) | [BOM](XIAO-EU/README.md) |
| XIAO-HVAC | [XIAO-HVAC/](XIAO-HVAC/) | [BOM](XIAO-HVAC/README.md) |

## History

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

## TODO (Future Improvements)

- [x] Develop XIAO-EU
  - [x] Schematic
  - [x] BOM
  - [x] Gerber
- [x] Develop XIAO-HVAC
  - [x] Schematic
  - [x] BOM
  - [x] Gerber
- [x] Renumber components prefixes without gaps
- [x] Add unpopulated additional GPIOs pins from port expander
- [x] Increase clearance around holes in case user uses large metal screw or washer
- [ ] Develop DevKit-EU
  - [ ] Schematic
  - [ ] BOM
  - [ ] Gerber
- [ ] Develop DevKit-HVAC
  - [ ] Schematic
  - [ ] BOM
  - [ ] Gerber
