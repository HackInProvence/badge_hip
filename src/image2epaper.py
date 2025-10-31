#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Small script to convert source images to C-source buffers that can be compiled and directly sent to the e-Paper module.
"""

import argparse
from collections import namedtuple
import os
import sys

try:
    from PIL import Image, ImageSequence
except ImportError:
    print("This script requires the PIL library")
    sys.exit(1)


# The palette is a dict {col: value} where each value is in [0..4]
#  and lsb is the bitplane for pixels that represent the lsb (i.e. lsb[y*width/8 + x/8] & (7-(x%8)) == palette[image[x,y]] & 1)
#  and the same goes for msb.
# To rephrase: palette[image[x,y]] &1 goes to bit 7-(x%8) of lsb[y*width/8 + x/8] and
#              palette[image[x,y]] &2 goes to bit 7-(x%8) of msb[y*width/8 + x/8]
Frame = namedtuple('Frame', 'width height pal lsb msb')


def get_palette(frames):
    """
    Produces a map {col: val} where val is 0 to 3,
     and col are sorted so that the blackest color of img is 0, and the whitest is 3.
    Supports 2 or 4 colors.
    With 2 colors, only use val=0 and val=1.
    """
    # Collect all pixels values (either palette indexes or color values)
    colors = set()
    for img in frames:
        colors |= {col for n,col in img.getcolors()}

    # Now put an indicator of the grayness to the colors to be able to sort them
    # We hope that all layers have the same mode (either palette or RGB(A))
    if img.palette is None:
        # For RGB(A), we already have the colors, but we have to sum the channels to sort them
        colors = [(sum(col),col) for col in colors]  # example [(0, (0,0,0)), (150, (50,50,50)), ...]
    else:
        # For palette mode, we have to recover the color to sort them by grayness.
        pal = img.palette
        ml = len(pal.mode)
        colors = [(sum(pal.palette[i:i+ml]),icol) for icol,i in enumerate(range(0, len(pal.palette), ml))]  # example [(0,0), (12,1), (6,2), (15,3)]
    colors.sort()  # Now entry (_,i) are sorted by increasing whiteness, example [(0,0), (6,2), (12,1), (15,3)]
    return {c:i for i,(_,c) in enumerate(colors)}  # When pixels[x,y] is 2 this means that it should be color nb 1 (NOT TESTED)


def convert_frame(img, pal):
    pixels = img.load()

    # Pack bits (8 bits per bytes instead of 1 pixel per byte)
    # This is done along the X axis (pixel (x,y) is located at (x//8 & (1<<(7-(x%8))), y))
    # Hence the buffer is of size w8*height
    width,height = img.size
    if width > 200 or height > 200:  # We do this script only for this screen after all...
        eprint('WARNING: image does not fit in 200x200')
    w8 = width//8 + (1 if width%8 else 0)

    # Split the image in 2 planes: the MSB (put in the Red RAM) and LSB (put in B/W RAM)
    lsb, msb = bytearray(w8*height), bytearray(w8*height)
    for j in range(height):
        for i in range(width):
            val = pal[pixels[i,j]]
            lsb[j*w8 + i//8] |= (val &1) << (7-(i%8))
            msb[j*w8 + i//8] |= (val>>1) << (7-(i%8))
        # Fill the line with the back color, in case the width is not divisible by 8
        for i in range(width, w8*8):
            lsb[j*w8 + i//8] |= (back_color &1) << (7-(i%8))
            msb[j*w8 + i//8] |= (back_color>>1) << (7-(i%8))

    return Frame(width, height, pal, lsb, msb)  # It's not necessary anymore to store the palette in the Frame but hey... that's more work.


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Convert images using PIL to C source')
    parser.add_argument('image', help='path to source image')
    parser.add_argument('--output', '-o', nargs='?', default=None, help='output to this instead of stdout')
    parser.add_argument('--back-color', '-b', nargs='?', type=str, default='11', help='fill color if the image\'s width is not divisible by 8')
    args = parser.parse_args()

    eprint = lambda *args, **kwargs: print(*args, file=sys.stderr, **kwargs)
    if args.image == args.output:
        eprint('output should not be the same file as input')  # Avoids overwrites
        sys.exit(1)

    if args.back_color not in ('00', '01', '10', '11'):
        eprint('fill color should be 00, 01, 10, or 11.')
        sys.exit(1)
    back_color = int(args.back_color, 2)

    # Avoid special chars for the name of the buffer, but please don't push it (é is alnum for python but might not work in names)
    buffer_prefix,_ = os.path.splitext(os.path.basename(args.image))
    buffer_prefix = ''.join(c if c.isalnum() else '_' for c in buffer_prefix)
    if buffer_prefix[0].isnumeric():
        buffer_prefix = '_'+buffer_prefix

    header_name = f'_{buffer_prefix.upper()}_H'

    # In fact, the palette must be shared by all frames, otherwise the same color may have a different order in different frames,
    #  which would lead to frames suddenly changing colors (this happens when one of the darker colors is missing in the frame)
    img = Image.open(args.image)
    frames = [frimg.copy() for frimg in ImageSequence.Iterator(img)]
    pal = get_palette(frames)
    frames = [convert_frame(frimg, pal) for frimg in frames]

    n_colors = len(pal)
    if n_colors not in (2, 4):
        eprint('palette mode image should be 2 or 4 colors (Palette mode or Indexed colors)')
        eprint(f'RGB image should only have 2 or 4 colors (counted {n_colors})')
        sys.exit(1)

    # Open destination as late as possible to avoid overwrites
    if args.output is None:
        dest = sys.stdout
    else:
        dest = open(args.output, 'w')  # Yes, we don't close it, such rebels!
        #eprint('write to file', args.output)
    fprint = lambda *args, **kwargs: print(*args, file=dest, **kwargs)

    for frame in frames:
        width, height = frame.width, frame.height
        break  # If frames have different shapes, I'm sorry for you!
    w8 = width//8

    fprint(rf'''
/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* WARNING: THIS FILE WAS GENERATED BY {parser.prog} */

#ifndef {header_name}
#define {header_name}

#include <stddef.h>

/* This is the width of the generated buffer, not the width of the image */
#define {buffer_prefix}_width {w8}
#define {buffer_prefix}_height {height}
#define {buffer_prefix}_n_frames {len(frames)}

/* {parser.prog} transformed {args.image} in {n_colors//2} planes for {len(frames)} frames */
'''.strip())

    for iframe,frame in enumerate(frames):
        if len(frames) > 1:
            buffer_name = f'{buffer_prefix}_frame{iframe}'  # Still a prefix in fact
        else:
            buffer_name = buffer_prefix

        # Only output the LSB plane when we have 2 colors
        if n_colors == 2:
            fprint()
            fprint(f'const uint8_t {buffer_name}[] = {{')
            lines = []
            for j in range(0, w8*height, 16):
                lines.append('    ' + ', '.join(f'0x{v:02X}' for v in frame.lsb[j: j+16]))
            fprint(',\n'.join(lines) + '\n};')
        else:
            fprint()
            fprint(f'const uint8_t {buffer_name}_lsb[] = {{')
            lines = []
            for j in range(0, w8*height, 16):
                lines.append('    ' + ', '.join(f'0x{v:02X}' for v in frame.lsb[j: j+16]))
            fprint(',\n'.join(lines) + '\n};')
            fprint(f'const uint8_t {buffer_name}_msb[] = {{')
            lines = []
            for j in range(0, w8*height, 16):
                lines.append('    ' + ', '.join(f'0x{v:02X}' for v in frame.msb[j: j+16]))
            fprint(',\n'.join(lines) + '\n};')

        fprint()

    # Also gather frames in lists of buffers for accessibility
    if len(frames) > 1:
        if n_colors == 2:
            # I'm not sure of the meaning of a const [][]...
            fprint(f'const uint8_t * {buffer_prefix}[] = {{')
            fprint(',\n'.join(f'    {buffer_prefix}_frame{i}' for i in range(len(frames))) + '\n};')
        else:
            fprint(f'const uint8_t[][] {buffer_prefix}_lsb = {{')
            fprint(',\n'.join(f'    {buffer_prefix}_frame{i}_lsb' for i in range(len(frames))) + '\n};')
            fprint(f'const uint8_t[][] {buffer_prefix}_msb = {{')
            fprint(',\n'.join(f'    {buffer_prefix}_frame{i}_msb' for i in range(len(frames))) + '\n};')

    fprint(f'\n#endif /* {header_name} */')

    # For your information
    if len(frames) == 1 and n_colors == 2:
        eprint(f'converted image {width}x{height} with {n_colors} colors, buffer name is {buffer_prefix}')
    elif len(frames) == 1 and n_colors == 4:
        eprint(f'converted image {width}x{height} with {n_colors} colors, buffer names are {buffer_prefix}_lsb and {buffer_prefix}_msb')
    elif n_colors == 2:
        eprint(f'converted image {width}x{height} with {n_colors} colors and {len(frames)} frames, which are pointed by {buffer_prefix}')
    else:
        eprint(f'converted image {width}x{height} with {n_colors} colors and {len(frames)} frames, which are pointed by {buffer_prefix}_lsb and {buffer_prefix}_msb')
