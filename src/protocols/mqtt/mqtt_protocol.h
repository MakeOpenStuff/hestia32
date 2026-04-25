#ifndef MQTT_PROTOCOL_H
#define MQTT_PROTOCOL_H

#include "protocols/protocol_interface.h"

/**
 * @brief Get MQTT protocol interface
 * 
 * Returns the protocol interface for MQTT/WiFi communication.
 * This wraps WiFi provisioning, WiFi connection, and OTA updates.
 * 
 * @return Pointer to MQTT protocol interface
 */
const protocol_interface_t* protocol_get_mqtt(void);

#endif // MQTT_PROTOCOL_H
