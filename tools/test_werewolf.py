#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Test of the loup-garou (Jeux > Loup-garou) with 2 badges, through their USB serial ports: the first badge is the
narrator ("meneur"), the second one the only real player. The narrator badge is put in admin mode: its party then
starts with a single player, robots complete it up to the minimum (5 in simple mode, 7 in advanced mode); its admin
mode is put back as it was at the end.

The player always chooses the first line of the lists (D), the narrator skips the phases without choices (débat,
aube, verdict...). The script follows the game in the logs ("werewolf: ..." lines) and checks:
- the role received by the player is the one dealt by the narrator,
- each choice of the player reaches the narrator (acknowledged: "Choix reçu"),
- both badges see the same phases and the same end,
- no text is cut or drawn under the footer (the check of the texts of ui.c, "U" key).
Screenshots of the pages in --out.

Usage:
    python tools/test_werewolf.py --ports COM9 COM11 [--advanced] [--no-reboot] [--out werewolf_test] [-v]

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
from badge_selftest import Tester  # noqa: E402

TOP = 'Badge SecSea'
JEUX_INDEX = 1  # The themes of the main menu (main.c SUBMENUS): Médias, Jeux...
WEREWOLF_INDEX = 16  # Loup-garou in the Jeux theme (main.c SUBMENUS)
ACTIONS = ('roles', 'cupid', 'wolves', 'seer', 'witch', 'vote')  # Phases where every living player chooses
SKIPPED = ('dawn', 'debate', 'verdict', 'shot')  # Phases without choices: the narrator skips them
MAX_PHASES = 80
ROLES = 'Villageois|Loup-garou|Voyante|Sorcière|Chasseur|Cupidon'


class Stop(Exception):
    """A step failed: the next ones cannot run."""


def connect(t, port, reboot):
    """Opens the serial port, reboots the badge (menus in a known state), checks that it answers."""
    badge = t.badge
    deadline = time.time() + 5
    while not badge.connected() and time.time() < deadline:
        time.sleep(0.1)
    if not badge.connected():
        sys.exit(f'cannot open {port}: used by another application?')
    if reboot:
        t.mark()
        t.keys('R')
        t.expect(r'^--- disconnected', 5)
        deadline = time.time() + 20
        while not badge.connected() and time.time() < deadline:
            time.sleep(0.2)
        t.pump(3)
        if t.expect(r'^ui: Réglage radio$', 2):  # A badge not tuned yet: the tuning of the radio first
            t.expect(r'^tune: crystal', 25)
            t.keys('a')
            t.pump(2)
    t.mark()
    t.keys('!')
    if not t.expect(r'^social: ', 5):
        sys.exit(f'{port}: the badge does not answer: is the menu application (badge_menu) flashed?')


def admin_state(t):
    """The admin mode of the badge, from the answer to '!' (None when not found)."""
    t.mark()
    t.keys('!')
    m = t.expect(r'(?:^admin: |, admin )(on|off)\b', 3)
    return m.group(1) == 'on' if m else None


def open_werewolf(t, from_admin=False):
    """From the main menu (first theme selected, or the Admin theme just after the admin mode): Jeux > Loup-garou."""
    moves = 1 + JEUX_INDEX if from_admin else JEUX_INDEX  # From Admin (the last theme), the next one is the first
    if not t.press('x' * moves + 'b', 'Jeux'):
        return False
    return t.press('x' * WEREWOLF_INDEX + 'b', 'Loup-garou')


def lines_since(t, start, pattern):
    regex = re.compile(pattern)
    return [m for m in (regex.search(line) for line in t.lines[start:]) if m]


