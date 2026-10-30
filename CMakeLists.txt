# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

# Use with cmake -DPICO_BOARD=badge_secsea or badge_proto or badge_radio
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
