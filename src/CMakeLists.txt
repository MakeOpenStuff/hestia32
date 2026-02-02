# Hestia32 Firmware Component

# Core sources (protocol-agnostic)
set(CORE_SRCS
    "core/thermostat.c"
    "core/display_manager.cpp"
    "core/display_ui.cpp"
    "core/protocol_manager.c"
)

# Protocol-specific sources (MQTT for now)
set(PROTOCOL_SRCS
    "protocols/mqtt/mqtt_protocol.c"
    "protocols/mqtt/wifi_manager.c"
    "protocols/mqtt/wifi_provisioning.c"
    "protocols/mqtt/ota_manager.c"
)

# Main application
set(APP_SRCS
    "main.c"
)

# Test sources - uncomment the ones you want to use
set(TEST_SRCS
    "${CMAKE_SOURCE_DIR}/tests/power_test.c"
    # "${CMAKE_SOURCE_DIR}/tests/rgb_led_test.c"
)

idf_component_register(
    SRCS ${CORE_SRCS} ${PROTOCOL_SRCS} ${APP_SRCS} ${TEST_SRCS}
    INCLUDE_DIRS "." "core" "protocols" "../tests"
    REQUIRES driver esp_wifi esp_event nvs_flash esp_netif
             esp_http_client esp_http_server esp_https_ota
             app_update bootloader_support wifi_provisioning
             protocomm json spi_flash esp_lcd lvgl esp_lcd_ili9488
)

# Define CONFIG_PROTOCOL_MQTT for this component
target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_PROTOCOL_MQTT=1)
