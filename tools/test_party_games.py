#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Test of the group games with two badges plugged in USB (menu application, badge_menu): Jeux > Tir à la corde,
then Jeux > Assassin. The buttons are simulated through the serial ports (see badge_selftest.py), the games are
followed in the log lines of the badges ("party: ...", "tug: ...", "assassin: ...").

- Tir à la corde: badge 1 creates the party, badge 2 joins it, badge 1 starts it; both show the teams (1 against 1)
  and the countdown; badge 2 pulls (left wing, right wing...) while badge 1 does nothing: badge 2's team must win,
  with the same result on both badges.
- Assassin: same lobby; the assassin needs 3 players, 2 are allowed when the host is in admin mode: the script puts
  badge 1 in admin mode (and switches it back off at the end if it was off). Each badge gets the other one as its
  target; badge 1 eliminates badge 2 (press D): the badges must be very close (ASSASSIN_KILL_RSSI in assassin.c,
  -50 dBm by default: lay them side by side). The RSSI measured by the victim is printed: use it to calibrate.

Both badges are rebooted first ("R" key) so that the menus start from a known state.

Usage:
    python tools/test_party_games.py --ports COM9 COM11 [--only tug,assassin] [--out party_test] [-v]

Exit code 0 when no test failed. Close the other applications that use the serial ports first.
"""

import argparse
import datetime
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from badge_remote import Badge  # noqa: E402
from badge_selftest import Tester, open_theme, TOP  # noqa: E402

JEUX = ['Morpion', 'Puissance 4', 'Simon', 'Réflexes', 'Snake', 'Démineur', '2048', 'Taquin', 'Sokoban', 'Mastermind',
        'Pendu', 'Blind test', 'CTF', 'Crypto', 'Duel', 'Bataille navale', 'Loup-garou', 'Assassin', 'Tir à la corde']
TUG = JEUX.index('Tir à la corde')  # Order of the "Jeux" theme in src/menu/main.c (SUBMENUS)
ASSASSIN = JEUX.index('Assassin')


class Duo:
    """The two badges, with a prefix for the messages."""

    def __init__(self, t1, t2):
        self.t = [t1, t2]
        self.results = []

    def result(self, name, ok, detail=''):
        status = 'SKIP' if ok is None else 'PASS' if ok else 'FAIL'
        self.results.append((name, status, detail))
        print(f'[{status}] {name}' + (f' - {detail}' if detail else ''))
        return ok

    def expect_both(self, pattern, timeout):
        """The same log line on both badges (in any order). Returns the two matches (None when missing)."""
        m1 = self.t[0].expect(pattern, timeout)
        m2 = self.t[1].expect(pattern, timeout if m1 is None else max(timeout / 2, 2))
        return m1, m2


def connect(badge, port):
    deadline = time.time() + 5
    while not badge.connected() and time.time() < deadline:
        time.sleep(0.1)
    if not badge.connected():
        sys.exit(f'cannot open {port}: used by another application?')


def reboot(t):
    t.mark()
    t.keys('R')
    t.expect(r'^--- disconnected', 5)
    deadline = time.time() + 20
    while not t.badge.connected() and time.time() < deadline:
        time.sleep(0.2)
    t.pump(3)
    if t.expect(r'^ui: Réglage radio$', 2):  # A badge not tuned yet: the tuning of the radio first
        t.expect(r'^tune: crystal', 25)
        t.keys('a')
        t.pump(2)
    if not t.badge.connected():
        sys.exit('a badge did not come back after the reboot')


def admin_state(t):
    """True / False from the "!" debug key, None when the badge doesn't answer."""
    t.mark()
    t.keys('!')
    m = t.expect(r'^remote: .*admin (on|off)', 5)
    return None if m is None else m.group(1) == 'on'


def open_game(t, index, name):
    """From the main menu (first theme selected): Jeux, then the game."""
    if not open_theme(t, 'Jeux'):
        return False
    return t.press('x' * index + 'b', name, 5)


def close_game(t, index):
    """From the first page of the game: back to the main menu, first theme selected."""
    t.press('a', 'Jeux', 5)
    t.keys('y' * index)
    t.press('a', TOP, 5)
    t.keys('y')  # Jeux -> Médias


