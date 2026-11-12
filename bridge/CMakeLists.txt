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

add_executable(bridge src/main.c src/ecu.c src/ms2.c)
target_compile_options(bridge PRIVATE -Wall -Wextra -Werror)
target_link_libraries(bridge pico_stdlib hardware_uart)
pico_enable_stdio_usb(bridge 1)
pico_enable_stdio_uart(bridge 0)
pico_add_extra_outputs(bridge)
