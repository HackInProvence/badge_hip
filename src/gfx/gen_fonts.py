#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Generate gfx_fonts.c: 1 bit fonts for the gfx library, rendered from Aileron (the CC0 font bundled with Pillow).
The generated file is committed, so this is only needed to change the fonts: python3 gen_fonts.py > gfx_fonts.c

The Aileron bundled with Pillow has no accented letters, nor acute accent or diaeresis:
the accented letters are composed from the base letter and an accent (the acute is the mirrored grave accent).
Use --preview file.png to check the result.
"""

import argparse

from PIL import Image, ImageDraw, ImageFont, ImageOps

ASCII = [chr(c) for c in range(32, 127)]
# Accented letter: (base letter, accent)
COMPOSED = {
    'à': ('a', 'grave'), 'â': ('a', 'circ'), 'ç': ('c', 'cedilla'), 'é': ('e', 'acute'), 'è': ('e', 'grave'),
    'ê': ('e', 'circ'), 'ë': ('e', 'diaeresis'), 'î': ('ı', 'circ'), 'ï': ('ı', 'diaeresis'), 'ô': ('o', 'circ'),
    'ù': ('u', 'grave'), 'û': ('u', 'circ'), 'ü': ('u', 'diaeresis'),
    'À': ('A', 'grave'), 'É': ('E', 'acute'), 'È': ('E', 'grave'), 'Ê': ('E', 'circ'), 'Ç': ('C', 'cedilla'),
    # Italian, Spanish, German (src/menu/lang/it.po, es.po, de.po)
    'á': ('a', 'acute'), 'í': ('ı', 'acute'), 'ó': ('o', 'acute'), 'ú': ('u', 'acute'), 'ì': ('ı', 'grave'),
    'ò': ('o', 'grave'), 'ä': ('a', 'diaeresis'), 'ö': ('o', 'diaeresis'), 'ñ': ('n', 'tilde'),
    'Á': ('A', 'acute'), 'Í': ('I', 'acute'), 'Ó': ('O', 'acute'), 'Ú': ('U', 'acute'), 'Ì': ('I', 'grave'),
    'Ò': ('O', 'grave'), 'Ù': ('U', 'grave'), 'Ä': ('A', 'diaeresis'), 'Ö': ('O', 'diaeresis'),
    'Ü': ('U', 'diaeresis'), 'Ñ': ('N', 'tilde'),
}
# Drawn from other glyphs (glyph_image): the sharp s, the inverted ? and ! of Spanish, the degree sign
SPECIAL = ['ß', '¿', '¡', '°']
FONTS = {  # name: pixel size
    'gfx_font_small': 14,
    'gfx_font_medium': 18,
    'gfx_font_large': 26,
}


# Pixels of space added on each side of a glyph: the slash touched its neighbours ("9/32")
SIDE_SPACE = {'/': 1}


def ink_bbox(img):
    return img.getbbox() or (0, 0, 0, 0)


def render_char(font, ch, height):
    """Renders a char of the font, returns (1 bit image, advance in pixels as a float)."""
    if ch == 'ı':  # dotless i, missing too: crop the dot of the i
        img, adv = render_char(font, 'i', height)
        top = ink_bbox(render_char(font, 'x', height)[0])[1]
        ImageDraw.Draw(img).rectangle((0, 0, img.width, top - 1), fill=0)
        return img, adv
    adv = font.getlength(ch)
    left = font.getbbox(ch)[0]
    # A glyph that starts left of its origin (the '/' of Aileron) was cut there: shifted right, its spacing kept
    shift = max(0, -left) + SIDE_SPACE.get(ch, 0)
    width = max(font.getbbox(ch)[2], round(adv), 1) + shift
    img = Image.new('1', (width + SIDE_SPACE.get(ch, 0), height), 0)
    draw = ImageDraw.Draw(img)
    draw.fontmode = '1'  # No antialiasing
    draw.text((shift, 0), ch, font=font, fill=1)
    return img, adv + 2 * SIDE_SPACE.get(ch, 0)  # The others overlap their neighbour by their bearing, as in the font


def accent_image(font, accent, height):
    """Returns the cropped image of an accent."""
    if accent in ('grave', 'acute'):
        # Drawn: the '`' of the font is 1 or 2 pixels in the small sizes, its mirror (the acute) was the same blob
        n = max(3, round(font.size * 0.22))
        t = max(1, round(font.size / 14))
        img = Image.new('1', (n + t - 1, n), 0)
        draw = ImageDraw.Draw(img)
        for k in range(t):
            if accent == 'acute':
                draw.line((k, n - 1, n - 1 + k, 0), fill=1)
            else:
                draw.line((k, 0, n - 1 + k, n - 1), fill=1)
        return img
    if accent == 'circ':
        # Drawn: the '^' of the font is a big caret (maths), it floated high above the letter ("rôles")
        w = max(5, round(font.size * 0.36)) | 1
        h = (w + 1) // 2
        t = max(1, round(font.size / 14))
        img = Image.new('1', (w, h + t - 1), 0)
        draw = ImageDraw.Draw(img)
        for k in range(t):
            draw.line((0, h - 1 + k, w // 2, k), fill=1)
            draw.line((w // 2, k, w - 1, h - 1 + k), fill=1)
        return img
    if accent == 'cedilla':
        img = render_char(font, ',', height)[0]
        return img.crop(ink_bbox(img))
    if accent == 'tilde':
        # Drawn: the '~' of the font is wide and low (maths); a small wave above the letter
        w = max(5, round(font.size * 0.36)) | 1
        t = max(1, round(font.size / 14))
        img = Image.new('1', (w, 2 + t), 0)
        draw = ImageDraw.Draw(img)
        q = w // 4
        for k in range(t):
            draw.line([(0, 1 + k), (q, k), (2 * q, 1 + k), (3 * q, 2 + k - 1), (w - 1, k)], fill=1)
        return img
    if accent == 'diaeresis':
        dot = max(1, round(font.size / 10))
        img = Image.new('1', (3*dot + dot, dot), 0)
        ImageDraw.Draw(img).rectangle((0, 0, dot-1, dot-1), fill=1)
        ImageDraw.Draw(img).rectangle((3*dot, 0, 4*dot-1, dot-1), fill=1)
        return img
    raise ValueError(accent)


def compose(font, ch, height):
    base_ch, accent = COMPOSED[ch]
    img, adv = render_char(font, base_ch, height)
    acc = accent_image(font, accent, height)
    if acc.width > img.width:
        # Narrow letter (i): widen the glyph so that the accent fits, the base letter is centered
        pad = acc.width - img.width
        wide = Image.new('1', (acc.width, height), 0)
        wide.paste(img, (pad // 2, 0))
        img, adv = wide, adv + pad
    left, top, right, bottom = ink_bbox(img)
    x = (left + right - acc.width) // 2
    gap = max(1, round(font.size / 14))
    if accent == 'cedilla':
        y = bottom - 1  # Hangs under the letter
        x = (left + right - acc.width) // 2 + 1
    else:
        # Accents are small: shrink the circumflex and the grave/acute if they are too big for the space
        y = max(0, top - gap - acc.height)
    img.paste(acc, (x, y), acc)
    return img, adv


def glyph_image(font, ch, height):
    if ch in COMPOSED:
        return compose(font, ch, height)
    if ch in '¿¡':
        # The ? and the ! turned upside down, hanging from the x height down under the baseline
        img, adv = render_char(font, '?' if ch == '¿' else '!', height)
        ink = img.crop(ink_bbox(img)).rotate(180)
        x_top = ink_bbox(render_char(font, 'x', height)[0])[1]
        baseline = ink_bbox(render_char(font, 'x', height)[0])[3]
        y = min(height - ink.height, x_top + max(0, (baseline - x_top) - ink.height + (height - baseline) // 2 + 1))
        out = Image.new('1', (img.width, height), 0)
        out.paste(ink, (ink_bbox(img)[0], max(0, y)))
        return out, adv
    if ch == 'ß':
        # Drawn from the B (Aileron has no sharp s): the top left corner rounded, the bottom bowl open on the stem,
        # the stem a little under the baseline
        img, adv = render_char(font, 'B', height)
        left, top, right, bottom = ink_bbox(img)
        t = max(1, round(font.size / 9))  # Stroke
        r = max(1, round(font.size / 9))
        draw = ImageDraw.Draw(img)
        draw.rectangle((left, top, left + r - 1, top + r - 1), fill=0)  # Rounded corner
        draw.rectangle((left + r, top, left + r, top), fill=1)
        # The bottom bar from the stem to the bowl removed: the bowl ends free
        draw.rectangle((left + t, bottom - t + 1, left + t + max(1, (right - left) // 4), bottom), fill=0)
        # The middle bar to the stem removed: an open 3 on a stem
        mid = (top + bottom) // 2
        draw.rectangle((left + t, mid - t // 2, left + t + max(1, (right - left) // 5), mid + (t - 1) // 2), fill=0)
        return img, adv
    if ch == '°':
        img, _ = render_char(font, 'o', height)
        o = img.crop(ink_bbox(img))
        o = o.resize((max(3, o.width*2//3), max(3, o.height*2//3)), Image.NEAREST)
        top = ink_bbox(render_char(font, 'H', height)[0])[1]
        img = Image.new('1', (o.width + 2, height), 0)
        img.paste(o, (1, top))
        return img, o.width + 2
    return render_char(font, ch, height)


def render(name, size, preview):
    font = ImageFont.load_default(size=size)
    ascent, descent = font.getmetrics()
    height = ascent + descent
    glyphs, bitmap = [], bytearray()
    for ch in ASCII + list(COMPOSED) + SPECIAL:
        img, adv = glyph_image(font, ch, height)
        # Rows are packed on whole bytes, bit 7 is the leftmost pixel, 1 = ink
        w8 = (img.width + 7) // 8
        offset = len(bitmap)
        px = img.load()
        for y in range(height):
            row = bytearray(w8)
            for x in range(img.width):
                if px[x, y]:
                    row[x // 8] |= 0x80 >> (x % 8)
            bitmap += row
        glyphs.append((ord(ch), round(adv * 16), img.width, offset))
        preview.append((ch, img))
    lines = [f'/* {name}: Aileron {size}px, {len(glyphs)} glyphs */',
             f'static const gfx_glyph_t {name}_glyphs[] = {{']
    lines += [f'    {{0x{cp:04X}, {adv}, {w}, {off}}},' for cp, adv, w, off in glyphs]
    lines.append('};')
    lines.append(f'static const uint8_t {name}_bitmap[] = {{')
    for i in range(0, len(bitmap), 16):
        lines.append('    ' + ', '.join(f'0x{b:02X}' for b in bitmap[i:i+16]) + ',')
    lines.append('};')
    lines.append(f'const gfx_font_t {name} = {{{height}, {len(glyphs)}, {name}_glyphs, {name}_bitmap}};')
    return '\n'.join(lines)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Generate gfx_fonts.c on stdout')
    parser.add_argument('--preview', default=None, help='also save the glyphs of all fonts in this image')
    args = parser.parse_args()

    print('/* WARNING: THIS FILE WAS GENERATED BY gen_fonts.py */')
    print('/* Font: Aileron by Sora Sagano (dotcolon.net), released under CC0 (public domain), as bundled with Pillow */')
    print()
    print('#include "gfx.h"')
    previews = []
    for name, size in FONTS.items():
        preview = []
        print()
        print(render(name, size, preview))
        previews.append(preview)

    if args.preview:
        w = max(sum(img.width + 2 for _, img in p) for p in previews)
        h = sum(p[0][1].height + 4 for p in previews)
        sheet = Image.new('1', (w, h), 1)
        y = 0
        for p in previews:
            x = 0
            for _, img in p:
                sheet.paste(ImageOps.invert(img.convert('L')).convert('1'), (x, y))
                x += img.width + 2
            y += p[0][1].height + 4
        sheet.save(args.preview)
