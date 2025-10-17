add_library(screen INTERFACE)
target_sources(screen INTERFACE ${CMAKE_CURRENT_LIST_DIR}/screen.c)
target_include_directories(screen SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR} "..")

target_link_libraries(screen INTERFACE
)
