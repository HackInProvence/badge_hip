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
