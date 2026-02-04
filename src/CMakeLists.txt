# Hestia32 Firmware Component

# Core sources (protocol-agnostic)
set(CORE_SRCS
    "core/thermostat.c"
    "core/display_manager.cpp"
    "core/display_ui.cpp"
    "core/protocol_manager.c"
)

# Protocol-specific sources (conditionally compiled based on Kconfig)
set(PROTOCOL_SRCS "")

if(CONFIG_PROTOCOL_MQTT)
    list(APPEND PROTOCOL_SRCS
        "protocols/mqtt/mqtt_protocol.c"
        "protocols/mqtt/wifi_manager.c"
        "protocols/mqtt/wifi_provisioning.c"
        "protocols/mqtt/ota_manager.c"
    )
endif()

if(CONFIG_PROTOCOL_ZIGBEE)
    list(APPEND PROTOCOL_SRCS
        "protocols/zigbee/protocol_zigbee.c"
    )
endif()

if(CONFIG_PROTOCOL_MATTER)
    list(APPEND PROTOCOL_SRCS
        "protocols/matter/protocol_matter.c"
    )
endif()

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
    PRIV_REQUIRES mbedtls
)

# Protocol selection is now controlled by Kconfig (CONFIG_PROTOCOL_*)
# No need to manually define these anymore
