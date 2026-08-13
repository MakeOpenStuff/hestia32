cmake_minimum_required(VERSION 3.16.0)
set(EXTRA_COMPONENT_DIRS src components)

execute_process(
    COMMAND git -C "${CMAKE_CURRENT_SOURCE_DIR}" status --porcelain --untracked-files=no
    OUTPUT_VARIABLE GIT_STATUS
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(GIT_STATUS)
    set(APP_VERSION_SUFFIX "-dirty")
else()
    set(APP_VERSION_SUFFIX "")
endif()

add_compile_definitions(APP_VERSION_SUFFIX="${APP_VERSION_SUFFIX}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hestia32)
