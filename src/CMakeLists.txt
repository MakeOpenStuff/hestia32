# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.c)

idf_component_register(SRCS ${app_sources}
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_wifi esp_event nvs_flash esp_netif
                                esp_http_client esp_http_server esp_https_ota
                                app_update bootloader_support wifi_provisioning
                                protocomm json)
