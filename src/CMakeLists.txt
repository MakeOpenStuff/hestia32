# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.c)
FILE(GLOB_RECURSE app_sources_cpp ${CMAKE_SOURCE_DIR}/src/*.cpp)

# Test sources - uncomment the ones you want to use
set(test_sources
    ${CMAKE_SOURCE_DIR}/tests/power_test.c
    # ${CMAKE_SOURCE_DIR}/tests/rgb_led_test.c
)

idf_component_register(SRCS ${app_sources} ${app_sources_cpp} ${test_sources}
                       INCLUDE_DIRS "." "../tests"
                       REQUIRES driver esp_wifi esp_event nvs_flash esp_netif
                                esp_http_client esp_http_server esp_https_ota
                                app_update bootloader_support wifi_provisioning
                                protocomm json spi_flash esp_lcd lvgl esp_lcd_ili9488)
