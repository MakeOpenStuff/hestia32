# Matter Protocol Implementation

**Status:** Not yet implemented (stub only)

## Overview

This directory contains stub implementation for Matter protocol support. Matter enables the Hestia32 thermostat to work with Apple HomeKit, Google Home, Amazon Alexa, and other Matter-certified ecosystems.

## Implementation Plan

### Phase 1: Matter Device Commissioning
- [ ] Initialize ESP Matter SDK
- [ ] Implement Matter thermostat device type
- [ ] QR code/manual pairing code generation
- [ ] Wi-Fi/Thread commissioning
- [ ] Basic attribute reporting

### Phase 2: Thermostat Cluster Implementation
- [ ] Local temperature reporting
- [ ] Occupied heating/cooling setpoint
- [ ] System mode (Off/Heat/Cool/Auto)
- [ ] Running state (Idle/Heating/Cooling)
- [ ] Control sequence of operation

### Phase 3: Advanced Features
- [ ] Relative humidity measurement cluster
- [ ] Fan control cluster
- [ ] Custom boost mode cluster
- [ ] OTA firmware updates via Matter
- [ ] Multi-fabric support (Apple + Google simultaneously)

### Phase 4: Optimization
- [ ] Low-power modes
- [ ] Subscription management
- [ ] Persistent ACL storage
- [ ] Network failover (Wi-Fi ↔ Thread)

## Required Components

- **ESP Matter SDK**: ESP-IDF v5.1+ with Matter libraries
- **Hardware**: ESP32-C5 supports Matter over Wi-Fi and Thread
- **Memory**: ~400KB flash, ~100KB RAM overhead
- **Certification**: Optional Matter certification for production

## Matter Clusters to Implement

| Cluster | ID | Purpose |
|---------|-----|---------|
| Thermostat | 0x0201 | HVAC control, setpoints, modes |
| Temperature Measurement | 0x0402 | Current temperature reporting |
| Relative Humidity | 0x0405 | Humidity reporting |
| Fan Control | 0x0202 | Fan speed/mode |
| On/Off | 0x0006 | Relay control |
| Identify | 0x0003 | Device identification |

## Device Type

**Thermostat (Device Type ID: 0x0301)**
- Server clusters: Thermostat, Temperature Measurement, Humidity, Fan Control
- Client clusters: None
- Endpoint: 1

## Configuration

Matter-specific settings will be stored in `matter_config.h`:
- Vendor ID (VID)
- Product ID (PID)
- Device name
- Commissioning timeout
- Attribute reporting intervals

## Commissioning Flow

1. User initiates commissioning via Matter controller app
2. Device generates pairing QR code (shown on display)
3. Controller scans QR code
4. BLE/Wi-Fi commissioning completes
5. Device joins Matter fabric
6. Operational credentials exchanged
7. Device ready for control

## Testing

1. Commission with Apple Home
2. Commission with Google Home (multi-fabric)
3. Verify attribute reporting
4. Test remote setpoint changes
5. Validate mode switching
6. Test OTA updates
7. Power cycle recovery

## Resources

- [ESP Matter SDK Documentation](https://docs.espressif.com/projects/esp-matter/)
- [Matter Specification](https://csa-iot.org/developer-resource/specifications-download-request/)
- [Matter Device Types](https://github.com/project-chip/connectedhomeip/tree/master/src/app/clusters)
- [Apple HomeKit Matter Support](https://developer.apple.com/smart-home/)
- [Google Home Matter Integration](https://developers.home.google.com/matter)
