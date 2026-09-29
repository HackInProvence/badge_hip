# Video library: plays a video converted by video2epaper.py from the SD card (see video.h)
add_library(video INTERFACE)
target_sources(video INTERFACE ${CMAKE_CURRENT_LIST_DIR}/video.c)
target_include_directories(video SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(video INTERFACE
    audio
    badge
    pico_time
    screen
    sd
)


# Standalone video player (see player.c)
add_executable(badge_video)
target_sources(badge_video PRIVATE player.c)
pico_add_extra_outputs(badge_video)

target_link_libraries(badge_video PRIVATE
    badge
    pico_stdlib
    btns
    log
    screen
    video
)

# enable usb output, disable uart output
pico_enable_stdio_usb(badge_video 1)
pico_enable_stdio_uart(badge_video 0)
