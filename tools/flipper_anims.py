#!/usr/bin/env python3
# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/
"""The animations of the dolphin of the Flipper Zero for the Flipper theme of the badge (Réglages > Thème).

Converts the animations of the Flipper Zero firmware (assets/dolphin: a meta.txt and PNG frames per animation) into
one .FZA file per animation for the SD card of the badge (FLIPPER/), read by src/menu/flipper.c.

The animations are not part of the badge: they belong to Flipper Devices Inc. under the GNU GPL v3 (the license of
the firmware of the Flipper Zero). They stay separate files on the SD card, with that license (COPYING.TXT) and where
they come from (SOURCE.TXT); the firmware of the badge contains none of them.

    python tools/flipper_anims.py                       # git clone (only assets/dolphin) into a temporary folder
    python tools/flipper_anims.py --src <flipperzero-firmware>   # an existing checkout
    python tools/flipper_anims.py --out <dir>           # default: docs/sd/FLIPPER

The .FZA format (little endian):
    "FZA1"
    u8 width, u8 height (<= 128 x 64), u8 frames per second, u8 min level, u8 max level (0: never chosen at random),
    u8 weight, u8 number of bitmaps, u8 passive frames, u8 active frames, u8 active cycles,
    u8 active cooldown (s), u8 number of bubbles, u16 offset of the bitmaps
    the order of the frames: passive + active bytes (index of a bitmap)
    the bubbles: u8 slot, u8 x, u8 y, u8 align (horizontal: 0 left, 1 center, 2 right; vertical << 4: 0 top,
        1 center, 2 bottom), u8 start frame, u8 end frame, u8 length, the text (ASCII, '\\n' between the lines)
    the bitmaps: (width + 7) / 8 bytes per row, 1 = black, the most significant bit on the left
"""

import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = 'https://github.com/flipperdevices/flipperzero-firmware.git'
HEADER_MAX = 1024  # FLIPPER_HEADER_MAX of src/menu/flipper.c
ALIGN_H = {'Left': 0, 'Center': 1, 'Right': 2}
ALIGN_V = {'Top': 0, 'Center': 1, 'Bottom': 2}


