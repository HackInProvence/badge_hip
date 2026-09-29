add_library(gfx INTERFACE)
target_sources(gfx INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/gfx.c
    ${CMAKE_CURRENT_LIST_DIR}/gfx_fonts.c
)
target_include_directories(gfx SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})
