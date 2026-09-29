add_library(sd INTERFACE)
target_sources(sd INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/sd.c
    ${CMAKE_CURRENT_LIST_DIR}/diskio.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_fs.c
    ${CMAKE_CURRENT_LIST_DIR}/fatfs/ff.c
    ${CMAKE_CURRENT_LIST_DIR}/fatfs/ffunicode.c
)
target_include_directories(sd SYSTEM INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/fatfs
)

target_link_libraries(sd INTERFACE
    badge
    hardware_gpio
    hardware_spi
    pico_time
    log
)
