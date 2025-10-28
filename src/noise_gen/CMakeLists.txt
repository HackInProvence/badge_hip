add_library(noise_gen INTERFACE)
target_sources(noise_gen INTERFACE ${CMAKE_CURRENT_LIST_DIR}/noise_gen.c)
target_include_directories(noise_gen SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})
pico_generate_pio_header(noise_gen ${CMAKE_CURRENT_LIST_DIR}/noise_gen.pio)

target_link_libraries(noise_gen INTERFACE
    badge
    hardware_pio
    pico_rand
)

## Additionally generate python and hex (where?) pioasm outputs for inclusion in the RP2040 datasheet
# FIXME: does not really work as is, probably because of paths... It work when there was a single project...
#add_custom_target(noise_gen_pygen DEPENDS ${CMAKE_BINARY_DIR}/noise_gen.pio.py)
#add_custom_command(OUTPUT noise_gen.pio.py
#    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/noise_gen.pio
#    COMMAND pioasm -o python ${CMAKE_CURRENT_LIST_DIR}/noise_gen.pio noise_gen.pio.py
#    VERBATIM)
#add_dependencies(noise_gen noise_gen_pygen)
