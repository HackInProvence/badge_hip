add_library(btns INTERFACE)
target_sources(btns INTERFACE ${CMAKE_CURRENT_LIST_DIR}/btns.c)
target_include_directories(btns SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(btns INTERFACE
    badge
    hardware_gpio
)
