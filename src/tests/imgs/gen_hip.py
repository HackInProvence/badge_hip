#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Generate the WebP layers of the cicada to animate it on the pico.
Generalized to slide in an image from the left, turn it 360, then slide out on the right.
"""

import argparse

from PIL import Image


def resize_dither(img, n_colors=2, width=200, height=200):
    """Resize, then quantize to black and white or 4 grays with dither."""
    img = img.resize((width, height), resample=Image.Resampling.LANCZOS)
    #return img.quantize(2, dither=Image.Dither.FLOYDSTEINBERG)  # This applies a threshold and does not dither

    # So we create a palette, which is given to quantize inside an Image...
    grays = [(i*255)//(n_colors-1) for i in range(n_colors)]
    rgb_pal = []
    for g in grays:
        rgb_pal.extend([g,g,g])
    impal = Image.new('P', (1,1))
    impal.putpalette(rgb_pal)
    return img.quantize(palette=impal, dither=Image.Dither.FLOYDSTEINBERG)


def slide(img, steps):
    """img is already at target size, steps are couples (dx, dy) to slide."""
    frames = []
    w,h = img.size
    for dx,dy in steps:
        # TODO: fill the image with a background color
        frame = Image.new(img.mode, img.size)
        frame.putpalette(img.getpalette())
        frame.paste(img, (dx, dy, w+dx, h+dy))
        frames.append(frame)
    return frames


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Generate an animation (slide in from the left, rotate, slide out)')
    parser.add_argument('--input', '-i', default='hip_src.png', help='path to source image')
    parser.add_argument('--output', '-o', nargs='?', default='hip_anim.webp')
    args = parser.parse_args()

    src = Image.open(args.input).convert('RGB')
    frames = []

    # Slide in
    to_slide = resize_dither(src)
    frames.extend(slide(to_slide, [(dx,0) for dx in range(-200, 0, 10)]))  # We don't draw the last one (no dx), it will be the rotation that does it

    # Rotate
    N = 20
    for i in range(N):
        frames.append(resize_dither(src.rotate(i*360/N)))

    # Slide out
    frames.extend(slide(to_slide, [(dx,0) for dx in range(0, 201, 10)]))

    # Save
    frames[0].save(
        args.output,
        lossless = True,
        append_images = frames[1:],
        duration = 50,
        loop = 0,
    )
