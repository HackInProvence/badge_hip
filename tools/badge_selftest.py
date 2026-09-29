#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Automatic test of a badge running the menu application (badge_menu), through its USB serial port:
the buttons are simulated with the keys of the serial protocol (a, b, x, y), the firmware answers with log lines
("ui: <page title>", "radio: ...", "game: ...") and the screen ("@FB ..." lines, saved as PNG screenshots).

The badge is rebooted first ("R" key) so that the menus start from a known state. Nothing is written in the settings
of the badge, except the records of the games (a Simon or Snake game can end with 0 points) and with --ctf
(the Konami code test marks the flag as found).
With the BADGE_SCORE_KEY environment variable (see score_check.py), the signature of the score QR code is checked.

Usage:
    python tools/badge_selftest.py [--port COM9] [--out selftest] [--ctf] [--only games,radio,...]

Exit code 0 when no test failed (skipped tests are fine: e.g. no SD card, uncalibrated battery).
Close the other applications that use the serial port (badge_remote.py, a terminal...) first.
"""

import argparse
import datetime
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from badge_remote import Badge, find_port, save_png  # noqa: E402

GROUPS = ['diag', 'menus', 'games', 'ctf', 'settings', 'radio', 'ir', 'images']


class Tester:
    def __init__(self, badge, out_dir, verbose):
        self.badge = badge
        self.out_dir = out_dir
        self.verbose = verbose
        self.lines = []  # Every log line received
        self.cursor = 0  # expect() searches from here
        self.frame = None  # Last screen received
        self.results = []

    # ---- Serial ----

    def pump(self, duration=0.0):
        """Collects the log lines and screens received, during at least \\p duration seconds."""
        end = time.time() + duration
        while True:
            while not self.badge.logs.empty():
                line = self.badge.logs.get()
                self.lines.append(line)
                if self.verbose:
                    print('    < ' + line)
            while not self.badge.frames.empty():
                self.frame = self.badge.frames.get()
            if time.time() >= end:
                return
            time.sleep(0.02)

    def mark(self):
        """Forget what was received until now: the next expect() only looks at newer lines."""
        self.pump()
        self.cursor = len(self.lines)

    def expect(self, pattern, timeout=3.0):
        """Waits for a log line matching the regular expression, returns the match (None after the timeout)."""
        end = time.time() + timeout
        regex = re.compile(pattern)
        while True:
            self.pump()
            for i in range(self.cursor, len(self.lines)):
                m = regex.search(self.lines[i])
                if m:
                    self.cursor = i + 1
                    return m
            if time.time() >= end:
                return None
            time.sleep(0.05)

    def keys(self, keys, pause=0.25):
        """Simulated buttons, one by one (the firmware reads one key per loop)."""
        for k in keys:
            self.badge.send(k)
            time.sleep(pause)

    def press(self, keys, ui=None, timeout=3.0):
        """Presses the keys, then waits for the page \\p ui (its title). Returns whether it was shown."""
        self.mark()
        self.keys(keys)
        return ui is None or self.expect(r'^ui: ' + re.escape(ui) + r'$', timeout) is not None

    def screenshot(self, name, settle=1.0):
        """Saves the screen once the display is done (a fast refresh takes ~0.3s)."""
        self.pump(settle)
        if self.frame is None:
            return None
        path = os.path.join(self.out_dir, name + '.png')
        save_png(path, self.frame, 2)
        return path

    # ---- Results ----

    def result(self, name, ok, detail=''):
        status = 'SKIP' if ok is None else 'PASS' if ok else 'FAIL'
        self.results.append((name, status, detail))
        print(f'[{status}] {name}' + (f' - {detail}' if detail else ''))
        return ok

    def check_ui(self, name, keys, ui, timeout=3.0):
        return self.result(name, self.press(keys, ui, timeout), f'page "{ui}"')


# ---- The tests, grouped. Each group starts and ends at the main menu, on the first theme ----

THEMES = ['Médias', 'Jeux', 'Badge', 'Radio & IR', 'Réglages']
TOP = 'Badge SecSea'


def open_theme(t, theme):
    """From the main menu (first theme selected): opens a theme."""
    return t.press('x' * THEMES.index(theme) + 'b', theme)


def close_theme(t, theme):
    """Back to the main menu, first theme selected again."""
    t.press('a', TOP)
    t.keys('y' * THEMES.index(theme))


def test_diag(t):
    t.mark()
    t.keys('!')
    m = t.expect(r'radio: CC1101 version 0x([0-9a-f]+), crystal used (\d+) Hz, measured (\d+) Hz')
    if not m:
        t.result('radio chip', False, 'no answer to "!"')
    else:
        t.result('radio chip', m.group(1) == '14', f'CC1101 version 0x{m.group(1)} (0x14 expected)')
        used, measured = int(m.group(2)), int(m.group(3))
        t.result('radio crystal', used in (26000000, 27000000),
                 f'{used} Hz used, {measured} Hz measured (26 or 27 MHz expected)')
    m = t.expect(r'battery: (.*)', 1)
    if not m:
        t.result('battery', False, 'no battery line')
    elif 'not calibrated' in m.group(1):
        t.result('battery', None, m.group(1))
    else:
        mv = int(re.search(r'(\d+) mV', m.group(1)).group(1))
        t.result('battery', 3000 <= mv <= 4300, m.group(1))


def test_menus(t):
    t.check_ui('menu: open a theme', 'b', 'Médias')
    t.check_ui('menu: back to the themes', 'a', TOP)
    t.screenshot('menu')
    ok = True
    for theme in THEMES[1:]:
        ok &= open_theme(t, theme)
        t.screenshot('theme_' + re.sub(r'\W+', '_', theme))
        close_theme(t, theme)
    t.result('menu: every theme', ok)


GAMES = ['Morpion', 'Puissance 4', 'Simon', 'Réflexes', 'Snake']


def test_games(t):
    if not open_theme(t, 'Jeux'):
        return t.result('games', False, 'theme "Jeux" not shown')
    for i, name in enumerate(GAMES):
        if not t.press('b', name):
            t.result(f'game: {name}', False, 'not started')
            t.keys('a')
            t.keys('x')
            continue
        detail = ''
        ok = True
        if name == 'Morpion':
            t.keys('b')  # The center, then the cicada answers
            t.pump(1.5)
        elif name == 'Puissance 4':
            t.keys('b')  # Middle column, the cicada thinks ~0.5s
            t.pump(2.0)
        elif name == 'Simon':
            t.pump(2.5)  # The first light of the sequence
        elif name == 'Réflexes':
            t.mark()
            t.keys('b')
            if t.expect(r'^game: reflex go$', 8):
                t.keys('b', pause=0)
                m = t.expect(r'^game: reflex (\d+) ms$', 3)
                ok = m is not None and int(m.group(1)) < 1000
                detail = f'{m.group(1)} ms through USB' if m else 'no reaction time'
            else:
                ok, detail = False, 'the LEDs never lit up'
        elif name == 'Snake':
            t.mark()
            t.keys('b')
            t.pump(3.0)
            t.screenshot('game_Snake_run', 0)
            # Straight into the wall, then the signed score as a QR code
            if t.expect(r'^game: snake over', 15):
                t.mark()
                t.keys('x')
                m = t.expect(r'^game: score code (HIP26:SNAKE:\d+:[0-9A-F]{8}:.+:[0-9A-F]{16})$', 3)
                t.screenshot('game_Snake_qr', 1.5)
                detail = m.group(1) if m else 'no score code'
                ok = m is not None
                key = os.environ.get('BADGE_SCORE_KEY')
                if m and key:
                    from score_check import check
                    ok, _ = check(m.group(1), bytes.fromhex(key))
                    detail += ', signature ' + ('ok' if ok else 'WRONG')
                t.keys('b')  # Closes the QR code
            else:
                ok, detail = False, 'no game over'
        t.screenshot('game_' + re.sub(r'\W+', '_', name), 0.8)
        # Quit. In Simon the left wing is also a pad: a wrong pad ends the game, then it quits
        t.mark()
        quit_ok = False
        for _ in range(4):
            t.keys('a')
            if t.expect(r'^ui: Jeux$', 1.5):
                quit_ok = True
                break
        ok &= quit_ok
        t.result(f'game: {name}', ok, detail)
        t.keys('x')
    # Long press on a game of the menu: the record as a signed QR code (Snake has one after the game above)
    t.keys('y')
    t.mark()
    t.keys('B')
    m = t.expect(r'^game: score code (HIP26:SNAKE:\d+:[0-9A-F]{8}:.+:[0-9A-F]{16})$', 3)
    t.screenshot('record_Snake_qr', 1.5)
    t.result('game: record QR code', m is not None and t.press('a', 'Jeux'), m.group(1) if m else 'no record shown')
    t.keys('y' * (len(GAMES) - 1))
    close_theme(t, 'Jeux')


def test_ctf(t):
    if not open_theme(t, 'Jeux'):
        return t.result('ctf', False, 'theme "Jeux" not shown')
    t.keys('x' * (len(GAMES) + 1))  # After the games and the blind test
    t.press('b', 'CTF')
    t.mark()
    t.keys('b')  # Type a code
    t.keys('yyxxababba')  # Konami: up up down down left right left right B A, with the badge buttons
    m = t.expect(r'^ctf: code (right|wrong)$', 4)
    t.screenshot('ctf')
    t.result('ctf: Konami code', m is not None and m.group(1) == 'right', m.group(0) if m else 'no answer')
    t.keys('a')  # Page -> CTF list
    t.press('a', 'Jeux')
    t.keys('y' * (len(GAMES) + 1))
    close_theme(t, 'Jeux')


def test_settings(t):
    if not open_theme(t, 'Réglages'):
        return t.result('settings', False, 'theme "Réglages" not shown')
    # Screensaver preview: third line of the screensaver settings
    t.keys('b')
    t.mark()
    t.keys('xxb')
    m = t.expect(r'^saver: on', 15)  # After the cleaning full refreshes (black, white)
    t.pump(4)  # The full refresh takes ~2s
    # Dithered black and white (the 4 grays waveform leaves a ghost): both colors, no gray
    shown = t.frame is not None and all(v in (0, 3) for row in t.frame for v in row) \
        and any(v == 0 for row in t.frame for v in row) and any(v == 3 for row in t.frame for v in row)
    t.screenshot('saver', 0)
    t.result('screensaver: preview', m is not None and shown, (m.group(0) if m else 'not started')
             + ('' if shown else ', no black and white image'))
    t.mark()
    t.keys('x')  # Any button wakes up
    t.result('screensaver: wake up', t.expect(r'^saver: off$', 3) is not None)
    t.keys('yy')
    t.press('a', 'Réglages')
    # Infos, then the credits
    t.check_ui('infos', 'xb', 'Infos')
    t.screenshot('infos')
    t.check_ui('credits', 'b', 'Crédits')
    t.mark()
    t.keys('x')
    m = t.expect(r'^credits: (.+)$')
    t.screenshot('credits_2')
    t.result('credits: next page', m is not None, m.group(1) if m else '')
    t.check_ui('credits: back to infos', 'a', 'Infos')
    t.press('a', 'Réglages')
    t.keys('y')
    close_theme(t, 'Réglages')


def test_radio(t):
    if not open_theme(t, 'Radio & IR'):
        return t.result('radio', False, 'theme "Radio & IR" not shown')
    t.mark()
    t.keys('xb')  # Radio : message
    m = t.expect(r'^radio: sending message #(\d+)$', 3)
    sent = t.expect(r'^radio: message #\d+ sent in (\d+) ms$', 5) if m else None
    t.result('radio: message', sent is not None, f'sent in {sent.group(1)} ms' if sent else 'not sent')
    # Long press: test mode, a message every 5 s (the first one right away)
    t.pump(1)
    t.mark()
    t.keys('B')
    started = t.expect(r'^radio test: every 5 s$', 3)
    times = []
    for _ in range(2):
        if t.expect(r'^radio: message #\d+ sent', 8):
            times.append(time.time())
    period = times[1] - times[0] if len(times) == 2 else 0
    t.keys('y')  # Left flank: 4 s
    t.screenshot('radio_test', 1.5)
    stopped = t.press('a', 'Radio & IR')
    t.result('radio: test mode', started is not None and 4.0 <= period <= 6.0 and stopped,
             f'{len(times)} message(s), {period:.1f} s apart' if started else 'not started')
    t.keys('y')
    # The network of the cicadas sends a beacon every ~2s
    counts = []
    for _ in range(2):
        t.mark()
        t.keys('!')
        m = t.expect(r'social: (on|off), .* beacons sent (\d+) received (\d+)', 3)
        counts.append(m)
        t.pump(5)
    if not all(counts):
        t.result('social: beacons', False, 'no answer to "!"')
    elif counts[0].group(1) == 'off':
        t.result('social: beacons', None, 'network disabled on this badge')
    else:
        a, b = int(counts[0].group(2)), int(counts[1].group(2))
        t.result('social: beacons', b > a, f'{b - a} beacon(s) in 5s, {counts[1].group(3)} received in total')
    close_theme(t, 'Radio & IR')


def test_ir(t):
    t.mark()
    t.keys('i')
    m = t.expect(r'ir test: decode (ok|failed), address 0x([0-9A-F]+), command 0x([0-9A-F]+)')
    t.result('ir: NEC decode', m is not None and m.group(1) == 'ok' and m.group(2) == '0004' and m.group(3) == '08',
             m.group(0) if m else 'no answer')
    m = t.expect(r'ir test: sent in (\d+) us')
    t.result('ir: NEC send', m is not None and 60000 <= int(m.group(1)) <= 80000,
             f'{m.group(1)} us (~67500 expected)' if m else 'no answer')


def test_images(t):
    if not open_theme(t, 'Médias'):
        return t.result('images', False, 'theme "Médias" not shown')
    t.mark()
    t.keys('b')  # Images
    m = t.expect(r'^browser: /(\S+), (\d+) dir\(s\), (\d+) file\(s\)', 5)
    if not m:
        t.result('images: SD card', None, 'no SD card (or no IMAGES directory)')
        t.keys('a')
    elif int(m.group(3)) == 0:
        t.result('images: SD card', None, f'/{m.group(1)}: no image')
        t.keys('a')
    else:
        t.result('images: SD card', True, f'/{m.group(1)}: {m.group(3)} file(s)')
        t.keys('x' * int(m.group(2)))  # The first file, after the directories
        t.mark()
        t.keys('b')
        first = t.expect(r'^image: (\S.*) \((\d) bit', 8)
        t.pump(3)
        t.screenshot('image_1', 0)
        t.keys('x')
        second = t.expect(r'^image: (\S.*) \((\d) bit', 8)
        t.pump(3)
        t.screenshot('image_2', 0)
        t.result('images: show', first is not None, first.group(1) if first else 'not shown')
        t.result('images: next', second is not None and (not first or second.group(1) != first.group(1)),
                 second.group(1) if second else 'not shown')
        t.keys('a')  # Back to the list
        t.keys('a')
    t.press('a', TOP)


TESTS = {'diag': test_diag, 'menus': test_menus, 'games': test_games, 'ctf': test_ctf,
         'settings': test_settings, 'radio': test_radio, 'ir': test_ir, 'images': test_images}


def main():
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')  # The page titles have accents
    parser = argparse.ArgumentParser(description='Automatic test of the badge through its USB serial port')
    parser.add_argument('--port', help='serial port (default: the first Raspberry Pi Pico found)')
    parser.add_argument('--out', default=None, help='directory of the screenshots (default: selftest_<date>)')
    parser.add_argument('--only', help='comma separated groups: ' + ', '.join(GROUPS))
    parser.add_argument('--ctf', action='store_true', help='also type the Konami code (marks the CTF flag as found)')
    parser.add_argument('--no-reboot', action='store_true', help='start from the current page (must be the main menu)')
    parser.add_argument('--verbose', '-v', action='store_true', help='show the log lines of the badge')
    args = parser.parse_args()

    groups = args.only.split(',') if args.only else [g for g in GROUPS if g != 'ctf' or args.ctf]
    for g in groups:
        if g not in TESTS:
            sys.exit(f'unknown group {g}, choose among: {", ".join(GROUPS)}')
    port = args.port or find_port()
    if not port:
        sys.exit('no badge found: is it connected and switched on?')
    out_dir = args.out or 'selftest_' + datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    os.makedirs(out_dir, exist_ok=True)

    badge = Badge(port)
    badge.start()
    t = Tester(badge, out_dir, args.verbose)
    try:
        # Connection (the Badge thread reconnects by itself after the reboot)
        deadline = time.time() + 5
        while not badge.connected() and time.time() < deadline:
            time.sleep(0.1)
        if not badge.connected():
            sys.exit(f'cannot open {port}: used by another application?')
        if not args.no_reboot:
            t.mark()
            t.keys('R')
            t.expect(r'^--- disconnected$', 5)
            deadline = time.time() + 20
            while not badge.connected() and time.time() < deadline:
                time.sleep(0.2)
            t.pump(3)  # Boot: the screen is cleared, the menu drawn
            if not badge.connected():
                sys.exit('the badge did not come back after the reboot')
        # The badge answers
        t.mark()
        t.keys('!')
        if not t.expect(r'^social: ', 5):
            sys.exit('the badge does not answer: is the menu application (badge_menu) flashed?')

        for g in groups:
            print(f'--- {g}')
            try:
                TESTS[g](t)
            except Exception as e:  # A broken group doesn't stop the others
                t.result(g, False, f'{type(e).__name__}: {e}')
    finally:
        badge.stop()

    failed = [r for r in t.results if r[1] == 'FAIL']
    print(f'\n{sum(r[1] == "PASS" for r in t.results)} passed, {len(failed)} failed, '
          f'{sum(r[1] == "SKIP" for r in t.results)} skipped. Screenshots in {out_dir}')
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
