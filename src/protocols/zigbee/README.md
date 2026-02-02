# Zigbee Protocol Implementation

**Status:** Not yet implemented (stub only)

## Overview

This directory contains stub implementation for Zigbee protocol support. The Zigbee protocol allows the Hestia32 thermostat to communicate with Zigbee smart home ecosystems like Home Assistant, Zigbee2MQTT, or proprietary Zigbee hubs.

## Implementation Plan

### Phase 1: Basic Zigbee End Device
- [ ] Initialize ESP Zigbee SDK
- [ ] Implement Zigbee thermostat cluster (HVAC)
- [ ] Device commissioning/pairing
- [ ] Basic attribute reporting (temperature, setpoint, mode)

### Phase 2: Advanced Features
- [ ] Relative humidity measurement cluster
- [ ] Fan control cluster
- [ ] Boost mode custom cluster
- [ ] OTA firmware updates via Zigbee
- [ ] Multi-relay control

### Phase 3: Optimization
- [ ] Power management (sleep modes)
- [ ] Network rejoin handling
- [ ] Persistent binding configuration

## Required Components

- **ESP Zigbee SDK**: ESP-IDF v5.1+ with Zigbee libraries
- **Hardware**: ESP32-C5 supports Zigbee 3.0 and Thread
- **Memory**: ~200KB flash, ~50KB RAM overhead

## Zigbee Clusters to Implement

| Cluster | ID | Purpose |
|---------|-----|---------|
| Thermostat | 0x0201 | HVAC control, setpoints, modes |
| Temperature Measurement | 0x0402 | Current temperature reporting |
| Relative Humidity | 0x0405 | Humidity reporting |
| Fan Control | 0x0202 | Fan speed/mode |
| On/Off | 0x0006 | Relay control |

## Configuration

Zigbee-specific settings will be stored in `zigbee_config.h`:
- Network channel
- PAN ID
- Join timeout
- Reporting intervals

## Testing

1. Pair with Zigbee coordinator (e.g., Zigbee2MQTT)
2. Verify attribute reporting
3. Test remote setpoint changes
4. Validate mode switching
5. Test power cycle recovery

## Resources

- [ESP Zigbee SDK Documentation](https://docs.espressif.com/projects/esp-zigbee-sdk/)
- [Zigbee Cluster Library Specification](https://zigbeealliance.org/developer_resources/zigbee-cluster-library/)
- [Home Assistant Zigbee Integration](https://www.home-assistant.io/integrations/zha/)