def lobby(d, prefix):
    """Badge 1 creates the party, badge 2 joins it. Returns True when both are in the lobby."""
    t1, t2 = d.t
    t1.mark()
    t1.keys('b')  # Créer une partie
    d.result(f'{prefix}: create', t1.expect(rf'^{prefix}: hosting$', 5) is not None)
    t2.mark()
    t2.keys('x')  # Rejoindre une partie
    t2.keys('b')
    if not d.result(f'{prefix}: scan', t2.expect(rf'^{prefix}: scanning$', 5) is not None):
        return False
    # The open party is heard within a second or two: D joins the first one of the list (the closest)
    joined = None
    deadline = time.time() + 15
    while joined is None and time.time() < deadline:
        t2.pump(1.5)
        t2.keys('b')
        joined = t2.expect(rf'^{prefix}: joining (.+)$', 1)
    if not d.result(f'{prefix}: join', joined is not None, joined.group(1) if joined else 'no party heard'):
        return False
    ok = t2.expect(r'^party: joined, (\d+) players', 10)
    d.result(f'{prefix}: joined (badge 2)', ok is not None, f'{ok.group(1)} players' if ok else '')
    ok1 = t1.expect(r'^party: .* joined \(2 players\)', 10)
    return d.result(f'{prefix}: joined (badge 1 sees 2 players)', ok1 is not None) and ok is not None


def test_tug(d):
    t1, t2 = d.t
    for i, t in enumerate(d.t):
        if not d.result(f'tug: open (badge {i + 1})', open_game(t, TUG, 'Tir à la corde')):
            return
    if not lobby(d, 'tug'):
        return
    for t in d.t:
        t.mark()
    t1.keys('b')  # Lancer
    d.result('tug: start', t1.expect(r'^tug: start, 2 players$', 5) is not None)
    m = d.expect_both(r'^tug: teams of 2 players, seed (\w+), I am in (\w+)$', 10)
    ok = all(m) and m[0].group(1) == m[1].group(1) and m[0].group(2) != m[1].group(2)
    d.result('tug: teams (same seed, one per team)', ok,
             ', '.join(f'badge {i + 1}: {x.group(2)}' for i, x in enumerate(m) if x) if any(m) else 'not shown')
    t1.screenshot('tug_teams_1')
    d.result('tug: countdown', all(d.expect_both(r'^tug: countdown 3$', 10)))
    if not d.result('tug: pull', all(d.expect_both(r'^tug: pull!$', 6))):
        return
    t2.screenshot('tug_pull_2', 0.2)
    # Badge 2 pulls (left wing, right wing...) until the end, badge 1 doesn't
    start = time.time()
    while time.time() - start < 21:
        t2.keys('ab', 0.08)
        t2.pump()
        if any(re.search(r'^tug: (time is up|stopped by|the \w+ reached)', l) for l in t2.lines[t2.cursor:]):
            break
    t2.pump(0.5)
    t1.screenshot('tug_rope_1', 0.2)
    pulls = [l for l in t2.lines if re.match(r'^tug: \d+ pulls$', l)]
    d.result('tug: pulls counted', bool(pulls), pulls[-1] if pulls else '')
    r = d.expect_both(r'^tug: result Cigales (\d+) - (\d+) Fourmis, winner (\S+).*, me (\w+) (\d+) pulls', 15)
    if not d.result('tug: result on both badges', all(r)):
        return
    t1.screenshot('tug_result_1')
    t2.screenshot('tug_result_2')
    d.result('tug: same winner', r[0].group(3) == r[1].group(3), f'{r[0].group(3)} / {r[1].group(3)}')
    d.result('tug: the puller won', r[1].group(4) == 'won' and r[0].group(4) == 'lost',
             f'badge 1 {r[0].group(4)}, badge 2 {r[1].group(4)} with {r[1].group(5)} pulls; totals '
             f'{r[0].group(1)}-{r[0].group(2)} / {r[1].group(1)}-{r[1].group(2)}')
    for t in d.t:
        t.keys('b')  # Rejouer: back to the first page
        t.pump(1)
        close_game(t, TUG)


