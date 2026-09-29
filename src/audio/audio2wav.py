#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Convert audio files (MP3, OGG, FLAC, WAV, videos...) to WAV files played by the badge (menu "Musique"),
to be copied on the SD card.

Requires ffmpeg in the PATH.

The badge plays sound on its buzzer, which only renders ~300Hz to ~5kHz, hence the default format:
8 bit unsigned PCM, mono, 16kHz (~1MB per minute), with the frequencies out of the buzzer range removed
and the loudness normalized (quiet passages are hard to hear on a buzzer).
The badge also plays other PCM WAV files (8 or 16 bit, mono or stereo, up to 48kHz), but they use more space for nothing.
"""

import argparse
import glob
import os
import shutil
import subprocess
import sys

SAMPLE_RATE = 16000
# Band of the buzzer, then strong compression and gain: the buzzer only makes big swings audible
FILTERS = ('highpass=f=250,lowpass=f=5000,acompressor=threshold=-30dB:ratio=10:attack=3:release=60:makeup=8,dynaudnorm=f=100:g=9:p=0.95:m=100,volume=6dB,alimiter=limit=0.97:level=false')


def audio_filters(no_filter):
    return 'anull' if no_filter else FILTERS


def convert(src, dst, rate, no_filter, start=None, duration=None):
    cmd = ['ffmpeg', '-v', 'error', '-y']
    if start:
        cmd += ['-ss', str(start)]
    if duration:
        cmd += ['-t', str(duration)]
    cmd += ['-i', src, '-vn', '-ac', '1', '-ar', str(rate), '-af', audio_filters(no_filter), '-acodec', 'pcm_u8', dst]
    subprocess.run(cmd, check=True)


AUDIO_EXTENSIONS = ('.mp3', '.ogg', '.oga', '.opus', '.flac', '.wav', '.m4a', '.aac', '.wma')


def expand_inputs(inputs):
    """Expands the wildcards (Windows shells don't) and the directories (their audio files)."""
    files = []
    for item in inputs:
        if os.path.isdir(item):
            files += sorted(os.path.join(item, f) for f in os.listdir(item) if f.lower().endswith(AUDIO_EXTENSIONS))
        elif glob.has_magic(item):
            matches = sorted(glob.glob(item))
            if not matches:
                print(f'WARNING: no file matches {item}', file=sys.stderr)
            files += matches
        else:
            files.append(item)
    return files


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Convert audio files to 8 bit mono WAV for the badge buzzer')
    parser.add_argument('inputs', nargs='+',
                        help='source files (any format read by ffmpeg), wildcards (e.g. "C:/music/*.mp3") or directories')
    parser.add_argument('--output-dir', '-o', default='.', help='where to write the .wav files')
    parser.add_argument('--rate', type=int, default=SAMPLE_RATE, help='sample rate, 16000 is enough for the buzzer')
    parser.add_argument('--no-filter', action='store_true', help='keep the whole band and dynamics')
    parser.add_argument('--start', type=float, default=None, help='start time in seconds')
    parser.add_argument('--duration', type=float, default=None, help='duration in seconds')
    args = parser.parse_args()

    if shutil.which('ffmpeg') is None:
        sys.exit('ffmpeg not found in the PATH')
    sources = expand_inputs(args.inputs)
    if not sources:
        sys.exit('no input file')
    os.makedirs(args.output_dir, exist_ok=True)
    failed = []
    for i, src in enumerate(sources, 1):
        name = os.path.splitext(os.path.basename(src))[0] + '.wav'
        dst = os.path.join(args.output_dir, name)
        try:
            convert(src, dst, args.rate, args.no_filter, args.start, args.duration)
            print(f'[{i}/{len(sources)}] {dst}: {os.path.getsize(dst)/1e6:.1f} MB', file=sys.stderr)
        except subprocess.CalledProcessError:
            # A broken file should not stop the conversion of the others
            print(f'[{i}/{len(sources)}] ERROR: cannot convert {src}', file=sys.stderr)
            failed.append(src)
    if failed:
        sys.exit(f'{len(failed)} file(s) not converted: ' + ', '.join(os.path.basename(f) for f in failed))
