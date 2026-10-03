#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
A color image with two hidden patterns: one appears under red light, the other under blue light.

The principle (see docs/en/color_images.md): under a red light (or through a red filter) only the red part of each
color counts: red, magenta, yellow and white look light, cyan, blue, green and black look dark. Under a blue light
only the blue part counts: blue, cyan, magenta and white look light, yellow, red, green and black look dark. So the
image is made of colored cells (dots, blobs or stripes):
- the red part of a cell says whether it is in pattern A (dark under red light) or not;
- its blue part says whether it is in pattern B (dark under blue light) or not, independently;
- its green part (magenta ink on paper), which neither light sees, carries the camouflage: it keeps every cell at
  about the same lightness for the eye (so the patterns only show as faint changes of hue), plus soft random blobs
  of light and dark, and an optional decoy text that the eye reads first and that vanishes under both lights.

Screen mode (default, sRGB): to look at on a PC or a phone through a red or a blue filter (a gel, a cellophane, the
lenses of red/blue 3D glasses). Print mode (--print): the colors are computed as inks (cyan, magenta, yellow, no
black) and corrected for the unwanted absorption of the inks (magenta absorbs some blue light); the paper is then
lit by the red or the blue LEDs of the badges (Admin > Cicada LEDs, Fixed) in a dark room.

    python tools/color_reveal.py --text-red SECSEA --image-blue @cicada -o reveal.png --preview
    python tools/color_reveal.py --text-red SECSEA --text-blue "HIP 2026" --style blobs --seed 7 -o hip.png
    python tools/color_reveal.py --text-red SECSEA --text-blue "HIP|2026" --print --size 150x100mm --dpi 300 \\
        --cmyk hip_cmyk.tif -o hip_print.png --preview
    python tools/color_reveal.py --image-red logo.png --text-blue "FLAG{...}" --decoy "NOTHING HERE" -o ctf.png

--text-*: a text ("|" or a new line: next line); --image-*: a black and white picture (dark = the pattern; --invert
for the opposite), or @cicada, a drawn cicada. Only one of the two colors may be given.
--preview also writes <output>_preview.png: the image without filter, under red light and under blue light, side by
side (a simulation: the real result depends on the filter, the LEDs, the screen, the inks and the paper).

