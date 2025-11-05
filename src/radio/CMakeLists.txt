add_library(radio INTERFACE)
target_sources(radio INTERFACE ${CMAKE_CURRENT_LIST_DIR}/radio.c)
target_include_directories(radio SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(radio INTERFACE
    badge
    hardware_gpio
    hardware_spi
    pico_time
)

# Use cmake -DCC1101_fXOSC to specify your custom frequency
if (NOT CC1101_fXOSC)
    message("BADGE: using CC1101 frequency from ${PICO_BOARD}.h")
else()
    message("BADGE: using CC1101 frequnecy ${CC1101_fXOSC}")
    target_compile_definitions(radio INTERFACE CC1101_fXOSC=${CC1101_fXOSC})
endif()
