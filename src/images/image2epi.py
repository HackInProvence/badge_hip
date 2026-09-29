#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Convert pictures (JPEG, PNG, ...) to .EPI images for the SD card of the badge (IMAGES directory):
200x200 pixels, 4 gray levels, shown by the screensaver (menu "Réglages").

The picture is resized to cover 200x200 (then center cropped, or letterboxed in white with --fit),
converted to grays, its contrast stretched (or its histogram equalized with --equalize),
then dithered (Floyd-Steinberg) to the 4 gray levels of the e-Paper.
Pictures already made for the screen (200x200 with at most 4 colors) are converted exactly.

.EPI format (little endian):
- 16 bytes header: "EPIMAGE1" magic, uint16 width (200), uint16 height (200), uint16 bits per pixel (1 or 2), uint16 0,
- the LSB plane then (for 2 bits per pixel) the MSB plane, width*height/8 bytes each,
  packed like image2epaper.py (bit 7 = leftmost pixel, row major); the gray level is MSB*2+LSB, 0 = black, 3 = white.
"""

import argparse
import os
import struct
import sys

try:
    from PIL import Image, ImageOps
except ImportError:
    print('This script requires the PIL library (pip install pillow)')
    sys.exit(1)

WIDTH = HEIGHT = 200
MAGIC = b'EPIMAGE1'
GRAYS = [0, 85, 170, 255]


def to_gray(img, fit):
    """Grayscale 200x200, alpha composited on white."""
    img = ImageOps.exif_transpose(img)
    if img.mode in ('RGBA', 'LA', 'P'):
        img = img.convert('RGBA')
        background = Image.new('RGBA', img.size, 'white')
        img = Image.alpha_composite(background, img)
    img = img.convert('L')
    if fit:
        img = ImageOps.pad(img, (WIDTH, HEIGHT), method=Image.Resampling.LANCZOS, color=255)
    else:
        img = ImageOps.fit(img, (WIDTH, HEIGHT), method=Image.Resampling.LANCZOS)
    return img


def exact_levels(img):
    """For images already made for the screen (200x200, at most 4 colors, e.g. src/tests/imgs/*_4g.png):
    the colors sorted by brightness are the gray levels (no resizing, no dithering). Returns None for other images."""
    if img.size != (WIDTH, HEIGHT):
        return None
    rgb = img.convert('RGB')
    colors = rgb.getcolors(4)
    if colors is None:
        return None
    by_brightness = sorted((sum(c), c) for _, c in colors)
    n = len(by_brightness)
    level = {c: (0 if n == 1 else round(i * 3 / (n - 1))) for i, (_, c) in enumerate(by_brightness)}
    px = rgb.load()
    return [[level[px[x, y]] for x in range(WIDTH)] for y in range(HEIGHT)]


def dither(gray, levels, contrast_cutoff, equalize=False):
    """Returns the gray level (0..levels-1) of each pixel, as a list of rows."""
    gray = ImageOps.equalize(gray) if equalize else ImageOps.autocontrast(gray, cutoff=contrast_cutoff)
    palette_img = Image.new('P', (1, 1))
    grays = GRAYS if levels == 4 else [0, 255]
    pal = []
    for g in grays:
        pal += [g, g, g]
    palette_img.putpalette(pal + [0] * (768 - len(pal)))
    q = gray.convert('RGB').quantize(palette=palette_img, dither=Image.Dither.FLOYDSTEINBERG)
    px = q.load()
    return [[px[x, y] for x in range(WIDTH)] for y in range(HEIGHT)]


def pack(levels_rows, bit):
    out = bytearray(WIDTH * HEIGHT // 8)
    for y, row in enumerate(levels_rows):
        for x, v in enumerate(row):
            if (v >> bit) & 1:
                out[y * (WIDTH // 8) + x // 8] |= 0x80 >> (x % 8)
    return bytes(out)


def convert(src, dst, fit, bw, contrast_cutoff, preview, equalize=False):
    levels = 2 if bw else 4
    img = Image.open(src)
    rows = None if bw else exact_levels(img)
    if rows is None:
        rows = dither(to_gray(img, fit), levels, contrast_cutoff, equalize)
    with open(dst, 'wb') as out:
        out.write(MAGIC + struct.pack('<HHHH', WIDTH, HEIGHT, 1 if bw else 2, 0))
        out.write(pack(rows, 0))
        if not bw:
            out.write(pack(rows, 1))
    if preview:
        grays = [0, 255] if bw else GRAYS
        img = Image.new('L', (WIDTH, HEIGHT))
        img.putdata([grays[v] for row in rows for v in row])
        img.save(preview)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Convert pictures to .EPI (200x200, 4 grays) for the badge')
    parser.add_argument('inputs', nargs='+', help='pictures (any format read by PIL)')
    parser.add_argument('--output-dir', '-o', default='.', help='where to write the .epi files')
    parser.add_argument('--fit', action='store_true', help='letterbox the whole picture instead of cropping the center')
    parser.add_argument('--bw', action='store_true', help='black and white only (1 bit per pixel)')
    parser.add_argument('--contrast', type=float, default=1.0, help='percentage of dark/light pixels clipped to stretch the contrast')
    parser.add_argument('--equalize', action='store_true', help='equalize the histogram instead (for pictures with few contrast)')
    parser.add_argument('--preview', action='store_true', help='also write a .png preview of the result next to each .epi')
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)
    for src in args.inputs:
        name = os.path.splitext(os.path.basename(src))[0]
        dst = os.path.join(args.output_dir, name + '.epi')
        convert(src, dst, args.fit, args.bw, args.contrast, os.path.join(args.output_dir, name + '.png') if args.preview else None,
                args.equalize)
        print(f'{dst}: {os.path.getsize(dst)} bytes', file=sys.stderr)
