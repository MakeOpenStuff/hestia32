idf_component_register(
    SRCS
        "i2c_scanner_main.c"
        "${CMAKE_SOURCE_DIR}/../i2c_scanner.c"
    INCLUDE_DIRS
        "."
        "${CMAKE_SOURCE_DIR}/.."
)
