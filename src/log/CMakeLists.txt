add_library(log INTERFACE)

target_include_directories(log SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})

# Use cmake -DBADGE_DISABLE_LOGS to disable logs on STDOUT (UART/USB/...)
if (NOT BADGE_DISABLE_LOGS)
    message("BADGE: logs enabled")
    target_sources(log INTERFACE ${CMAKE_CURRENT_LIST_DIR}/log.c)
    target_link_libraries(log INTERFACE
        badge
        pico_stdlib
    )
else()
    message("BADGE: logs disabled")
    target_compile_definitions(log INTERFACE BADGE_DISABLE_LOGS)
    target_sources(log INTERFACE ${CMAKE_CURRENT_LIST_DIR}/log_nolog.c)
    target_link_libraries(log INTERFACE badge)
endif()

# The "newer" syntax does not have the same effect as the "older" one which is deprecated...
#set(CACHE{BADGE_DISABLE_LOGS} TYPE BOOL HELP "Disables the logs" FORCE VALUE ${BADGE_DISABLE_LOGS})
set(BADGE_DISABLE_LOGS ${BADGE_DISABLE_LOGS} CACHE BOOL "Disables the logs on STDOUT (UART/USB/...)" FORCE)