def read_kv(path):
    """The "Key: value" lines of a Flipper Format file, in order"""
    out = []
    with open(path, encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\r\n')
            if ':' in line and not line.startswith('#'):
                k, v = line.split(':', 1)
                out.append((k.strip(), v.strip()))
    return out


def read_manifest(path):
    anims = {}
    cur = None
    for k, v in read_kv(path):
        if k == 'Name':
            cur = anims.setdefault(v, {})
        elif cur is not None:
            cur[k] = int(v)
    return anims


def short_name(name):
    """L1_Waves_128x50 -> L1_Waves"""
    parts = name.split('_')
    if len(parts) > 1 and 'x' in parts[-1] and parts[-1].replace('x', '').isdigit():
        parts = parts[:-1]
    return '_'.join(parts)


def frame_bits(path, w, h):
    im = Image.open(path).convert('LA')
    if im.size != (w, h):
        raise ValueError(f'{path}: {im.size} instead of {w}x{h}')
    out = bytearray()
    for y in range(h):
        for bx in range((w + 7) // 8):
            byte = 0
            for b in range(8):
                x = bx * 8 + b
                if x < w:
                    lum, alpha = im.getpixel((x, y))
                    if alpha >= 128 and lum < 128:
                        byte |= 0x80 >> b
            out.append(byte)
    return bytes(out)


def convert(adir, entry):
    meta = read_kv(os.path.join(adir, 'meta.txt'))
    m = {}
    bubbles = []
    cur = None
    for k, v in meta:
        if k == 'Slot':
            cur = {'Slot': int(v)}
            bubbles.append(cur)
        elif cur is not None:
            cur[k] = v
        else:
            m[k] = v
    w, h = int(m['Width']), int(m['Height'])
    passive, active = int(m['Passive frames']), int(m['Active frames'])
    order = [int(x) for x in m['Frames order'].split()]
    if len(order) != passive + active or w > 128 or h > 64:
        raise ValueError(f'{adir}: unexpected meta.txt')
    n_bitmaps = max(order) + 1
    head = bytearray()
    head += bytes(order)
    for b in bubbles:
        text = b['Text'].replace('\\n', '\n').encode('ascii', 'replace')[:255]
        align = ALIGN_H.get(b.get('AlignH', 'Left'), 0) | ALIGN_V.get(b.get('AlignV', 'Top'), 0) << 4
        head += bytes([b['Slot'], int(b['X']), int(b['Y']), align, int(b['StartFrame']), int(b['EndFrame']),
                       len(text)]) + text
    fixed = struct.Struct('<4s12BH')
    offset = fixed.size + len(head)
    if offset > HEADER_MAX:
        raise ValueError(f'{adir}: header of {offset} bytes')
    out = fixed.pack(b'FZA1', w, h, int(m['Frame rate']), entry.get('Min level', 0), entry.get('Max level', 0),
                     entry.get('Weight', 1), n_bitmaps, passive, active, int(m.get('Active cycles', 0)),
                     min(255, int(m.get('Active cooldown', 0))), len(bubbles), offset) + head
    for i in range(n_bitmaps):
        out += frame_bits(os.path.join(adir, f'frame_{i}.png'), w, h)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--src', help='a checkout of flipperzero-firmware (default: cloned into a temporary folder)')
    ap.add_argument('--out', default=os.path.join(ROOT, 'docs', 'sd', 'FLIPPER'))
    args = ap.parse_args()
    tmp = None
    src = args.src
    if not src:
        tmp = tempfile.mkdtemp(prefix='fz_')
        src = os.path.join(tmp, 'fz')
        subprocess.run(['git', 'clone', '--depth', '1', '--filter=blob:none', '--sparse', REPO, src], check=True)
        subprocess.run(['git', '-C', src, 'sparse-checkout', 'set', 'assets/dolphin'], check=True)
    try:
        commit = subprocess.run(['git', '-C', src, 'log', '-1', '--format=%H %cs'], capture_output=True,
                                text=True).stdout.strip() or 'inconnu'
        os.makedirs(args.out, exist_ok=True)
        for f in os.listdir(args.out):
            if f.upper().endswith('.FZA'):
                os.remove(os.path.join(args.out, f))
        total = 0
        names = []
        # external: the idle animations; internal: L1_Tv, L1_BadBattery (shown when the battery is low)
        for part in ('external', 'internal'):
            base = os.path.join(src, 'assets', 'dolphin', part)
            manifest = read_manifest(os.path.join(base, 'manifest.txt'))
            for name in sorted(os.listdir(base)):
                adir = os.path.join(base, name)
                if not os.path.isfile(os.path.join(adir, 'meta.txt')) or name.startswith('L1_NoSd'):
                    continue  # NoSd: the animations are on the card
                entry = dict(manifest.get(name, {}))
                if name.startswith('L1_BadBattery'):
                    entry['Min level'] = entry['Max level'] = 0  # Only when the battery is low
                data = convert(adir, entry)
                out_name = short_name(name) + '.FZA'
                with open(os.path.join(args.out, out_name), 'wb') as f:
                    f.write(data)
                total += len(data)
                names.append(out_name)
        lic = os.path.join(src, 'LICENSE')
        if os.path.isfile(lic):
            shutil.copyfile(lic, os.path.join(args.out, 'COPYING.TXT'))
        with open(os.path.join(args.out, 'SOURCE.TXT'), 'w', encoding='utf-8', newline='\r\n') as f:
            f.write('Les animations du dauphin du Flipper Zero, pour le thème Flipper du badge SecSea\n'
                    '(Réglages > Thème). The animations of the dolphin of the Flipper Zero, for the Flipper theme of\n'
                    'the SecSea badge (Settings > Theme).\n\n'
                    '(c) Flipper Devices Inc., GNU GPL v3 (COPYING.TXT), from:\n'
                    f'{REPO} (assets/dolphin), commit {commit}\n\n'
                    'Converted by tools/flipper_anims.py of the badge (frames in 1 bit, see the .FZA format in it).\n'
                    'Unofficial: the badge is not related to Flipper Devices Inc.\n')
        print(f'{len(names)} animations, {total // 1024} KB in {args.out}')
    finally:
        if tmp:
            shutil.rmtree(tmp, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
