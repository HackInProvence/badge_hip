# This is to be able to include the pinout from libraries
add_library(badge INTERFACE)
target_include_directories(badge SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_sources(badge INTERFACE ${CMAKE_CURRENT_LIST_DIR}/badge_defs.h)

# Add libraries projects
add_subdirectory(btns)
add_subdirectory(leds)
add_subdirectory(log)
add_subdirectory(music)
add_subdirectory(noise_gen)
add_subdirectory(radio)
add_subdirectory(screen)
#add_subdirectory(template)

# Add tests
add_subdirectory(tests)


# Screen precompilation target: generate C headers from images using image2epaper.py
# Images paths are relative to the current directory and will output basename.h that can be included by them
# Add badge_images to your link libraries to use one of the images
function(badge_image2epaper path)
    get_filename_component(basename ${path} NAME_WLE)
    add_custom_command(OUTPUT ${basename}.h
        DEPENDS ${path}
        COMMAND python3 ${CMAKE_CURRENT_LIST_DIR}/image2epaper.py ${CMAKE_CURRENT_LIST_DIR}/${path} -o ${basename}.h
        VERBATIM
    )
    target_sources(badge_images PRIVATE ${basename}.h)
endfunction()
add_library(badge_images INTERFACE)
target_include_directories(badge_images SYSTEM INTERFACE ${CMAKE_CURRENT_BINARY_DIR})

badge_image2epaper(tests/imgs/hip_bw.png)
badge_image2epaper(tests/imgs/secsea_bw.png)
badge_image2epaper(tests/imgs/squares_bw.png)
badge_image2epaper(tests/imgs/text_bw.png)
badge_image2epaper(tests/imgs/grad_4g.png)
badge_image2epaper(tests/imgs/secsea_4g.png)
badge_image2epaper(tests/imgs/hip_4g.png)
badge_image2epaper(tests/imgs/notif_4g.png)
badge_image2epaper(tests/imgs/companion.png)
badge_image2epaper(tests/imgs/hip_anim.webp)
