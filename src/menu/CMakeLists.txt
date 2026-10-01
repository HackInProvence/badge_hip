# Menu application: the features of the badge in a menu shown on the screen (see main.c)

add_executable(badge_menu)
target_sources(badge_menu PRIVATE
    main.c
    admin.c
    apps.c
    lamp.c
    battle.c
    chorus.c
    contacts.c
    crypto_app.c
    crypto_ctf.c
    duel.c
    hotcold.c
    image_radio.c
    infection.c
    messages.c
    nametag.c
    program.c
    vote.c
    talk.c
    remote.c
    ui.c
    vcard.c
    net.c
    ook_rx.c
    ook_tx.c
    radio433.c
    radio_tune.c
    ledcast.c
    announce.c
    reset.c
    ../radio/ookdec.c
    battery.c
    battcal.c
    party.c
    werewolf.c
    assassin.c
    tug.c
    gamebook.c
    rtttl.c
    rtttl_parse.c
    pirate_radio.c
    demo.c
    smuggler.c
    skills.c
    achievements.c
    display.c
    credits.c
    ctf.c
    games.c
    puzzles.c
    puzzles_app.c
    oled_demo.c
    radio_tools.c
    rsvp.c
    score_code.c
    screen_demo.c
    social.c
    store.c
)
target_include_directories(badge_menu PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# Version of the firmware (menu Réglages > Infos, "!" on the serial port): raise it for each release given to the
# badges. version.h also gets the git commit and the date of the build, at each build.
set(BADGE_VERSION 1.0.0)
add_custom_target(badge_version ALL
    COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -DOUT=${CMAKE_CURRENT_BINARY_DIR}/version.h
            -DVERSION=${BADGE_VERSION} -P ${CMAKE_CURRENT_LIST_DIR}/version.cmake
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/version.h
    COMMENT "Version of the badge")
add_dependencies(badge_menu badge_version)
target_include_directories(badge_menu PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
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
    hardware_adc
    hardware_flash
    hardware_watchdog
    pico_rand
    qrcode
    radio
    screen
    sd
    video
)

# enable usb output, disable uart output
pico_enable_stdio_usb(badge_menu 1)
pico_enable_stdio_uart(badge_menu 0)
