add_library(radio INTERFACE)
target_sources(radio INTERFACE ${CMAKE_CURRENT_LIST_DIR}/radio.c)
target_include_directories(radio SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(radio INTERFACE
    badge
    hardware_gpio
    hardware_spi
    pico_time
)
