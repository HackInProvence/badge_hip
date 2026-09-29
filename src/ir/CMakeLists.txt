add_library(ir INTERFACE)
target_sources(ir INTERFACE ${CMAKE_CURRENT_LIST_DIR}/ir.c)
target_include_directories(ir SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(ir INTERFACE
    badge
    hardware_clocks
    hardware_gpio
    hardware_pwm
    pico_time
)
