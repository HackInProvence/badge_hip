#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Convert a video to the .epv format played by the badge video player (see player.c), to be copied on the SD card.

Requires ffmpeg in the PATH, numpy and PIL.

The video is resized to cover 200x200 (then center cropped, or letterboxed with --fit), converted to grays,
resampled to the given frame rate, then dithered to black and white.
The ordered (Bayer) dithering is the default because it is stable from one frame to the next:
the e-Paper fast refresh only draws the differences, fewer changed pixels mean less ghosting.

The sound is kept for the buzzer of the badge (8 bit mono 16kHz, filtered like audio2wav.py), use --no-audio to remove it.

File format (little endian):
- a 512 bytes header: "EPVIDEO2" magic, uint16 width, uint16 height, uint16 fps, uint16 bits per pixel (1), uint32 n_frames,
  uint32 audio sample rate (0 without sound), uint32 n_audio_samples, then zeros
  ("EPVIDEO1" files are the same without sound),
- n_frames frames of width*height/8 bytes, packed like image2epaper.py (1 = white, bit 7 = leftmost pixel, row major),
- n_audio_samples samples, 8 bit unsigned mono, that start with the first frame.
  They are stored after the frames so that the badge can read them independently (with a second file handle).
"""

import argparse
import shutil
import struct
import subprocess
import sys

try:
    import numpy as np
    from PIL import Image
except ImportError:
    print('This script requires numpy and PIL')
    sys.exit(1)


WIDTH = HEIGHT = 200
HEADER_SIZE = 512
MAGIC = b'EPVIDEO2'
AUDIO_RATE = 16000
AUDIO_FILTERS = ('highpass=f=250,lowpass=f=5000,acompressor=threshold=-30dB:ratio=10:attack=3:release=60:makeup=8,dynaudnorm=f=100:g=9:p=0.95:m=100,volume=6dB,alimiter=limit=0.97:level=false')  # Same as audio2wav.py

# 8x8 Bayer matrix, normalized to thresholds in [0..255]
_B2 = np.array([[0, 2], [3, 1]])
_B4 = np.block([[4*_B2, 4*_B2+2], [4*_B2+3, 4*_B2+1]])
_B8 = np.block([[4*_B4, 4*_B4+2], [4*_B4+3, 4*_B4+1]])
BAYER8 = ((_B8 + .5) * 256 / 64).astype(np.uint8)


def read_frames(path, fps, fit, start, duration):
    """Yields gray frames (numpy uint8 HEIGHTxWIDTH) decoded by ffmpeg."""
    if fit:
        vf = f'scale={WIDTH}:{HEIGHT}:force_original_aspect_ratio=decrease:flags=lanczos,' \
             f'pad={WIDTH}:{HEIGHT}:(ow-iw)/2:(oh-ih)/2:white'
    else:
        vf = f'scale={WIDTH}:{HEIGHT}:force_original_aspect_ratio=increase:flags=lanczos,crop={WIDTH}:{HEIGHT}'
    cmd = ['ffmpeg', '-v', 'error']
    if start:
        cmd += ['-ss', str(start)]
    if duration:
        cmd += ['-t', str(duration)]
    cmd += ['-i', path, '-an', '-vf', f'fps={fps},{vf},format=gray', '-f', 'rawvideo', '-']
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    size = WIDTH*HEIGHT
    while True:
        raw = proc.stdout.read(size)
        if len(raw) < size:
            break
        yield np.frombuffer(raw, np.uint8).reshape(HEIGHT, WIDTH)
    proc.wait()
    if proc.returncode:
        sys.exit(f'ffmpeg failed with code {proc.returncode}')


def read_audio(path, start, duration):
    """Returns the sound as 8 bit unsigned mono samples at AUDIO_RATE (empty when the video has no sound)."""
    cmd = ['ffmpeg', '-v', 'error']
    if start:
        cmd += ['-ss', str(start)]
    if duration:
        cmd += ['-t', str(duration)]
    cmd += ['-i', path, '-vn', '-ac', '1', '-ar', str(AUDIO_RATE), '-af', AUDIO_FILTERS, '-f', 'u8', '-']
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if proc.returncode:
        print('WARNING: no sound extracted: ' + proc.stderr.decode(errors='replace').strip(), file=sys.stderr)
        return b''
    return proc.stdout


def adjust(gray, gamma, contrast):
    """Gamma then contrast around the mid gray, returns uint8."""
    f = (gray / 255.) ** gamma
    f = (f - .5) * contrast + .5
    return (np.clip(f, 0, 1) * 255).astype(np.uint8)


def dither(gray, method):
    """Returns a boolean array, True for white."""
    if method == 'bayer':
        thresholds = np.tile(BAYER8, (HEIGHT//8 + 1, WIDTH//8 + 1))[:HEIGHT, :WIDTH]
        return gray > thresholds
    if method == 'fs':
        return np.array(Image.fromarray(gray).convert('1'), dtype=bool)  # PIL uses Floyd-Steinberg
    return gray >= 128  # threshold


def pack(white):
    """Packs 8 pixels per byte, bit 7 is the leftmost pixel (same as image2epaper.py)."""
    return np.packbits(white.astype(np.uint8), axis=1, bitorder='big').tobytes()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Convert a video to the .epv format of the badge video player')
    parser.add_argument('video', help='source video (any format read by ffmpeg)')
    parser.add_argument('--output', '-o', default='VIDEO.EPV', help='output file, to copy at the root of the SD card')
    parser.add_argument('--fps', type=int, default=10, help='frame rate: 10 has the best contrast, 20 and 30 are blurrier')
    parser.add_argument('--fit', action='store_true', help='letterbox the whole video instead of cropping the center')
    parser.add_argument('--dither', choices=('bayer', 'fs', 'threshold'), default='bayer',
                        help='bayer is stable between frames (less ghosting), fs (Floyd-Steinberg) is finer on still images')
    parser.add_argument('--gamma', type=float, default=1., help='< 1 lightens, > 1 darkens')
    parser.add_argument('--contrast', type=float, default=1.2, help='> 1 increases contrast')
    parser.add_argument('--start', type=float, default=None, help='start time in seconds')
    parser.add_argument('--duration', type=float, default=None, help='duration in seconds')
    parser.add_argument('--preview', default=None, help='also write an animated GIF preview of the result')
    parser.add_argument('--no-audio', action='store_true', help='do not keep the sound')
    args = parser.parse_args()

    if shutil.which('ffmpeg') is None:
        sys.exit('ffmpeg not found in the PATH')
    if args.fps not in (10, 20, 30):
        print('WARNING: the player has waveforms for 10, 20 and 30 fps, it will use the closest one', file=sys.stderr)

    previews = []
    n_frames = 0
    with open(args.output, 'wb') as out:
        out.write(b'\0' * HEADER_SIZE)  # Rewritten at the end, when n_frames is known
        for gray in read_frames(args.video, args.fps, args.fit, args.start, args.duration):
            white = dither(adjust(gray, args.gamma, args.contrast), args.dither)
            out.write(pack(white))
            if args.preview:
                previews.append(Image.fromarray(white.astype(np.uint8)*255))
            n_frames += 1

        # The sound, cut or padded with silence to the length of the video
        audio = b'' if args.no_audio else read_audio(args.video, args.start, args.duration)
        audio_rate = AUDIO_RATE if audio else 0
        n_samples = n_frames * audio_rate // args.fps
        audio = audio[:n_samples].ljust(n_samples, b'\x80')
        out.write(audio)

        header = MAGIC + struct.pack('<HHHHIII', WIDTH, HEIGHT, args.fps, 1, n_frames, audio_rate, n_samples)
        out.seek(0)
        out.write(header.ljust(HEADER_SIZE, b'\0'))

    if n_frames == 0:
        sys.exit('no frame decoded')
    if args.preview:
        previews[0].save(args.preview, save_all=True, append_images=previews[1:], duration=1000//args.fps, loop=0)
    size = HEADER_SIZE + n_frames*WIDTH*HEIGHT//8 + n_samples
    print(f'{args.output}: {n_frames} frames @ {args.fps} fps ({n_frames/args.fps:.1f}s), '
          f'{"sound " + str(audio_rate) + " Hz" if audio_rate else "no sound"}, {size/1e6:.1f} MB', file=sys.stderr)
