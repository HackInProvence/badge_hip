#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Test of the smuggler cicada (Social > Contrebande) with two badges, through their USB serial ports:
an exchange, a gift, a refusal and a cancellation after the guest sealed its good. The first badge invites,
the second one answers. The buttons are simulated (keys a, b, x, y) and the log lines "smuggler: ..." are checked
on both sides; screenshots of the pages are saved.

The two badges must be "à portée de main": held against each other (RSSI >= SMUGGLER_TRADE_RSSI, smuggler.c),
the script waits for it. A badge never used gets a few goods at the first opening, enough for the test.
The cargo of the badges changes (that's the point): run it on test badges.

Usage:
    python tools/test_smuggler.py --ports COM9 COM11 [--out smuggler_test] [--no-reboot] [-v]

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
from badge_selftest import SOCIAL, Tester, open_theme  # noqa: E402

AT_HAND_S = 60  # Waiting for the other badge "à portée de main"


def connect(port, out_dir, verbose, reboot):
    badge = Badge(port)
    badge.start()
    t = Tester(badge, out_dir, verbose)
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
    return t


def open_smuggler(t, who):
    """From the main menu: Social > Contrebande."""
    ok = open_theme(t, 'Social')
    t.mark()
    t.keys('x' * SOCIAL.index('Contrebande') + 'b')
    ok = ok and t.expect(r'^ui: Contrebande$', 3) is not None
    seeded = t.expect(r'^smuggler: seeded the cargo: (.*)$', 1)
    if seeded:
        print(f'    {who}: first opening, cargo: {seeded.group(1)}')
    return t.result(f'{who}: Social > Contrebande', ok)


def wait_at_hand(t, who):
    """On the page "À portée de main": waits for a cicada close enough."""
    m = t.expect(r'^smuggler: at hand (.+)#([0-9A-F]{4}) (-?\d+) dBm$', 3)
    if not m:
        print(f'    {who}: hold the two badges against each other (waiting {AT_HAND_S} s)...')
        m = t.expect(r'^smuggler: at hand (.+)#([0-9A-F]{4}) (-?\d+) dBm$', AT_HAND_S)
    t.result(f'{who}: a cicada at hand', m is not None, f'{m.group(1)}#{m.group(2)} {m.group(3)} dBm' if m else
             'none: closer, or calibrate SMUGGLER_TRADE_RSSI')
    return m


def invite(a, kind):
    """Badge A, on the page of the cicadas at hand: invites the first one. Returns the trade id."""
    if not wait_at_hand(a, 'A'):
        return None
    a.mark()
    a.keys('b')
    m = a.expect(r'^smuggler: inviting .+, trade ([0-9A-F]{8}), ' + kind, 3)
    a.result(f'A: invitation ({kind})', m is not None)
    a.screenshot(f'inviting_{kind.split()[0]}', 0.8)  # The page of the inviter while it waits
    return m.group(1) if m else None


def answer(b, trade, key, kind):
    """Badge B: waits for the invitation, then accepts (b) or refuses (a)."""
    m = b.expect(rf'^smuggler: invited by .+, trade {trade}, {kind}', 10)
    b.result(f'B: invited ({kind})', m is not None)
    if not m:
        return False
    b.pump(0.5)
    b.screenshot(f'invited_{kind.split()[0]}', 0.8)
    b.mark()
    b.keys(key)
    return True


def done_lines(a, b, trade):
    da = a.expect(rf'^smuggler: done trade {trade} inviter gave=(-?\d+) got=(-?\d+)', 10)
    db = b.expect(rf'^smuggler: done trade {trade} guest gave=(-?\d+) got=(-?\d+)', 10)
    return da, db


def back_home(t):
    t.mark()
    t.keys('b')  # "D : continuer"
    return t.expect(r'^smuggler: page home$', 3) is not None


def test_exchange(a, b):
    a.mark()
    a.keys('xb')  # Échanger en douce
    a.expect(r'^smuggler: page near$', 3)
    trade = invite(a, 'exchange')
    if not trade or not answer(b, trade, 'b', 'exchange'):
        return a.result('exchange', False, 'no invitation')
    ok = b.expect(rf'^smuggler: accepted trade {trade}', 3) is not None
    ok &= a.expect(rf'^smuggler: trade {trade} accepted by', 10) is not None
    a.result('exchange: accepted on both badges', ok)
    # Each one offers its first good
    a.pump(1)
    a.screenshot('choose')
    a.mark()
    b.mark()
    a.keys('b')
    b.keys('b')
    oa, ob = a.expect(r'^smuggler: offer (.+)$', 3), b.expect(r'^smuggler: offer (.+)$', 3)
    ra, rb = a.expect(r'^smuggler: review (.+) <-> (.+)$', 10), b.expect(r'^smuggler: review (.+) <-> (.+)$', 10)
    ok = bool(oa and ob and ra and rb and ra.group(1) == rb.group(2) and ra.group(2) == rb.group(1))
    a.result('exchange: both offers seen on both badges', ok,
             f'A offers {oa.group(1) if oa else "?"}, B offers {ob.group(1) if ob else "?"}')
    a.screenshot('review')
    # The guest confirms first (its good is sealed), then the inviter: the decision
    b.keys('b')
    sealed = b.expect(r'^smuggler: confirmed, (.+) sealed$', 3)
    b.screenshot('sealed')
    a.expect(r'^smuggler: (?!confirmed).+ confirmed$', 10)
    a.mark()
    a.keys('b')
    da, db = done_lines(a, b, trade)
    ok = bool(sealed and da and db and da.group(1) == db.group(2) and da.group(2) == db.group(1))
    a.result('exchange: done on both badges, goods swapped', ok,
             f'A gave {da.group(1)} got {da.group(2)}, B gave {db.group(1)} got {db.group(2)}' if da and db else
             f'done A {bool(da)}, B {bool(db)}')
    a.screenshot('done')
    a.result('exchange: back to the home page', back_home(a) and back_home(b))


def test_gift(a, b):
    a.mark()
    a.keys('xb')  # From Échanger to Donner
    a.expect(r'^smuggler: page gift$', 3)
    a.screenshot('gift_pick')
    a.mark()
    a.keys('b')  # The first good
    a.expect(r'^smuggler: page near$', 3)
    trade = invite(a, 'gift of')
    if not trade or not answer(b, trade, 'b', 'gift of'):
        return a.result('gift', False, 'no invitation')
    da, db = done_lines(a, b, trade)
    ok = bool(da and db and da.group(1) == db.group(2) and da.group(2) == '-1' and db.group(1) == '-1')
    a.result('gift: given and received', ok, f'good {da.group(1)}' if da else '')
    b.screenshot('gift_received')
    a.result('gift: back to the home page', back_home(a) and back_home(b))


def test_refuse(a, b):
    a.mark()
    a.keys('yb')  # Back to Échanger
    a.expect(r'^smuggler: page near$', 3)
    trade = invite(a, 'exchange')
    if not trade or not answer(b, trade, 'a', 'exchange'):
        return a.result('refusal', False, 'no invitation')
    ok = b.expect(rf'^smuggler: refused trade {trade}', 3) is not None
    ok &= a.expect(rf'^smuggler: trade {trade} refused', 10) is not None
    a.screenshot('refused')
    a.result('refusal: seen by the inviter', ok)
    back_home(a)


def test_cancel_sealed(a, b):
    """B seals its good, A cancels: B gets its good back."""
    a.mark()
    a.keys('b')  # Échanger (still selected)
    a.expect(r'^smuggler: page near$', 3)
    trade = invite(a, 'exchange')
    if not trade or not answer(b, trade, 'b', 'exchange'):
        return a.result('cancel', False, 'no invitation')
    a.expect(rf'^smuggler: trade {trade} accepted by', 10)
    a.keys('b')
    b.keys('b')
    ok = a.expect(r'^smuggler: review ', 10) is not None and b.expect(r'^smuggler: review ', 10) is not None
    b.keys('b')
    sealed = b.expect(r'^smuggler: confirmed, (.+) sealed$', 3)
    a.expect(r'^smuggler: (?!confirmed).+ confirmed$', 10)
    a.mark()
    a.keys('a')  # Cancel: allowed until the inviter decides
    ok &= a.expect(rf'^smuggler: trade {trade} cancelled$', 3) is not None
    back = b.expect(r'^smuggler: (.+) back in the cargo$', 10)
    ok &= b.expect(rf'^smuggler: trade {trade} cancelled by the peer', 10) is not None
    ok = bool(ok and sealed and back and back.group(1) == sealed.group(1))
    b.screenshot('cancelled')
    a.result('cancel after the seal: the good is back', ok)
    back_home(a)
    back_home(b)


def main():
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    parser = argparse.ArgumentParser(description='Two badge test of the smuggler cicada (Social > Contrebande)')
    parser.add_argument('--ports', nargs=2, required=True, metavar=('INVITER', 'GUEST'), help='the two serial ports')
    parser.add_argument('--out', default=None, help='directory of the screenshots (default: smuggler_<date>)')
    parser.add_argument('--no-reboot', action='store_true', help='start from the current page (must be the main menu)')
    parser.add_argument('--verbose', '-v', action='store_true', help='show the log lines of the badges')
    args = parser.parse_args()
    out_dir = args.out or 'smuggler_' + datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    os.makedirs(os.path.join(out_dir, 'A'), exist_ok=True)
    os.makedirs(os.path.join(out_dir, 'B'), exist_ok=True)

    a = connect(args.ports[0], os.path.join(out_dir, 'A'), args.verbose, not args.no_reboot)
    b = connect(args.ports[1], os.path.join(out_dir, 'B'), args.verbose, not args.no_reboot)
    b.results = a.results  # One list of results
    try:
        if open_smuggler(a, 'A') and open_smuggler(b, 'B'):
            a.screenshot('home')
            for test in (test_exchange, test_gift, test_refuse, test_cancel_sealed):
                print(f'--- {test.__name__[5:]}')
                try:
                    test(a, b)
                except Exception as e:  # A broken test doesn't stop the others
                    a.result(test.__name__, False, f'{type(e).__name__}: {e}')
            a.keys('A')  # Long press: leave the page
            b.keys('A')
    finally:
        a.badge.stop()
        b.badge.stop()

    failed = [r for r in a.results if r[1] == 'FAIL']
    print(f'\n{sum(r[1] == "PASS" for r in a.results)} passed, {len(failed)} failed. Screenshots in {out_dir}')
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
