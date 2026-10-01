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

GROUPS = ['diag', 'menus', 'games', 'puzzles', 'ctf', 'settings', 'radio', 'ir', 'images', 'apps', 'admin', 'battery',
          'radio433', 'social', 'net']


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

THEMES = ['Médias', 'Jeux', 'Social', 'Radio & IR', 'Badge', 'Réglages']
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
    m = t.expect(r'^version: (\d+\.\d+\.\d+) \((\S+) (\S+)\)$')
    t.result('firmware version', m is not None, f'{m.group(1)}, commit {m.group(2)}, built {m.group(3)}' if m else
             'no version line (firmware older than the version numbers?)')
    m = t.expect(r'radio: CC1101 version 0x([0-9a-f]+), crystal used (\d+) Hz, measured (\d+) Hz')
    if not m:
        t.result('radio chip', False, 'no answer to "!"')
    else:
        t.result('radio chip', m.group(1) == '14', f'CC1101 version 0x{m.group(1)} (0x14 expected)')
        used, measured = int(m.group(2)), int(m.group(3))
        t.result('radio crystal', any(abs(used - f) <= 50000 for f in (26000000, 27000000)),  # Calibrated values too
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


PUZZLES = ['Démineur', '2048', 'Taquin', 'Sokoban', 'Mastermind', 'Pendu']


def test_puzzles(t):
    """Each puzzle: help page, start, a move, quit with a long press on the left wing."""
    if not open_theme(t, 'Jeux'):
        return t.result('puzzles', False, 'theme "Jeux" not shown')
    t.keys('x' * len(GAMES))
    for name in PUZZLES:
        ok = t.press('b', name)
        t.keys('b')  # Help page -> the game
        t.keys('x')  # A move (right)
        t.screenshot('puzzle_' + re.sub(r'\W+', '_', name), 1.0)
        ok = ok and t.press('A', 'Jeux')
        t.result(f'puzzle: {name}', ok)
        t.keys('x')
    t.keys('y' * (len(GAMES) + len(PUZZLES)))
    close_theme(t, 'Jeux')


def test_ctf(t):
    if not open_theme(t, 'Jeux'):
        return t.result('ctf', False, 'theme "Jeux" not shown')
    t.keys('x' * (len(GAMES) + len(PUZZLES) + 1))  # After the games, the puzzles and the blind test
    t.press('b', 'CTF')
    t.mark()
    t.keys('b')  # Type a code
    t.keys('yyxxababba')  # Konami: up up down down left right left right B A, with the badge buttons
    m = t.expect(r'^ctf: code (right|wrong)$', 4)
    t.screenshot('ctf')
    t.result('ctf: Konami code', m is not None and m.group(1) == 'right', m.group(0) if m else 'no answer')
    t.keys('a')  # Page -> CTF list
    t.press('a', 'Jeux')
    t.keys('y' * (len(GAMES) + len(PUZZLES) + 1))
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
    t.check_ui('infos', 'xxxb', 'Infos')  # After the screensaver, the remote and the mute settings
    t.screenshot('infos')
    t.check_ui('credits', 'b', 'Crédits')
    t.mark()
    t.keys('x')
    m = t.expect(r'^credits: (.+)$')
    t.screenshot('credits_2')
    t.result('credits: next page', m is not None, m.group(1) if m else '')
    t.check_ui('credits: back to infos', 'a', 'Infos')
    t.press('a', 'Réglages')
    t.keys('yyy')
    close_theme(t, 'Réglages')


def test_radio(t):
    if not open_theme(t, 'Radio & IR'):
        return t.result('radio', False, 'theme "Radio & IR" not shown')
    t.mark()
    t.keys('b')  # Radio : message (first line)
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


def test_social(t):
    """Social theme: radar, hot / cold; talk badge (Badge theme) with its states."""
    if not open_theme(t, 'Social'):
        return t.result('social', False, 'theme "Social" not shown')
    for name, shot in (('Contacts', 'contacts'), ('Radar des cigales', 'radar'), ('Chaud - froid', 'hotcold'),
                       ('Choeur', 'chorus')):
        ok = social_app(t, name)
        t.pump(2)
        t.screenshot(shot)
        t.result(f'social: {name}', ok and t.press('a', 'Social'))
        t.keys('y' * SOCIAL.index(name))
    close_theme(t, 'Social')
    if not open_theme(t, 'Badge'):
        return t.result('talk', False, 'theme "Badge" not shown')
    ok = t.press('xxb', 'Badge de talk')
    t.mark()
    t.keys('xxxx')  # Green, orange, red, angry
    states = [m for m in (t.expect(r'^talk: (\S.*)$', 2) for _ in range(4)) if m]
    t.screenshot('talk_angry', 1.0)
    t.keys('x')  # Off
    t.result('talk: states', ok and len(states) == 4 and t.press('a', 'Badge'),
             ', '.join(m.group(1) for m in states))
    t.keys('yy')
    close_theme(t, 'Badge')


SOCIAL = ['Réseau cigales', 'Messages', 'Contacts', 'Programme', 'Vote', 'Radar des cigales', 'Chaud - froid',
          'Virus des cigales', 'Choeur', 'Annonces']
ADMIN = ['Commandes radio', 'LEDs des cigales', 'Annonces (admin)', 'Annoncer un talk', 'Vote (admin)',
         'Choeur : lancer', 'Balise chaud-froid', 'Virus : patient zéro', 'Remise à zéro',
         'Batterie (calibration)', 'Type du badge',
         'Quitter le mode admin']


def open_admin(t):
    """From the main menu: the admin mode by the serial port (like the box of badge_remote.py), then the Admin
    theme, selected by it (test_admin checks the secret sequence of the flanks)."""
    t.mark()
    t.badge.send('\x01A')
    if not t.expect(r'^admin: on$', 3):
        return False
    t.pump(0.5)
    return t.press('b', 'Admin')


def admin_app(t, name):
    """In the Admin theme (first line selected): open an application."""
    return t.press('x' * ADMIN.index(name) + 'b', name)


def admin_back(t, name):
    t.press('a', 'Admin')
    t.keys('y' * ADMIN.index(name))


def social_app(t, name):
    return t.press('x' * SOCIAL.index(name) + 'b', name)


def social_back(t, name):
    t.press('a', 'Social')
    t.keys('y' * SOCIAL.index(name))


def test_net(t):
    """The social features with a single badge: loopback (the packets sent come back from a twin badge)."""
    t.mark()
    t.keys('L')
    if not t.expect(r'^net: loopback on$', 3):
        return t.result('net: loopback', False)
    if not open_admin(t):
        t.keys('L')
        return t.result('net: admin menu', False)
    # Vote: the admin badge opens a question, hears it as a voter, votes, counts the vote of the twin
    ok = admin_app(t, 'Vote (admin)')
    t.mark()
    t.keys('b')  # First question
    opened = t.expect(r'^vote: opened question 0$', 3)
    heard = t.expect(r'^vote: question 0 ', 6)
    admin_back(t, 'Vote (admin)')
    t.press('a', TOP)
    t.keys('x')  # From the admin menu (the last one) to the first theme
    open_theme(t, 'Social')
    ok &= social_app(t, 'Vote')
    t.screenshot('vote_voter', 1.0)
    t.mark()
    t.keys('xb')  # Second answer
    voted = t.expect(r'^vote: answer 1$', 3)
    counted = t.expect(r'^vote: 1 voter\(s\)$', 6)
    social_back(t, 'Vote')
    close_theme(t, 'Social')
    t.press('yb', 'Admin')
    admin_app(t, 'Vote (admin)')
    t.screenshot('vote_admin', 1.0)
    t.mark()
    t.keys('b')
    closed = t.expect(r'^vote: closed, 1 voter\(s\)$', 3)
    admin_back(t, 'Vote (admin)')
    t.result('net: vote', bool(ok and opened and heard and voted and counted and closed),
             f'opened {bool(opened)}, heard {bool(heard)}, voted {bool(voted)}, counted {bool(counted)}, closed {bool(closed)}')
    # Program: announce the third talk
    admin_app(t, 'Annoncer un talk')
    t.mark()
    t.keys('xxb')
    announced = t.expect(r'^program: talk 2 announced$', 3)
    t.result('net: program announce', announced is not None)
    admin_back(t, 'Annoncer un talk')
    # Infection: patient zero, then cured
    admin_app(t, 'Virus : patient zéro')
    t.mark()
    t.keys('b')
    zero = t.expect(r'^infection: patient zero$', 3)
    t.keys('y')
    cured = t.expect(r'^infection: this badge cured$', 3)
    t.result('net: infection', zero is not None and cured is not None)
    admin_back(t, 'Virus : patient zéro')
    t.press('a', TOP)
    t.keys('x')
    # Messages: write to everybody
    open_theme(t, 'Social')
    ok = social_app(t, 'Messages')
    t.mark()
    t.keys('bbb')  # Write, everybody, first message
    sent = t.expect(r'^message: sent ', 3)
    t.screenshot('message_sent', 1.0)
    t.keys('b')
    t.result('net: message', ok and sent is not None)
    social_back(t, 'Messages')
    close_theme(t, 'Social')
    # Program page and infection page
    open_theme(t, 'Social')
    ok = social_app(t, 'Programme')
    t.keys('xxb')
    t.screenshot('program_details', 1.0)
    t.keys('a')
    social_back(t, 'Programme')
    ok = ok and social_app(t, 'Virus des cigales')
    t.screenshot('infection')
    social_back(t, 'Virus des cigales')
    close_theme(t, 'Social')
    t.result('net: program and infection pages', ok)
    # Leave the admin mode and the loopback
    t.press('yb', 'Admin')
    t.mark()
    t.keys('x' * ADMIN.index('Quitter le mode admin') + 'b')
    t.expect(r'^admin: off$', 3)
    t.mark()
    t.keys('L')
    t.result('net: loopback off', t.expect(r'^net: loopback off$', 3) is not None)


def test_apps(t):
    """The applications of the Badge theme: name tag and lamp."""
    if not open_theme(t, 'Badge'):
        return t.result('apps', False, 'theme "Badge" not shown')
    ok = t.press('b', 'Badge nominatif')
    t.screenshot('nametag')
    t.result('app: name tag', ok and t.press('a', 'Badge'))
    ok = t.press('xb', 'Lampe')
    t.keys('x')  # Brighter
    t.screenshot('lamp')
    t.result('app: lamp', ok and t.press('a', 'Badge'))
    t.keys('y')
    close_theme(t, 'Badge')


def test_admin(t):
    """The secret sequence shows the admin menu; mute / unmute commands; leave the admin mode."""
    t.pump(8.5)  # The flanks pressed before don't count (the sequence must be typed within 8 s)
    t.mark()
    t.keys('yyxxyxyx')  # Left, left, right, right, left, right, left, right: the Admin theme is selected
    on = t.expect(r'^admin: (on)$', 3)
    t.result('admin: secret sequence', on is not None)
    if not on:
        return
    t.press('b', 'Admin')
    ok = t.press('b', 'Commandes radio')
    t.mark()
    t.keys('b')  # Muet
    muted = t.expect(r'^remote: command 0x02', 3)
    t.screenshot('admin_commands')
    t.keys('xb')  # Fin du mode muet
    unmuted = t.expect(r'^remote: command 0x03', 3)
    t.result('admin: mute / unmute', ok and muted is not None and unmuted is not None)
    t.keys('y')
    t.press('a', 'Admin')
    t.mark()
    t.keys('x' * ADMIN.index('Quitter le mode admin') + 'b')
    t.result('admin: leave', t.expect(r'^admin: off$', 3) is not None)


def test_radio433(t):
    """The OOK receivers: the pages open, the radio goes back to the network afterwards."""
    if not open_theme(t, 'Radio & IR'):
        return t.result('radio433', False, 'theme "Radio & IR" not shown')
    ok = t.press('xxb', 'Décodeur 433 MHz')
    t.pump(4)
    t.screenshot('decoder', 0.5)
    t.result('433: decoder page', ok and t.press('a', 'Radio & IR'))
    ok = t.press('xb', 'Station météo')
    t.pump(2)
    t.screenshot('weather', 0.5)
    t.result('433: weather page', ok and t.press('a', 'Radio & IR'))
    t.keys('yyy')
    close_theme(t, 'Radio & IR')
    # Back to the network: beacons again (the remote windows lend the radio a moment every second)
    t.pump(2)
    counts = []
    for _ in range(2):
        t.mark()
        t.keys('!')
        m = t.expect(r'social: (on|off), .* beacons sent (\d+)', 3)
        counts.append(int(m.group(2)) if m else None)
        t.pump(6)
    ok = None not in counts and counts[1] > counts[0]
    t.result('433: network after the OOK receivers', ok, f'{counts[1] - counts[0]} beacon(s) in 6s' if ok else str(counts))


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


def test_battery(t):
    """Admin > Batterie (calibration): 2 points make the measure calibrated, then they are cleared. Only on a badge
    not calibrated yet: the points are factory settings, a real calibration is not touched."""
    t.mark()
    t.keys('!')
    m = t.expect(r'^battery: (not calibrated)?', 5)
    if not m or not m.group(1):
        t.result('battery: calibration', None, 'already calibrated: left as it is')
        return
    if not open_admin(t) or not admin_app(t, 'Batterie (calibration)'):
        t.result('battery: calibration', False, 'page not opened')
        return
    t.screenshot('battery_calibration')
    t.mark()
    t.keys('xb')  # Enregistrer le point (the voltage proposed: 4.20 V on USB)
    p1 = t.expect(r'^battery: point 1 set, ADC raw (\d+) = (\d+) mV', 3)
    t.expect(r'^store: factory settings saved \(ok\)', 3)
    t.keys('y' + 'a' * 50 + 'xb')  # 0.50 V less, but the same ADC value...
    p2 = t.expect(r'^battery: point 1 set', 3)  # ...replaces point 1: 2 points need 2 different charges
    t.mark()
    t.keys('!')
    still = t.expect(r'^battery: not calibrated', 5)  # One point (the same ADC value twice): never a wrong value
    t.keys('xb')  # Effacer: asks a confirmation...
    t.pump(0.5)
    t.mark()
    t.keys('b')  # ...cleared
    cleared = t.expect(r'^battery: calibration cleared', 3)
    t.result('battery: calibration', p1 is not None and p2 is not None and still is not None and cleared is not None,
             f'point {p1.group(1)} = {p1.group(2)} mV' if p1 else 'no point saved')
    t.keys('ya')  # Back (from "Enregistrer")
    t.press('a', TOP)
    t.mark()
    t.badge.send('\x01a')
    t.expect(r'^admin: off$', 3)


TESTS = {'diag': test_diag, 'menus': test_menus, 'games': test_games, 'puzzles': test_puzzles, 'ctf': test_ctf,
         'settings': test_settings, 'radio': test_radio, 'ir': test_ir, 'images': test_images,
         'apps': test_apps, 'admin': test_admin, 'battery': test_battery, 'radio433': test_radio433, 'social': test_social,
         'net': test_net}


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
            t.expect(r'^--- disconnected', 5)
            deadline = time.time() + 20
            while not badge.connected() and time.time() < deadline:
                time.sleep(0.2)
            t.pump(3)  # Boot: the screen is cleared, the menu drawn
            if t.expect(r"^ui: Réglage radio$", 2):  # A badge not tuned yet: the tuning of the radio first
                t.expect(r"^tune: crystal", 25)
                t.keys("a")
                t.pump(2)
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