def main():
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    parser = argparse.ArgumentParser(description='Loup-garou with 2 badges: a narrator and a player (+ robots)')
    parser.add_argument('--ports', nargs=2, required=True, metavar=('NARRATOR', 'PLAYER'), help='serial ports')
    parser.add_argument('--advanced', action='store_true', help='advanced mode (sorcière, chasseur, Cupidon...)')
    parser.add_argument('--no-reboot', action='store_true', help='start from the current pages (main menus)')
    parser.add_argument('--out', default=None, help='directory of the screenshots (default: werewolf_<date>)')
    parser.add_argument('--verbose', '-v', action='store_true', help='show the log lines of the badges')
    args = parser.parse_args()
    out = args.out or 'werewolf_' + datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    os.makedirs(out, exist_ok=True)

    nb, pb = Badge(args.ports[0]), Badge(args.ports[1])
    nb.start()
    pb.start()
    na = Tester(nb, out, args.verbose)  # The narrator
    pl = Tester(pb, out, args.verbose)  # The player
    results = []

    def result(name, ok, detail=''):
        status = 'SKIP' if ok is None else 'PASS' if ok else 'FAIL'
        results.append((name, status))
        print(f'[{status}] {name}' + (f' - {detail}' if detail else ''))
        return ok

    def shot(t, name):
        t.screenshot(name)

    def require(name, ok, detail=''):
        if not result(name, ok, detail):
            raise Stop(name)

    admin_before = None
    try:
        connect(na, args.ports[0], not args.no_reboot)
        connect(pl, args.ports[1], not args.no_reboot)
        admin_before = admin_state(na)
        for t in (na, pl):
            t.keys('U')  # The check of the texts: "uicheck: ..." for a text cut or under the footer

        # ---- The narrator opens a party in test mode (admin) ----
        na.mark()
        nb.send('\x01A')
        require('narrator: admin mode', na.expect(r'^admin: on$', 3) is not None)
        na.pump(0.5)
        require('narrator: Jeux > Loup-garou', open_werewolf(na, from_admin=True))
        shot(na, 'menu')
        na.keys('b')  # Mener une partie (the inner pages of an app have no "ui:" trace)
        na.pump(0.5)
        if args.advanced:
            na.keys('b')  # Mode : avancé
        na.mark()
        na.keys('xxb')  # Ouvrir la partie
        m = na.expect(r'^werewolf: narrator, (\w+) mode, debate (\d+) s(, test mode)?$', 3)
        result('narrator: party open in test mode', m is not None and m.group(3) is not None,
               m.group(0) if m else 'no "werewolf: narrator" line')
        require('narrator: mode', m is not None and m.group(1) == ('advanced' if args.advanced else 'simple'),
                'the mode is kept since the last party: reboot the narrator badge (no --no-reboot)')
        shot(na, 'lobby_empty')

        # ---- The player joins ----
        require('player: Jeux > Loup-garou', open_werewolf(pl))
        pl.keys('xb')  # Rejoindre une partie
        pl.pump(3)  # The OPEN of the narrator, every second
        shot(pl, 'scan')
        pl.mark()
        na.mark()
        pl.keys('b')
        ok = pl.expect(r'^werewolf: joining ', 3) is not None and pl.expect(r'^party: joined', 8) is not None
        require('player: joined', ok and na.expect(r'^party: .* joined \(1 players\)', 3) is not None)
        shot(pl, 'waiting')
        na.pump(1.5)
        shot(na, 'lobby')

        # ---- Launch: roles dealt by the narrator, received by the player ----
        na.mark()
        pl.mark()
        na.keys('b')
        m = na.expect(r'^werewolf: launch, (\d+) players \((\d+) robots\)', 3)
        require('narrator: launch with robots', m is not None and int(m.group(2)) >= 4, m.group(0) if m else '')
        dealt = {}
        start = na.cursor
        na.pump(1)
        for mm in lines_since(na, start, r'^werewolf: (.+) is (' + ROLES + r')$'):
            dealt[mm.group(1)] = mm.group(2)
        human = [r for n, r in dealt.items() if not n.startswith('Robot')]
        m = pl.expect(r'^werewolf: role (.+)$', 15)
        role = m.group(1) if m else None
        result('player: role received', role is not None and human == [role],
               f'{role} (dealt: {", ".join(f"{n}={r}" for n, r in dealt.items())})')
        shot(pl, 'role')

        # ---- The phases ----
        alive = True
        phases_n, phases_p = [], []
        winner_n = winner_p = None
        won = None
        acks = fails = 0
        narrator_roles_shot = False
        for _ in range(MAX_PHASES):
            start = pl.cursor
            m = pl.expect(r'^werewolf: (?:phase (\w+), day (\d+)|end, (.+), I (won|lost))$', 240)
            pl.pump(1.0)  # The page is drawn; "I am dead" follows the phase
            if lines_since(pl, start, r'^werewolf: I am dead$'):
                alive = False
            if not m:
                result('player: next phase', False, 'nothing for 4 minutes')
                break
            if m.group(3):
                winner_p, won = m.group(3), m.group(4) == 'won'
                break
            phase, day = m.group(1), m.group(2)
            phases_p.append(phase)
            mn = na.expect(r'^werewolf: (?:phase (?!roles,)(\w+), day (\d+)|end, (.+))', 10)  # The player waits after the roles
            if mn and mn.group(3):
                winner_n = mn.group(3)
            elif mn:
                phases_n.append(mn.group(1))
            shot(pl, f'{len(phases_p):02d}_{phase}_{day}' + ('' if alive else '_dead'))
            if phase in ACTIONS and alive:
                na_start = na.cursor
                pl.keys('bxb' if phase == 'cupid' else 'b')  # Cupidon: 2 names (the others pretend the same way)
                ok = na.expect(r'^werewolf: .+ \((?:' + ROLES + r')\) (?:chose |is ready$)', 8) is not None
                acks += ok
                fails += not ok
                if not ok:
                    print(f'    no choice received by the narrator in phase {phase}')
                    na.cursor = na_start
                pl.pump(1.0)
                shot(pl, f'{len(phases_p):02d}_{phase}_{day}_chosen')
                if phase == 'wolves' and not narrator_roles_shot:
                    na.keys('B')  # The roles, on a separate page
                    na.pump(1.0)
                    shot(na, 'narrator_roles')
                    na.keys('a')
                    narrator_roles_shot = True
            elif phase == 'hunter':
                pl.keys('b')  # The player shoots if it is the hunter (the first name), nothing otherwise
            elif phase in SKIPPED or (phase in ACTIONS and not alive):
                na.pump(1.0)
                shot(na, f'narrator_{len(phases_p):02d}_{phase}')
                if phase in SKIPPED:
                    na.keys('b')  # Next phase
        else:
            result('game: ends', False, f'still going after {MAX_PHASES} phases')
        if winner_n is None:
            m = na.expect(r'^werewolf: end, (.+)$', 10)
            winner_n = m.group(1) if m else None
        result('player: every choice received by the narrator', fails == 0 and acks > 0,
               f'{acks} received, {fails} lost')
        same = phases_n == phases_p[:len(phases_n)] and len(phases_n) >= len(phases_p) - 1
        result('both badges: the same phases', same, 'player: ' + ' '.join(phases_p) + ' / narrator: ' + ' '.join(phases_n))
        result('game: the same end on both badges', winner_p is not None and winner_p == winner_n,
               f'{winner_n}; the player {"won" if won else "lost"}')
        for t, who in ((na, 'narrator'), (pl, 'player')):
            bad = sorted(set(m.group(0) for m in lines_since(t, 0, r'^uicheck: (?!on$|off$).*')))
            result(f'{who}: the texts fit on the screen', not bad, '; '.join(bad[:5]))
        pl.pump(1.5)
        shot(pl, 'end')
        shot(na, 'end_narrator')
        pl.keys('b')
        pl.pump(1.0)
        shot(pl, 'end_roles')

        # ---- Both leave (long press on the left wing, then "oui") ----
        pl.mark()
        pl.keys('a')  # The roles page -> the end page
        pl.keys('A')
        pl.pump(1.0)  # The quit page (no "ui:" trace)
        pl.keys('b')
        result('player: left', pl.expect(r'^werewolf: left$', 3) is not None)
        na.mark()
        na.keys('A')
        na.keys('b')
        result('narrator: left', na.expect(r'^werewolf: left$', 3) is not None)
        for t in (na, pl):
            t.press('a', 'Jeux')
            t.keys('y' * WEREWOLF_INDEX)
            t.press('a', TOP)
    except Stop:
        pass
    finally:
        for t in (na, pl):
            t.keys('U')  # The check of the texts off again
        if admin_before is False:
            nb.send('\x01a')  # The admin mode as it was
            na.expect(r'^admin: off$', 3)
        nb.stop()
        pb.stop()

    failed = [r for r in results if r[1] == 'FAIL']
    print(f'\n{sum(r[1] == "PASS" for r in results)} passed, {len(failed)} failed. Screenshots in {out}')
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
