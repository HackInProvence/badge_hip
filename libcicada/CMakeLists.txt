# FIXME: license, for now defaults to Copyright (C) Miaou 2025

# Use with cmake -DPICO_BOARD=pico_w
#  or cmake -DPICO_PLATFORM=rp2040
#  or cmake -DPICO_NO_FLASH=1
#  -Gninja -DCMAKE_BUILD_TYPE=Debug
# SDK is in $PICO_SDK_PATH

# --- Common to projects around the badge ---

cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(secsea C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

#set(PICO_BOARD pico)
#set(PICO_BOOT_STAGE2_FILE ${PICO_SDK_PATH}/src/rp2040/boot_stage2/boot2_generic_03h.S)

# If you want debug output from USB (pass -DPICO_STDIO_USB=1) this ensures you don't lose any debug output while USB is set up
if (NOT DEFINED PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS)
    set(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 3000)
endif()

pico_sdk_init()

# Set the optimization level
#set(CMAKE_C_FLAGS_RELEASE "-O1")


# --- Specific to libcicada ---

add_executable(test_cicada)
target_sources(test_cicada PRIVATE libcicada.c main.c)
#target_compile_definitions(pio_ws2812_parallel PRIVATE FOO=1)
pico_generate_pio_header(test_cicada ${CMAKE_CURRENT_LIST_DIR}/libcicada.pio)
pico_add_extra_outputs(test_cicada)

target_link_libraries(test_cicada PRIVATE
    hardware_pio
    pico_rand
    pico_stdlib
)

# enable usb output, disable uart output
pico_enable_stdio_usb(test_cicada 1)
pico_enable_stdio_uart(test_cicada 0)

# Pass cmake -DLIBCICADA_BUZZ_PIN=x, where x is the pin you want to use
if(LIBCICADA_BUZZ_PIN)
    target_compile_definitions(test_cicada PRIVATE LIBCICADA_BUZZ_PIN=${LIBCICADA_BUZZ_PIN})
endif()

# Additionally generate python and hex (where?) pioasm outputs for inclusion in the RP2040 datasheet
add_custom_target(pio_libcicada_pygen DEPENDS ${CMAKE_BINARY_DIR}/libcicada.pio.py)
add_custom_command(OUTPUT libcicada.pio.py
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/libcicada.pio
    COMMAND pioasm -o python ${CMAKE_CURRENT_LIST_DIR}/libcicada.pio libcicada.pio.py
    VERBATIM)
add_dependencies(test_cicada pio_libcicada_pygen)

# TODO: make an INTERFACE (FIXME change the target_sources of the test_ app and move the generate_header to the lib)
