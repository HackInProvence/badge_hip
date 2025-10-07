# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

# Use with cmake -DPICO_BOARD=pico_w or badge
#  or cmake -DPICO_NO_FLASH=1
#  -Gninja -DCMAKE_BUILD_TYPE=Debug
# SDK is in $PICO_SDK_PATH

cmake_minimum_required(VERSION 3.13)

# Tell the SDK where to find our board header (badge_secsea),
#  but leave the choice between using it or not (mainly because I develop on pico_w for now...)
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/src)
include(src/pico_sdk_import.cmake)

project(secsea C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# If you want debug output from USB (pass -DPICO_STDIO_USB=1) this ensures you don't lose any debug output while USB is set up
if (NOT DEFINED PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS)
    set(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 3000)
endif()

pico_sdk_init()

# Set the optimization level
#set(CMAKE_C_FLAGS_RELEASE "-O1")

add_subdirectory(src)
