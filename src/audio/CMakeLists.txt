# Audio library: samples on the buzzer (audio.h), WAV player from the SD card (wav.h)
add_library(audio INTERFACE)
target_sources(audio INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/audio.c
    ${CMAKE_CURRENT_LIST_DIR}/wav.c
)
target_include_directories(audio SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(audio INTERFACE
    badge
    hardware_clocks
    hardware_dma
    hardware_gpio
    hardware_pwm
    log
    sd
)