Needs Pillow and numpy (pip install pillow numpy).
"""

import argparse
import math
import os
import sys

try:
    import numpy as np
    from PIL import Image, ImageDraw, ImageFilter, ImageFont
except ImportError:
    sys.exit('Needs Pillow and numpy: pip install pillow numpy')

FONTS = ['DejaVuSans-Bold.ttf', 'arialbd.ttf', 'Arial Bold.ttf', 'LiberationSans-Bold.ttf', 'verdanab.ttf']
FONT_DIRS = ['', os.path.join(os.environ.get('WINDIR', 'C:\\Windows'), 'Fonts'), '/usr/share/fonts/truetype/dejavu',
             '/usr/share/fonts/TTF', '/Library/Fonts', '/System/Library/Fonts/Supplemental']


# Print mode: the share of light each ink lets through (full coverage, on white paper), for the eye (linear sRGB
# of the process inks) and for the LEDs of the badges (red ~625 nm, blue ~465 nm). Rough values from the usual
# reflectance curves of process inks: magenta lets red through but absorbs part of the blue, cyan absorbs red
# and a little blue, yellow absorbs blue only.
INK_EYE = {'c': np.array([0.00, 0.42, 0.86]), 'm': np.array([0.84, 0.00, 0.26]), 'y': np.array([1.00, 0.89, 0.00])}
INK_RED = {'c': 0.10, 'm': 0.92, 'y': 0.95}
INK_BLUE = {'c': 0.85, 'm': 0.55, 'y': 0.08}
PAPER = 0.92  # A white office paper
M_MAX = 0.55  # Most magenta (the camouflage): more would darken the blue view too much


# ---------------------------------------------------------------------------------------------------------- patterns

def find_font(size):
    for name in FONTS:
        for d in FONT_DIRS:
            try:
                return ImageFont.truetype(os.path.join(d, name) if d else name, size)
            except OSError:
                pass
    return ImageFont.load_default(size)


def fit(img, w, h, margin):
    """Paste a mask (L, 255 = pattern) in the middle of a w x h mask, as large as possible inside the margins."""
    bbox = img.getbbox()
    if bbox is None:
        return Image.new('L', (w, h), 0)
    img = img.crop(bbox)
    mw, mh = int(w * (1 - 2 * margin)), int(h * (1 - 2 * margin))
    k = min(mw / img.width, mh / img.height)
    img = img.resize((max(1, int(img.width * k)), max(1, int(img.height * k))), Image.LANCZOS)
    out = Image.new('L', (w, h), 0)
    out.paste(img, ((w - img.width) // 2, (h - img.height) // 2))
    return out


def text_mask(text, w, h, margin=0.08):
    lines = text.replace('|', '\n').split('\n')
    font = find_font(400)
    draw = ImageDraw.Draw(Image.new('L', (1, 1)))
    boxes = [draw.textbbox((0, 0), line or ' ', font=font) for line in lines]
    gap = 60
    tw = max(b[2] - b[0] for b in boxes) + 40
    th = sum(b[3] - b[1] for b in boxes) + gap * (len(lines) - 1) + 40
    img = Image.new('L', (tw, th), 0)
    d = ImageDraw.Draw(img)
    y = 20
    for line, b in zip(lines, boxes):
        d.text(((tw - (b[2] - b[0])) // 2 - b[0], y - b[1]), line, fill=255, font=font)
        y += b[3] - b[1] + gap
    return fit(img, w, h, margin)


def rotated_ellipse(draw, cx, cy, rx, ry, angle, fill):
    a = math.radians(angle)
    pts = []
    for i in range(72):
        t = 2 * math.pi * i / 72
        x, y = rx * math.cos(t), ry * math.sin(t)
        pts.append((cx + x * math.cos(a) - y * math.sin(a), cy + x * math.sin(a) + y * math.cos(a)))
    draw.polygon(pts, fill=fill)


def cicada_mask(w, h, margin=0.06):
    """A cicada seen from above, wings open (the mascot of the badges)."""
    img = Image.new('L', (1000, 1000), 0)
    d = ImageDraw.Draw(img)
    # Wings: a long front pair and a short back pair, with a few veins left empty
    for side in (-1, 1):
        rotated_ellipse(d, 500 + side * 215, 470, 250, 85, side * 28, 255)
        rotated_ellipse(d, 500 + side * 165, 600, 170, 62, side * 52, 255)
        for k in range(3):
            a = math.radians(side * 28)
            x0, y0 = 500 + side * 70, 430
            d.line([(x0, y0 + 25 * k), (x0 + side * 360 * abs(math.cos(a)), y0 + 25 * k + 360 * abs(math.sin(a)))],
                   fill=0, width=9)
    # Body: head with two eyes, thorax, abdomen
    d.ellipse([430, 240, 570, 320], fill=255)
    d.ellipse([395, 245, 445, 300], fill=255)
    d.ellipse([555, 245, 605, 300], fill=255)
    d.ellipse([410, 300, 590, 470], fill=255)
    d.polygon([(420, 440), (580, 440), (540, 760), (500, 800), (460, 760)], fill=255)
    for k in range(4):  # The rings of the abdomen
        y = 520 + 60 * k
        d.line([(420 + 15 * k, y), (580 - 15 * k, y)], fill=0, width=10)
    return fit(img, w, h, margin)


def image_mask(path, w, h, invert, margin=0.06):
    if path == '@cicada':
        return cicada_mask(w, h, margin)
    img = Image.open(path)
    if img.mode in ('RGBA', 'LA', 'P'):
        img = img.convert('RGBA')
        bg = Image.new('RGBA', img.size, (255, 255, 255, 255))
        img = Image.alpha_composite(bg, img)
    img = img.convert('L').point(lambda v: 255 if (v < 128) != invert else 0)
    return fit(img, w, h, margin)


def pattern(text, image, w, h, invert):
    if text and image:
        sys.exit('Give a text or an image for a color, not both')
    if text:
        return text_mask(text, w, h)
    if image:
        return image_mask(image, w, h, invert)
    return Image.new('L', (w, h), 0)


# ------------------------------------------------------------------------------------------------------------- cells

def smooth_noise(w, h, scale, rng):
    """Soft random field in 0..1 with blobs of about `scale` pixels."""
    gw, gh = max(2, int(w / scale) + 2), max(2, int(h / scale) + 2)
    small = Image.fromarray((rng.random((gh, gw)) * 255).astype(np.uint8))
    return np.asarray(small.resize((w, h), Image.BICUBIC), dtype=np.float32) / 255


def cells_dots(w, h, cell, rng):
    """Ishihara-like dots of several sizes; -1 between the dots."""
    labels = np.full((h, w), -1, np.int32)
    rmin, rmax = cell * 0.35, cell * 0.85
    grid = rmax * 2
    gw, gh = int(w / grid) + 1, int(h / grid) + 1
    buckets = {}
    n = 0
    for r in (rmax, (rmax + rmin) / 2, rmin):  # Big dots first, the small ones fill the gaps
        tries = int(w * h / (r * r) * 3)
        for _ in range(tries):
            rr = r * (0.85 + 0.3 * rng.random())
            x, y = rng.random() * w, rng.random() * h
            gx, gy = int(x / grid), int(y / grid)
            ok = True
            for i in range(gx - 1, gx + 2):
                for j in range(gy - 1, gy + 2):
                    for (ox, oy, orr) in buckets.get((i, j), ()):
                        if (ox - x) ** 2 + (oy - y) ** 2 < (orr + rr + 1.2) ** 2:
                            ok = False
                            break
                    if not ok:
                        break
                if not ok:
                    break
            if not ok:
                continue
            buckets.setdefault((gx, gy), []).append((x, y, rr))
            x0, x1 = max(0, int(x - rr)), min(w, int(x + rr) + 2)
            y0, y1 = max(0, int(y - rr)), min(h, int(y + rr) + 2)
            yy, xx = np.mgrid[y0:y1, x0:x1]
            inside = (xx + 0.5 - x) ** 2 + (yy + 0.5 - y) ** 2 <= rr * rr
            labels[y0:y1, x0:x1][inside] = n
            n += 1
    return labels, n


def cells_blobs(w, h, cell, rng):
    """Irregular blobs: a Voronoi of jittered seeds, the coordinates bent by a soft noise."""
    s = cell * 1.1
    gw, gh = int(w / s) + 3, int(h / s) + 3
    sx = (np.arange(gw)[None, :] - 1 + rng.random((gh, gw))) * s
    sy = (np.arange(gh)[:, None] - 1 + rng.random((gh, gw))) * s
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    xx += (smooth_noise(w, h, s * 2.5, rng) - 0.5) * s * 1.2
    yy += (smooth_noise(w, h, s * 2.5, rng) - 0.5) * s * 1.2
    cx = np.clip((xx / s).astype(np.int32) + 1, 1, gw - 2)
    cy = np.clip((yy / s).astype(np.int32) + 1, 1, gh - 2)
    best = np.full((h, w), np.inf, np.float32)
    labels = np.zeros((h, w), np.int32)
    for dj in (-1, 0, 1):
        for di in (-1, 0, 1):
            j, i = cy + dj, cx + di
            dist = (sx[j, i] - xx) ** 2 + (sy[j, i] - yy) ** 2
            closer = dist < best
            best[closer] = dist[closer]
            labels[closer] = (j * gw + i)[closer]
    uniq, labels = np.unique(labels, return_inverse=True)
    return labels.reshape(h, w).astype(np.int32), len(uniq)


def cells_stripes(w, h, cell, rng):
    """Wavy stripes of random widths, cut into dashes of random lengths."""
    a = rng.random() * math.pi
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    v = -xx * math.sin(a) + yy * math.cos(a)
    u = xx * math.cos(a) + yy * math.sin(a) + np.sin(v / (cell * 4)) * cell * 0.8
    u -= u.min()
    widths = cell * (0.45 + 0.7 * rng.random(int(u.max() / (cell * 0.45)) + 2))
    band = np.searchsorted(np.cumsum(widths), u)
    nb = len(widths) + 1
    length = cell * (1 + 2 * rng.random(nb))
    offset = rng.random(nb) * 100 * cell
    seg = np.floor((v - v.min() + offset[band]) / length[band]).astype(np.int64)
    uniq, labels = np.unique(band.astype(np.int64) * 100000 + seg, return_inverse=True)
    return labels.reshape(h, w).astype(np.int32), len(uniq)


# ------------------------------------------------------------------------------------------------------------ colors

def lin_to_srgb(v):
    v = np.clip(np.asarray(v, np.float64), 0, 1)
    return np.where(v <= 0.0031308, v * 12.92, 1.055 * v ** (1 / 2.4) - 0.055)


def ink_views(c, m, y):
    """Print: linear RGB seen by the eye under white light, and the reflectance under the red and the blue LEDs."""
    eye = PAPER * np.ones(c.shape + (3,))
    for cov, ink in ((c, 'c'), (m, 'm'), (y, 'y')):
        eye *= 1 - cov[..., None] * (1 - INK_EYE[ink])
    red = PAPER * (1 - c * (1 - INK_RED['c'])) * (1 - m * (1 - INK_RED['m'])) * (1 - y * (1 - INK_RED['y']))
    blue = PAPER * (1 - c * (1 - INK_BLUE['c'])) * (1 - m * (1 - INK_BLUE['m'])) * (1 - y * (1 - INK_BLUE['y']))
    return eye, red, blue


def screen_colors(tr, tb, level):
    """Screen: the red and blue parts as asked, the green part (seen by neither light) is the camouflage."""
    return np.stack([tr, level, tb], -1)


def print_inks(tr, tb, level):
    """Print: magenta is the camouflage; cyan from the red target and yellow from the blue target, each corrected
    for the unwanted absorption of the other inks (a few rounds of a fixed point)."""
    m = M_MAX * level
    c = np.zeros_like(tr)
    y = np.zeros_like(tr)
    for _ in range(5):
        rest = PAPER * (1 - m * (1 - INK_RED['m'])) * (1 - y * (1 - INK_RED['y']))
        c = np.clip((1 - tr / rest) / (1 - INK_RED['c']), 0, 1)
        rest = PAPER * (1 - c * (1 - INK_BLUE['c'])) * (1 - m * (1 - INK_BLUE['m']))
        y = np.clip((1 - tb / rest) / (1 - INK_BLUE['y']), 0, 1)
    return c, m, y


def build(args, w, h):
    rng = np.random.default_rng(args.seed)
    a = np.asarray(pattern(args.text_red, args.image_red, w, h, args.invert), np.float32) / 255
    b = np.asarray(pattern(args.text_blue, args.image_blue, w, h, args.invert), np.float32) / 255
    decoy = np.zeros((h, w), np.float32)
    if args.decoy:
        decoy = np.asarray(text_mask(args.decoy, w, h, 0.12).filter(ImageFilter.GaussianBlur(2)), np.float32) / 255

    cell = args.cell or max(6, int(min(w, h) / 60))
    labels, n = {'dots': cells_dots, 'blobs': cells_blobs, 'stripes': cells_stripes}[args.style](w, h, cell, rng)
    valid = labels >= 0
    lab = labels[valid]
    count = np.maximum(np.bincount(lab, minlength=n), 1)
    a_on = np.bincount(lab, weights=a[valid], minlength=n) / count > 0.5
    b_on = np.bincount(lab, weights=b[valid], minlength=n) / count > 0.5
    # Camouflage noise: a few cells take the other state (they blur the hue patterns for the eye)
    a_on ^= rng.random(n) < args.noise
    b_on ^= rng.random(n) < args.noise
    blobs = smooth_noise(w, h, cell * 6, rng)
    blob = np.bincount(lab, weights=blobs[valid], minlength=n) / count
    dec = np.bincount(lab, weights=decoy[valid], minlength=n) / count

    gap = min(0.95, (0.9 if args.print else 0.75) * args.strength)  # Share of the light lost by the pattern
    jit = rng.integers(-2, 3, (2, n)) * 0.04  # Discrete steps: fewer colors, smaller PNG
    # Red and blue targets: the sRGB value of the part on a screen, the reflectance under the LED on paper (the
    # blue one lower: room to correct the blue light taken by the magenta)
    offs = (0.80, 0.62) if args.print else (0.85, 0.85)
    targets, gaps = [], []
    for off, is_on, j in zip(offs, (a_on, b_on), jit):
        # Haze: soft wide blobs of red and of blue, a part of the contrast of the patterns but without edges: the
        # eye under a light still reads the sharp edges of the pattern, the naked eye sees hues everywhere
        field = smooth_noise(w, h, cell * 9, rng)
        haze = (np.bincount(lab, weights=field[valid], minlength=n) / count - 0.5) * 2 * args.haze * gap * off
        off_h = off * (1 - args.haze * gap * 0.5)
        on_h = off * (1 - gap) + args.haze * gap * off * 0.5
        targets.append(np.clip(np.where(is_on, on_h, off_h) + j + haze, 0, 1))
        gaps.append(np.array([off_h]))
    tr, tb = targets
    # Camouflage (green part / magenta ink): random per cell, soft blobs, the decoy text stronger
    level = 0.5 * rng.integers(0, 5, n) / 4 + 0.5 * np.round(blob * 4) / 4
    if args.decoy:  # The decoy in full camouflage color, the rest lighter so that it stands out
        level = np.clip(0.55 * level * (1 - dec) + dec, 0, 1)
    if not args.print:
        level = 1 - level  # Screen: less green = darker for the eye
    gap_state = (gaps[0], gaps[1], np.array([0.0 if args.print else 1.0]))
    if args.print:
        c, m, yk = print_inks(tr, tb, level)
        gc, gm, gy = print_inks(*gap_state)
        inks = np.zeros((h, w, 3))
        inks[valid] = np.stack([c, m, yk], -1)[lab]
        inks[~valid] = [gc[0], gm[0], gy[0]]
        eye, red, blue = ink_views(inks[..., 0], inks[..., 1], inks[..., 2])
        rgb = lin_to_srgb(eye / PAPER)  # The paper white is the white of the file
        return rgb, lin_to_srgb(red / PAPER), lin_to_srgb(blue / PAPER), inks  # Views as seen (sRGB)
    cols = screen_colors(tr, tb, level)
    rgb = np.zeros((h, w, 3))
    rgb[valid] = cols[lab]
    rgb[~valid] = screen_colors(*gap_state)[0]
    return rgb, rgb[..., 0], rgb[..., 2], None


# ------------------------------------------------------------------------------------------------------------ output

def to_image(rgb):
    return Image.fromarray(np.round(np.clip(rgb, 0, 1) * 255).astype(np.uint8), 'RGB')


def save_png(img, path, dpi=None, colors=256):
    """A palette PNG: the cells are flat colors, so it stays small."""
    q = img.quantize(colors=colors, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    kw = {'optimize': True}
    if dpi:
        kw['dpi'] = (dpi, dpi)
    q.save(path, **kw)


def tinted(view, tint):
    """What the eye sees under a colored light: the view in that color (a little lifted to stay readable)."""
    v = np.clip(view, 0, 1)[..., None]
    return to_image(v * np.array(tint))


def preview(rgb, red, blue, path, width, labels):
    panels = [to_image(rgb), tinted(red, (1.0, 0.32, 0.26)), tinted(blue, (0.32, 0.48, 1.0))]
    h0, w0 = rgb.shape[:2]
    k = min(1.0, width / (3 * w0 + 40))
    pw, ph = int(w0 * k), int(h0 * k)
    font = find_font(max(12, int(ph / 14)))
    bar = int(font.size * 1.6)
    out = Image.new('RGB', (3 * pw + 40, ph + bar + 10), (32, 32, 32))
    d = ImageDraw.Draw(out)
    for i, (p, label) in enumerate(zip(panels, labels)):
        x = 10 + i * (pw + 10)
        out.paste(p.resize((pw, ph), Image.LANCZOS), (x, bar))
        d.text((x, bar // 6), label, fill=(235, 235, 235), font=font)
    save_png(out, path)


def parse_size(text, dpi):
    t = text.lower().replace(' ', '')
    mm = t.endswith('mm')
    try:
        w, h = (float(v) for v in t.rstrip('m').split('x'))
    except ValueError:
        sys.exit('--size: WxH in pixels (1200x600) or in millimetres (150x100mm)')
    if mm:
        w, h = w / 25.4 * dpi, h / 25.4 * dpi
    return int(round(w)), int(round(h))


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--text-red', help='text that appears under red light ("|": new line)')
    p.add_argument('--text-blue', help='text that appears under blue light')
    p.add_argument('--image-red', help='black and white picture that appears under red light (@cicada: a cicada)')
    p.add_argument('--image-blue', help='black and white picture that appears under blue light (@cicada: a cicada)')
    p.add_argument('--invert', action='store_true', help='pictures: the light parts are the pattern')
    p.add_argument('--decoy', help='decoy text seen by the naked eye, gone under both lights')
    p.add_argument('-o', '--output', default='color_reveal.png', help='output PNG (default: color_reveal.png)')
    p.add_argument('--size', help='WxH in pixels, or WxHmm with --dpi (default: 1200x600, print: 150x100mm)')
    p.add_argument('--print', action='store_true', help='colors for printing (CMY inks, lit by the badge LEDs)')
    p.add_argument('--dpi', type=int, default=300, help='print: resolution (default: 300)')
    p.add_argument('--cmyk', help='print: also write the inks as a CMYK TIFF (K = 0), for a print shop')
    p.add_argument('--style', choices=('dots', 'blobs', 'stripes'), default='dots', help='camouflage (default: dots)')
    p.add_argument('--cell', type=int, help='size of the dots / blobs / stripes in pixels (default: about 1/55 of '
                                            'the height)')
    p.add_argument('--strength', type=float, default=0.7, help='0.3..1: contrast of the hidden patterns (lower: '
                                                               'better hidden, harder to read; default: 0.7)')
    p.add_argument('--noise', type=float, default=0.06, help='share of cells of the other state, 0..0.2 '
                                                             '(default: 0.06)')
    p.add_argument('--haze', type=float, default=0.25, help='0..1: soft blobs of red and blue over the image, '
                                                           'against the eye (default: 0.25)')
    p.add_argument('--seed', type=int, help='random seed (the same seed gives the same image)')
    p.add_argument('--preview', action='store_true', help='also write <output>_preview.png: simulated views')
    p.add_argument('--preview-width', type=int, default=1200, help='width of the preview (default: 1200)')
    args = p.parse_args()

    if not (args.text_red or args.image_red or args.text_blue or args.image_blue):
        p.error('give at least --text-red / --image-red or --text-blue / --image-blue')
    if not 0.1 <= args.strength <= 1:
        p.error('--strength: from 0.1 to 1')
    args.noise = min(max(args.noise, 0.0), 0.3)
    if args.cmyk and not args.print:
        p.error('--cmyk needs --print')
    w, h = parse_size(args.size or ('150x100mm' if args.print else '1200x600'), args.dpi)
    if not (32 <= w <= 8000 and 32 <= h <= 8000):
        p.error('--size: from 32 to 8000 pixels per side')

    rgb, red, blue, inks = build(args, w, h)
    save_png(to_image(rgb), args.output, args.dpi if args.print else None)
    print('%s: %d x %d px%s, %d bytes' % (args.output, w, h, ', %d dpi = %.0f x %.0f mm' % (
        args.dpi, w / args.dpi * 25.4, h / args.dpi * 25.4) if args.print else '', os.path.getsize(args.output)))
    if args.cmyk:
        cmyk = np.zeros((h, w, 4), np.uint8)
        cmyk[..., :3] = np.round(np.clip(inks, 0, 1) * 255)
        Image.fromarray(cmyk, 'CMYK').save(args.cmyk, compression='tiff_lzw', dpi=(args.dpi, args.dpi))
        print('%s: inks C, M, Y (K = 0), %d bytes' % (args.cmyk, os.path.getsize(args.cmyk)))
    if args.preview:
        base, _ = os.path.splitext(args.output)
        path = base + '_preview.png'
        light = 'LEDs' if args.print else 'filter'
        preview(rgb, red, blue, path, args.preview_width,
                ('No filter' if not args.print else 'White light', 'Red ' + light, 'Blue ' + light))
        print('%s: simulated views, %d bytes' % (path, os.path.getsize(path)))


if __name__ == '__main__':
    main()
