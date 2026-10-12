add_library(radio INTERFACE)
target_sources(radio INTERFACE ${CMAKE_CURRENT_LIST_DIR}/radio.c)
target_include_directories(radio SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(radio INTERFACE
    badge_tests
    hardware_gpio
    hardware_irq
    hardware_pwm
    hardware_spi
    pico_time
    log
    flash_write
)

# Use cmake -DCC1101_fXOSC to specify your custom frequency
if (CC1101_fXOSC)
    # Now that this is just a default value for unconfigured devices, there is not much use for this...
    message("BADGE: using CC1101 frequency ${CC1101_fXOSC} (does not override the one in flash, see flash_write)")
    target_compile_definitions(radio INTERFACE CC1101_fXOSC=${CC1101_fXOSC})
endif()