def test_assassin(d, admin_was):
    t1, t2 = d.t
    if not admin_was:
        t1.mark()
        t1.badge.send('\x01A')  # 2 players are allowed when the host is in admin mode
        if not d.result('assassin: admin mode on badge 1', t1.expect(r'^admin: on$', 3) is not None):
            return
        t1.pump(0.5)
        t1.keys('x')  # The Admin theme (the last one) was selected: back to the first one
    for i, t in enumerate(d.t):
        if not d.result(f'assassin: open (badge {i + 1})', open_game(t, ASSASSIN, 'Assassin')):
            return
    if not lobby(d, 'assassin'):
        return
    for t in d.t:
        t.mark()
    t1.keys('b')  # Lancer
    d.result('assassin: start', t1.expect(r'^assassin: start, 2 players$', 5) is not None)
    d.result('assassin: game started', all(d.expect_both(r'^assassin: game started', 10)))
    d.result('assassin: targets sent', t1.expect(r'^assassin: the host sends the targets$', 10) is not None)
    d.result('assassin: target received (badge 2)', t2.expect(r'^assassin: target received$', 15) is not None)
    m = d.expect_both(r'^assassin: my target is (.+)$', 15)  # The host: right after the draw
    if not d.result('assassin: targets', all(m), ' / '.join(x.group(1) for x in m if x)):
        return
    d.result('assassin: target acknowledged', t1.expect(r'acknowledged$', 15) is not None)
    t1.screenshot('assassin_target_1')
    # Badge 1 eliminates badge 2: up to 3 attempts (the badges must almost touch)
    killed = None
    rssi = []
    for attempt in range(3):
        for t in d.t:
            t.mark()
        t1.keys('b')
        t1.expect(r'^assassin: kill attempt on', 3)
        end = time.time() + 6
        while time.time() < end and killed is None:
            t2.pump(0.2)
            for line in t2.lines[t2.cursor:]:
                k = re.match(r'^assassin: KILL from .*, rssi (-?\d+)$', line)
                if k:
                    rssi.append(int(k.group(1)))
                if re.match(r'^assassin: killed by ', line):
                    killed = line
        if killed:
            break
        print(f'    attempt {attempt + 1}: refused or not heard, RSSI {rssi[-3:]} (bring the badges closer)')
        t1.pump(5)  # "Trop loin ou absente"
    rssi_text = f'RSSI of the KILL packets: {min(rssi)}..{max(rssi)} dBm' if rssi else 'no KILL packet heard'
    if not d.result('assassin: kill accepted by badge 2', killed is not None, rssi_text):
        return
    t2.screenshot('assassin_dead_2')
    d.result('assassin: killer gets the new target', t1.expect(r'^assassin: killed .*, new target ', 10) is not None)
    d.result('assassin: victory (badge 1)', t1.expect(r'^assassin: victory', 10) is not None)
    d.result('assassin: game over (badge 2)', t2.expect(r'^assassin: (game over, winner|party cancelled)', 15)
             is not None)
    t1.screenshot('assassin_victory_1')
    for t in d.t:
        t.keys('b')  # Nouvelle partie: back to the first page
        t.pump(1)
        close_game(t, ASSASSIN)


def main():
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    parser = argparse.ArgumentParser(description='Test of the group games with two badges')
    parser.add_argument('--ports', nargs=2, required=True, metavar='PORT', help='serial ports of badge 1 and badge 2')
    parser.add_argument('--only', help='comma separated: tug, assassin')
    parser.add_argument('--out', default=None, help='directory of the screenshots (default: party_<date>)')
    parser.add_argument('--no-reboot', action='store_true', help='start from the current pages (main menus)')
    parser.add_argument('--verbose', '-v', action='store_true', help='show the log lines of the badges')
    args = parser.parse_args()
    groups = args.only.split(',') if args.only else ['tug', 'assassin']
    out_dir = args.out or 'party_' + datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    os.makedirs(out_dir, exist_ok=True)

    badges = [Badge(p) for p in args.ports]
    for b in badges:
        b.start()
    testers = [Tester(b, out_dir, args.verbose) for b in badges]
    d = Duo(*testers)
    admin_was = None
    try:
        for b, p in zip(badges, args.ports):
            connect(b, p)
        if not args.no_reboot:
            for t in testers:
                reboot(t)
        admin_was = admin_state(testers[0])
        if admin_was is None or admin_state(testers[1]) is None:
            sys.exit('a badge does not answer: is the menu application (badge_menu) flashed?')
        for g in groups:
            print(f'--- {g}')
            try:
                if g == 'tug':
                    test_tug(d)
                elif g == 'assassin':
                    test_assassin(d, admin_was)
                else:
                    print(f'unknown group {g}')
            except Exception as e:
                d.result(g, False, f'{type(e).__name__}: {e}')
    finally:
        if admin_was is False:
            testers[0].badge.send('\x01a')  # Back to the mode of before the test
            testers[0].pump(0.5)
        for b in badges:
            b.stop()

    failed = [r for r in d.results if r[1] == 'FAIL']
    print(f'\n{sum(r[1] == "PASS" for r in d.results)} passed, {len(failed)} failed. Screenshots in {out_dir}')
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
