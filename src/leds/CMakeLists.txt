add_library(leds INTERFACE)
target_sources(leds INTERFACE ${CMAKE_CURRENT_LIST_DIR}/leds.c)
target_include_directories(leds SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})
pico_generate_pio_header(leds ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

target_link_libraries(leds INTERFACE
    badge
    hardware_pio
)

## Additionally generate python and hex (where?) pioasm outputs for inclusion in the RP2040 datasheet
# FIXME: does not really work as is, probably because of paths... It work when there was a single project...
#add_custom_target(leds_pygen DEPENDS ${CMAKE_BINARY_DIR}/ws2812.pio.py)
#add_custom_command(OUTPUT ws2812.pio.py
#    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio
#    COMMAND pioasm -o python ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio ws2812.pio.py
#    VERBATIM)
#add_dependencies(noise_gen leds_pygen)
