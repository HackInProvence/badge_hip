add_library(noise_gen INTERFACE)
target_sources(noise_gen INTERFACE ${CMAKE_CURRENT_LIST_DIR}/noise_gen.c)
target_include_directories(noise_gen SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})
pico_generate_pio_header(noise_gen ${CMAKE_CURRENT_LIST_DIR}/noise_gen.pio)

target_link_libraries(noise_gen INTERFACE
    hardware_pio
    pico_rand
)

# Pass cmake -DNOISE_GEN_BUZZ_PIN=x, where x is the pin you want to use
if(NOISE_GEN_BUZZ_PIN)
    target_compile_definitions(noise_gen INTERFACE NOISE_GEN_BUZZ_PIN=${NOISE_GEN_BUZZ_PIN})
endif()

## Additionally generate python and hex (where?) pioasm outputs for inclusion in the RP2040 datasheet
# FIXME: does not really work as is, probably because of paths... It work when there was a single project...
#add_custom_target(noise_gen_pygen DEPENDS ${CMAKE_BINARY_DIR}/noise_gen.pio.py)
#add_custom_command(OUTPUT noise_gen.pio.py
#    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/noise_gen.pio
#    COMMAND pioasm -o python ${CMAKE_CURRENT_LIST_DIR}/noise_gen.pio noise_gen.pio.py
#    VERBATIM)
#add_dependencies(noise_gen noise_gen_pygen)
