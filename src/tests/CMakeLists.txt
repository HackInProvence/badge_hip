# Test noise_gen

add_executable(test_noise_gen)
target_sources(test_noise_gen PRIVATE noise_gen.c)
pico_add_extra_outputs(test_noise_gen)

target_link_libraries(test_noise_gen PRIVATE
    pico_stdlib
    noise_gen
)

# enable usb output, disable uart output
pico_enable_stdio_usb(test_noise_gen 1)
pico_enable_stdio_uart(test_noise_gen 0)


# Test music

add_executable(test_music)
target_sources(test_music PRIVATE music.c)
pico_add_extra_outputs(test_music)

target_link_libraries(test_music PRIVATE
    pico_stdlib
    music
)

# enable usb output, disable uart output
pico_enable_stdio_usb(test_music 1)
pico_enable_stdio_uart(test_music 0)


# Test screen

add_executable(test_screen)
target_sources(test_screen PRIVATE screen.c)
pico_add_extra_outputs(test_screen)

target_link_libraries(test_screen PRIVATE
    hardware_gpio
    hardware_spi
    pico_stdlib
    screen
)

# enable usb output, disable uart output
pico_enable_stdio_usb(test_screen 1)
pico_enable_stdio_uart(test_screen 0)


# Test radio

add_executable(test_radio)
target_sources(test_radio PRIVATE radio.c)
pico_add_extra_outputs(test_radio)

target_link_libraries(test_radio PRIVATE
    hardware_gpio
    pico_stdlib
    pico_time
    radio
)

# enable usb output, disable uart output
pico_enable_stdio_usb(test_radio 1)
pico_enable_stdio_uart(test_radio 0)

add_executable(radio_calibrate)
target_sources(radio_calibrate PRIVATE radio_calibrate.c)
pico_add_extra_outputs(radio_calibrate)

target_link_libraries(radio_calibrate PRIVATE
    hardware_gpio
    pico_multicore
    pico_stdlib
    pico_time
    radio
)

# enable usb output, disable uart output
pico_enable_stdio_usb(test_radio 1)
pico_enable_stdio_uart(test_radio 0)


# Test leds

add_executable(test_leds)
target_sources(test_leds PRIVATE leds.c)
pico_add_extra_outputs(test_leds)

target_link_libraries(test_leds PRIVATE
    hardware_pio
    pico_stdlib
    leds
)

# enable usb output, disable uart output
pico_enable_stdio_usb(test_leds 1)
pico_enable_stdio_uart(test_leds 0)


# Test btns

add_executable(test_btns)
target_sources(test_btns PRIVATE btns.c)
pico_add_extra_outputs(test_btns)

target_link_libraries(test_btns PRIVATE
    pico_stdlib
    btns
)

# enable usb output, disable uart output
pico_enable_stdio_usb(test_btns 1)
pico_enable_stdio_uart(test_btns 0)


# Test logs

add_executable(test_log)
target_sources(test_log PRIVATE log.c)
pico_add_extra_outputs(test_log)

target_link_libraries(test_log PRIVATE
    pico_stdlib
    log
)

# enable usb output, disable uart output
pico_enable_stdio_usb(test_log 1)
pico_enable_stdio_uart(test_log 0)


## Test template
#
#add_executable(test_template)
#target_sources(test_template PRIVATE template.c)
#pico_add_extra_outputs(test_template)
#
#target_link_libraries(test_template PRIVATE
#    pico_stdlib
#    template
#)
#
## enable usb output, disable uart output
#pico_enable_stdio_usb(test_template 1)
#pico_enable_stdio_uart(test_template 0)
