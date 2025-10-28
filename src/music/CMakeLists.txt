add_library(music INTERFACE)
target_sources(music INTERFACE ${CMAKE_CURRENT_LIST_DIR}/music.c)
target_include_directories(music SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(music INTERFACE
    badge
    hardware_clocks
    hardware_gpio
    hardware_pwm
    pico_time
)
