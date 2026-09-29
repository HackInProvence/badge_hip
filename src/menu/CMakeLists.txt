# Menu application: the features of the badge in a menu shown on the screen (see main.c)

add_executable(badge_menu)
target_sources(badge_menu PRIVATE
    main.c
    display.c
    ctf.c
    oled_demo.c
    radio_tools.c
    rsvp.c
    screen_demo.c
    social.c
    store.c
)
target_include_directories(badge_menu PRIVATE ${CMAKE_CURRENT_LIST_DIR})
pico_add_extra_outputs(badge_menu)

target_link_libraries(badge_menu PRIVATE
    audio
    badge
    badge_images
    pico_stdlib
    btns
    gfx
    leds
    log
    noise_gen
    ir
    oled
    pico_flash
    pico_unique_id
    hardware_flash
    pico_rand
    radio
    screen
    sd
    video
)

# enable usb output, disable uart output
pico_enable_stdio_usb(badge_menu 1)
pico_enable_stdio_uart(badge_menu 0)
