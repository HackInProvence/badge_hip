#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Automatic tests run on the PC, without the badge:
- C modules of the firmware compiled with a host compiler (gcc or clang) and a stand-in of the Pico SDK (stubs/):
  graphics (gfx), infrared decoding (ir), mini games (games), signed score QR codes (score), fast reading (rsvp), copy of the screen RAM (screen),
- Python converters: image2epi.py (exact round trip), video2epaper.py and audio2wav.py (need ffmpeg, skipped without).

Usage: python src/tests/host/run_tests.py [--cc gcc] [--keep] [test names...]
Exit code 0 when all the tests pass.
"""

import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(HERE, '..', '..'))
STUBS = os.path.join(HERE, 'stubs')
CFLAGS = ['-std=gnu11', '-O1', '-g', '-Wall', '-Wno-unused-function', '-Wno-unused-variable',
          '-Wno-pointer-sign', '-Wno-unused-but-set-variable', '-DBADGE_SECSEA', '-DPICO_HOST']


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8', errors='replace', **kw)


# ---- C tests ----

def c_test(name, sources, includes, work, copy=()):
    """Compiles then runs tests/host/<name>.c, returns (ok, output)."""
    for src, dst in copy:
        shutil.copy(os.path.join(SRC, src), os.path.join(work, dst))
    exe = os.path.join(work, name + ('.exe' if os.name == 'nt' else ''))
    cmd = [ARGS.cc] + CFLAGS + ['-I' + i for i in includes + [HERE]] + \
          [os.path.join(HERE, name + '.c')] + [os.path.join(SRC, s) for s in sources] + ['-o', exe]
    r = run(cmd)
    if r.returncode:
        return False, 'compilation failed:\n' + r.stdout + r.stderr
    r = run([exe], cwd=work)
    return r.returncode == 0, r.stdout + r.stderr


def test_gfx(work):
    return c_test('test_gfx', ['gfx/gfx.c', 'gfx/gfx_fonts.c'], [os.path.join(SRC, 'gfx')], work)


def test_ir(work):
    return c_test('test_ir', [], [STUBS, SRC], work)


def test_screen(work):
    return c_test('test_screen', [], [STUBS, SRC, os.path.join(SRC, 'log'), os.path.join(SRC, 'screen')], work)


def test_games(work):
    return c_test('test_games', ['gfx/gfx.c', 'gfx/gfx_fonts.c', 'menu/score_code.c', 'qrcode/qrcodegen.c'],
                  [STUBS, os.path.join(SRC, 'gfx'), os.path.join(SRC, 'menu'), os.path.join(SRC, 'qrcode')], work)


def test_score(work):
    return c_test('test_score', ['gfx/gfx.c', 'gfx/gfx_fonts.c', 'qrcode/qrcodegen.c'],
                  [os.path.join(SRC, 'gfx'), os.path.join(SRC, 'menu'), os.path.join(SRC, 'qrcode')], work)


def test_rsvp(work):
    # rsvp.c is copied next to the stand-ins of its dependencies, so that its "store.h", "screen.h"... are them
    for h in os.listdir(os.path.join(STUBS, 'rsvp')):
        shutil.copy(os.path.join(STUBS, 'rsvp', h), work)
    return c_test('test_rsvp', ['gfx/gfx.c', 'gfx/gfx_fonts.c'], [work, STUBS, os.path.join(SRC, 'gfx')], work,
                  copy=[('menu/rsvp.c', 'rsvp.c'), ('menu/rsvp.h', 'rsvp.h')])


# ---- Python converters ----

def py(script, *args, cwd=None):
    return run([sys.executable, os.path.join(SRC, script)] + list(args), cwd=cwd)


def test_image2epi(work):
    try:
        from PIL import Image
    except ImportError:
        return None, 'PIL not installed'
    out = []
    # A 200x200 picture with 4 colors is converted exactly: black < red < gray < white
    img = Image.new('RGB', (200, 200), (255, 255, 255))
    colors = [(0, 0, 0), (200, 0, 0), (180, 180, 180), (255, 255, 255)]
    for y in range(200):
        for x in range(200):
            img.putpixel((x, y), colors[(x // 10 + y // 50) % 4])
    src = os.path.join(work, 'exact.png')
    img.save(src)
    r = py('images/image2epi.py', src, '-o', work)
    if r.returncode:
        return False, r.stdout + r.stderr
    data = open(os.path.join(work, 'exact.epi'), 'rb').read()
    ok = len(data) == 16 + 2 * 5000 and data[:8] == b'EPIMAGE1' and struct.unpack('<HHHH', data[8:16]) == (200, 200, 2, 0)
    out.append(f'header and size: {"ok" if ok else "BAD"}')
    lsb, msb = data[16:5016], data[5016:]
    bad = 0
    for y in range(200):
        for x in range(200):
            bit = 0x80 >> (x % 8)
            level = (2 if msb[y * 25 + x // 8] & bit else 0) + (1 if lsb[y * 25 + x // 8] & bit else 0)
            bad += level != (x // 10 + y // 50) % 4
    out.append(f'exact round trip: {bad} wrong pixels')
    ok = ok and bad == 0

    # Any picture: resized and dithered, black and white variant is 1 bit per pixel
    photo = Image.radial_gradient('L').resize((320, 240))
    photo.save(os.path.join(work, 'photo.jpg'))
    r = py('images/image2epi.py', os.path.join(work, 'photo.jpg'), '--fit', '--bw', '-o', work)
    data = open(os.path.join(work, 'photo.epi'), 'rb').read() if r.returncode == 0 else b''
    bw_ok = len(data) == 16 + 5000 and struct.unpack('<H', data[12:14])[0] == 1
    out.append(f'black and white: {"ok" if bw_ok else "BAD " + r.stderr}')
    return ok and bw_ok, '\n'.join(out)


def make_test_video(work, seconds=2):
    path = os.path.join(work, 'source.mp4')
    r = run(['ffmpeg', '-y', '-loglevel', 'error', '-f', 'lavfi', '-i', f'testsrc=size=320x240:rate=25:duration={seconds}',
             '-f', 'lavfi', '-i', f'sine=frequency=440:duration={seconds}', '-shortest', '-pix_fmt', 'yuv420p', path])
    return path if r.returncode == 0 else None


def test_video2epaper(work):
    if shutil.which('ffmpeg') is None:
        return None, 'ffmpeg not in the PATH'
    src = make_test_video(work)
    if not src:
        return False, 'ffmpeg could not generate the test video'
    dst = os.path.join(work, 'test.epv')
    r = py('video/video2epaper.py', src, '-o', dst, '--fps', '10')
    if r.returncode:
        return False, r.stdout + r.stderr
    data = open(dst, 'rb').read()
    magic = data[:8]
    w, h, fps, bpp, n_frames, rate, n_samples = struct.unpack('<HHHHIII', data[8:28])
    out = [f'{magic.decode()} {w}x{h} {fps} fps, {n_frames} frames, sound {rate} Hz, {n_samples} samples']
    ok = (magic == b'EPVIDEO2' and (w, h, fps, bpp) == (200, 200, 10, 1) and 19 <= n_frames <= 21
          and rate == 16000 and n_samples == n_frames * 1600 and len(data) == 512 + n_frames * 5000 + n_samples)
    # The sound is not silence
    audio = data[512 + n_frames * 5000:]
    ok = ok and max(audio) - min(audio) > 40
    out.append(f'sound amplitude {max(audio) - min(audio) if audio else 0}')
    return ok, '\n'.join(out)


def test_audio2wav(work):
    if shutil.which('ffmpeg') is None:
        return None, 'ffmpeg not in the PATH'
    src = os.path.join(work, 'tone.mp3')
    r = run(['ffmpeg', '-y', '-loglevel', 'error', '-f', 'lavfi', '-i', 'sine=frequency=880:duration=1', src])
    if r.returncode:
        return False, 'ffmpeg could not generate the test sound'
    outdir = os.path.join(work, 'wav')
    r = py('audio/audio2wav.py', os.path.join(work, '*.mp3'), '-o', outdir)  # The wildcard is expanded by the script
    if r.returncode:
        return False, r.stdout + r.stderr
    with wave.open(os.path.join(outdir, 'tone.wav')) as w:
        params = (w.getnchannels(), w.getsampwidth(), w.getframerate())
        n = w.getnframes()
        frames = w.readframes(n)
    ok = params == (1, 1, 16000) and 15000 <= n <= 17000 and max(frames) - min(frames) > 100
    return ok, f'{params[0]} channel, {params[1] * 8} bits, {params[2]} Hz, {n} samples, amplitude {max(frames) - min(frames)}'


TESTS = [('gfx', test_gfx), ('ir', test_ir), ('games', test_games), ('score', test_score), ('rsvp', test_rsvp), ('screen', test_screen),
         ('image2epi', test_image2epi), ('video2epaper', test_video2epaper), ('audio2wav', test_audio2wav)]


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Host tests of the badge firmware and tools')
    parser.add_argument('names', nargs='*', help='tests to run (default: all): ' + ', '.join(n for n, _ in TESTS))
    parser.add_argument('--cc', default=os.environ.get('CC') or shutil.which('gcc') and 'gcc' or 'clang',
                        help='host C compiler (default: gcc, or $CC)')
    parser.add_argument('--keep', action='store_true', help='keep the temporary directories')
    parser.add_argument('--verbose', '-v', action='store_true', help='show the output of passing tests too')
    ARGS = parser.parse_args()

    results = []
    for name, func in TESTS:
        if ARGS.names and name not in ARGS.names:
            continue
        if func in (test_gfx, test_ir, test_games, test_score, test_rsvp, test_screen) and not shutil.which(ARGS.cc):
            ok, output = None, f'no C compiler ({ARGS.cc})'
        else:
            work = tempfile.mkdtemp(prefix=f'badge_{name}_')
            try:
                ok, output = func(work)
            except Exception as e:  # A test that crashes fails, the others still run
                ok, output = False, f'{type(e).__name__}: {e}'
            if ARGS.keep:
                output += f'\n(files in {work})'
            else:
                shutil.rmtree(work, ignore_errors=True)
        status = 'SKIP' if ok is None else 'PASS' if ok else 'FAIL'
        results.append((name, status))
        print(f'[{status}] {name}')
        if status != 'PASS' or ARGS.verbose:
            print('    ' + output.strip().replace('\n', '\n    '))

    failed = [n for n, s in results if s == 'FAIL']
    print(f'\n{sum(s == "PASS" for _, s in results)} passed, {len(failed)} failed, '
          f'{sum(s == "SKIP" for _, s in results)} skipped')
    sys.exit(1 if failed else 0)
