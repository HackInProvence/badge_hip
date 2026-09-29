add_library(oled INTERFACE)
target_sources(oled INTERFACE ${CMAKE_CURRENT_LIST_DIR}/oled.c)
target_include_directories(oled SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(oled INTERFACE
    badge
    hardware_gpio
    hardware_i2c
    log
)
