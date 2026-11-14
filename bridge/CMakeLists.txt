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

FetchContent_Declare(fatfs
    URL https://elm-chan.org/fsw/ff/arc/ff15a.zip
    URL_HASH SHA256=74737b1cafa1a67a3f722dd0d1a44767c5b54d37b6300ad3825a904cbe88fc3c
    SOURCE_SUBDIR none)
FetchContent_MakeAvailable(fatfs)
set(FATFS ${fatfs_SOURCE_DIR}/source)
set_source_files_properties(${FATFS}/ff.c ${FATFS}/ffunicode.c PROPERTIES COMPILE_OPTIONS "-w")

set(COMMON ${CMAKE_CURRENT_LIST_DIR}/../common)
add_executable(bridge src/main.c src/datalog.c src/sdspi.c ${FATFS}/ff.c ${FATFS}/ffunicode.c ${COMMON}/ecu.c ${COMMON}/link.c ${COMMON}/ms1.c ${COMMON}/ms1_tables.c
    ${COMMON}/ms2.c ${COMMON}/rtc.c ${COMMON}/tune.c)
target_include_directories(bridge PRIVATE ${COMMON} ${FATFS})
target_compile_options(bridge PRIVATE -include ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/ffconf.h)
target_compile_options(bridge PRIVATE -Wall -Wextra -Werror)
target_link_libraries(bridge pico_stdlib hardware_uart hardware_flash hardware_sync hardware_i2c hardware_spi)
pico_enable_stdio_usb(bridge 1)
pico_enable_stdio_uart(bridge 0)
pico_add_extra_outputs(bridge)
