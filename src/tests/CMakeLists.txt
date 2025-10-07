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
