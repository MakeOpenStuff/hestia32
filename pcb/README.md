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

- `v1.0`: first complete PCB design release for the Hestia32 XIAO variant.

## TODO (Future Improvements)

- [ ] Add unpopulated additional GPIOs pins from port expander
- [ ] Increase clearance around holes in case user uses large metal screw or washer