#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Plays every video and every music of the SD card of the badge, through its USB serial port, and checks that each
one plays to its end: no read error, not stopped before its duration, the frames per second of the videos.

Usage: python tools/badge_media_test.py [--port COM9] [--videos] [--music] [--max 0]
    --max N: play N seconds of each file at most (0 = the whole file, the default)
The badge is restarted first. A report is printed, and written to media_report.txt.
"""

import argparse
import re
import sys
import time
import os

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import badge_selftest as st  # noqa: E402
from badge_remote import Badge, find_port  # noqa: E402


def browser_listing(t, timeout=6):
    m = t.expect(r'^browser: /(\S*), (\d+) dir\(s\), (\d+) file\(s\)', timeout)
    return (m.group(1), int(m.group(2)), int(m.group(3))) if m else None


def play_all(t, kind, item_index, max_s, report, path_prefix=''):
    """In the browser of \\p kind (already open): every file, then every sub-folder."""
    listing = browser_listing(t)
    if not listing:
        report.append(f'{kind}: pas de liste de fichiers')
        return
    folder, n_dirs, n_files = listing
    print(f'{kind} /{folder}: {n_dirs} dossier(s), {n_files} fichier(s)')
    for i in range(n_files):
        t.mark()
        t.keys('x' * (n_dirs + i) + 'b', 0.3)
        if kind == 'video':
            m = t.expect(r'^video: (playing .*|.*error.*|.*failed.*|cannot.*)$', 8)
            if not m or not m.group(1).startswith('playing'):
                report.append(f'ÉCHEC vidéo /{folder} n°{i + 1}: {m.group(1) if m else "ne démarre pas"}')
                t.keys('a', 1)
                t.keys('y' * (n_dirs + i), 0.2)
                continue
            info = m.group(1)
            dur = float(re.search(r'\(([\d.]+)s\)', info).group(1)) if re.search(r'\(([\d.]+)s\)', info) else 600
            wait = dur + 20 if not max_s else min(dur, max_s)
            end = t.expect(r'^video: (\d+ frames in .*|done|read error.*)$', wait)
            if max_s and dur > max_s and not end:
                t.keys('a', 1)  # Stop: long enough
                end = t.expect(r'^video: (\d+ frames in .*|done|read error.*)$', 5)
                result = f'ok ({max_s} s sur {dur:.0f} s)' if end and 'error' not in end.group(1) else 'ERREUR'
            else:
                result = 'ok' if end and 'error' not in end.group(1) else f'ERREUR ({end.group(1) if end else "pas de fin"})'
            stats = t.expect(r'^video: \d+ frames in .*', 2)
            line = f'{"ok  " if result.startswith("ok") else "ÉCHEC"} vidéo {info[8:]} -> {result}' + \
                   (f' [{stats.group(0)[7:]}]' if stats else '')
        else:
            m = t.expect(r'^wav: (.*)$', 8)
            if not m or not m.group(1).startswith('playing'):
                report.append(f'ÉCHEC musique /{folder} n°{i + 1}: {m.group(1) if m else "ne démarre pas"}')
                t.keys('a', 1)
                t.keys('y' * (n_dirs + i), 0.2)
                continue
            info = m.group(1)
            dur = int(re.search(r'(\d+)s$', info).group(1)) if re.search(r'(\d+)s$', info) else 600
            wait = dur + 20 if not max_s else min(dur, max_s) + 3
            end = t.expect(r'^music: end at (\d+)s of (\d+)s', wait)
            if not end:
                t.keys('a', 1)  # Stop: long enough, or stuck
                end = t.expect(r'^music: end at (\d+)s of (\d+)s', 5)
                ok = end is not None and (max_s and dur > max_s)
                result = f'ok ({max_s} s sur {dur} s)' if ok else 'ERREUR (pas de fin)'
            else:
                pos, d = int(end.group(1)), int(end.group(2))
                ok = pos + 1 >= d
                result = 'ok' if ok else f'ERREUR (arrêt à {pos} s sur {d} s)'
            line = f'{"ok  " if result.startswith("ok") else "ÉCHEC"} musique {info[8:]} -> {result}'
        print(' ', line)
        report.append(line)
        t.pump(1)
        t.keys('y' * (n_dirs + i), 0.2)  # Back to the top of the list
    for d in range(n_dirs):
        t.mark()
        t.keys('x' * d + 'b', 0.3)
        play_all(t, kind, item_index, max_s, report)
        t.keys('a', 1)  # Back to the parent folder
        browser_listing(t, 3)
        t.keys('y' * d, 0.2)


def main():
    parser = argparse.ArgumentParser(description='Plays every video and music of the SD card of the badge')
    parser.add_argument('--port', default=None)
    parser.add_argument('--videos', action='store_true')
    parser.add_argument('--music', action='store_true')
    parser.add_argument('--max', type=int, default=0, help='seconds of each file at most (0: the whole file)')
    args = parser.parse_args()
    kinds = [k for k, on in (('video', args.videos), ('music', args.music)) if on] or ['video', 'music']
    badge = Badge(args.port or find_port())
    badge.start()
    while not badge.connected():
        time.sleep(0.1)
    t = st.Tester(badge, '.', False)
    t.mark()
    t.keys('R')
    t.expect(r'^--- disconnected', 5)
    while not badge.connected():
        time.sleep(0.2)
    t.pump(14)
    report = []
    for kind in kinds:
        st.open_theme(t, 'Médias')
        t.mark()
        t.keys(('x' if kind == 'video' else 'xx') + 'b')  # Vidéos, Musique
        play_all(t, kind, 1 if kind == 'video' else 2, args.max, report)
        t.keys('a', 1)
        t.keys('y' * (1 if kind == 'video' else 2))
        st.close_theme(t, 'Médias')
    badge.stop()
    failures = [l for l in report if l.startswith('ÉCHEC')]
    summary = f'{len(report)} fichier(s), {len(failures)} échec(s)'
    print(summary)
    with open('media_report.txt', 'w', encoding='utf-8') as f:
        f.write('\n'.join(report + ['', summary]) + '\n')
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()
