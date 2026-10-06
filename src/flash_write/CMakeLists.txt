add_library(flash_write INTERFACE)
target_sources(flash_write INTERFACE ${CMAKE_CURRENT_LIST_DIR}/flash_write.c)
target_include_directories(flash_write SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})

# Contrary to what's usually done in this project, the badge library that depends on flash_write to be sure that any badge project reserves the sector
target_link_libraries(badge INTERFACE flash_write)

# This tells the linker we want to reserve the last sector of the flash.
# It's not telling to place data there if there are not explicitly positioned in the C.
target_link_options(flash_write INTERFACE "LINKER:--script=${CMAKE_CURRENT_LIST_DIR}/reserved_sector.ld")
set_property(TARGET flash_write APPEND PROPERTY INTERFACE_LINK_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/reserved_sector.ld)

# Initialize with default configuration while pushing the binary
#target_compile_definitions(flash_write INTERFACE FLASH_INITILIZE_PERSISTENCE)

target_link_libraries(flash_write INTERFACE
    #badge
    pico_flash
    log
)
