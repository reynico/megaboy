cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico CACHE STRING "")

include(FetchContent)
if(DEFINED ENV{PICO_SDK_PATH} AND NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()
if(NOT PICO_SDK_PATH)
    FetchContent_Declare(pico_sdk
        GIT_REPOSITORY https://github.com/raspberrypi/pico-sdk.git
        GIT_TAG 2.1.1
        GIT_SHALLOW TRUE
        GIT_SUBMODULES lib/tinyusb)
    FetchContent_Populate(pico_sdk)
    set(PICO_SDK_PATH ${pico_sdk_SOURCE_DIR})
endif()
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(megaboy_bridge C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

set(COMMON ${CMAKE_CURRENT_LIST_DIR}/../common)
add_executable(bridge src/main.c ${COMMON}/ecu.c ${COMMON}/link.c ${COMMON}/ms1.c ${COMMON}/ms1_tables.c
    ${COMMON}/ms2.c ${COMMON}/tune.c)
target_include_directories(bridge PRIVATE ${COMMON})
target_compile_options(bridge PRIVATE -Wall -Wextra -Werror)
target_link_libraries(bridge pico_stdlib hardware_uart hardware_flash hardware_sync)
pico_enable_stdio_usb(bridge 1)
pico_enable_stdio_uart(bridge 0)
pico_add_extra_outputs(bridge)
