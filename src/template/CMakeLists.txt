add_library(template INTERFACE)
target_sources(template INTERFACE ${CMAKE_CURRENT_LIST_DIR}/template.c)
target_include_directories(template SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(template INTERFACE
    badge
)
