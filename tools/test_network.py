#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Test of the radio network between 2 badges or more plugged in USB, without touching their menus or their settings:
- identity: the version, the name, the radio chip of each badge ("!" key);
- pings: each badge in turn sends --rounds pings ("P" key, NET_PING at +10 dBm), every other badge must hear them
  ("net: ping #n from <id>, rssi ... dBm"): a matrix of the share received and of the mean RSSI, for each link;
- beacons: after the pings, each badge must list every other one among its neighbours (Social, "!" key);
- the counters of the network: no packet dropped by the sender during the test (queue full, transmission failed:
  "net: sent / received / dropped").

A link passes when at least --min-percent % of the pings arrive (default 80). Mute mode, admin mode, scores: nothing
changes (a ping has no effect but its log line).

Usage:
    python tools/test_network.py --ports COM9 COM11 COM13 [--rounds 10] [--min-percent 80] [-v]

Exit code 0 when every test passed. Close the other applications that use the serial ports first.
"""

import argparse
import re
import sys
import threading
import time

import serial


class Badge:
    """A badge on its serial port: the log lines read in the background."""

    def __init__(self, port, verbose):
        self.port = port
        self.verbose = verbose
        self.ser = serial.Serial(port, 115200, timeout=0.1, write_timeout=2)
        self.lines = []
        self.lock = threading.Lock()
        self.running = True
        self.thread = threading.Thread(target=self.read, daemon=True)
        self.thread.start()
        self.name = self.id = self.version = self.radio = None

    def read(self):
        buf = b''
        while self.running:
            try:
                buf += self.ser.read(4096)
            except serial.SerialException:
                break
            *done, buf = buf.split(b'\n')
            for raw in done:
                line = raw.decode('utf-8', errors='replace').rstrip('\r')
                with self.lock:
                    self.lines.append(line)
                if self.verbose:
                    print(f'    [{self.port}] {line}')

    def send(self, keys):
        self.ser.write(keys.encode())

    def mark(self):
        with self.lock:
            return len(self.lines)

    def since(self, mark):
        with self.lock:
            return self.lines[mark:]

    def diag(self):
        """The "!" lines."""
        m = self.mark()
        self.send('!')
        time.sleep(1.5)
        return self.since(m)

    def close(self):
        self.running = False
        self.thread.join(1)
        self.ser.close()


def counters(lines):
    for line in lines:
        m = re.match(r'^net: sent (\d+), received (\d+), dropped (\d+)', line)
        if m:
            return tuple(int(v) for v in m.groups())
    return None


def main():
    parser = argparse.ArgumentParser(description='Radio network test between badges')
    parser.add_argument('--ports', nargs='+', required=True, help='serial ports of the badges (2 or more)')
    parser.add_argument('--rounds', type=int, default=10, help='pings sent by each badge (10)')
    parser.add_argument('--min-percent', type=int, default=80, help='share of the pings a link must receive (80)')
    parser.add_argument('-v', '--verbose', action='store_true', help='print the log lines of the badges')
    args = parser.parse_args()
    if len(args.ports) < 2:
        parser.error('2 badges or more')

    badges = []
    try:
        for port in args.ports:
            badges.append(Badge(port, args.verbose))
    except serial.SerialException as e:
        sys.exit(f'cannot open a port: {e}')
    results = []

    def result(name, ok, detail=''):
        results.append(ok)
        print(f'[{"PASS" if ok else "FAIL"}] {name}' + (f' - {detail}' if detail else ''))

    try:
        time.sleep(1)
        print('--- identity')
        before = {}
        for b in badges:
            lines = b.diag()
            for line in lines:
                m = re.match(r'^social: \w+, (.+), score \d+', line)
                if m:
                    b.name = m.group(1)
                m = re.match(r'^version: (.+)$', line)
                if m:
                    b.version = m.group(1)
                m = re.match(r'^radio: (CC1101 version 0x[0-9a-fA-F]+)', line)
                if m:
                    b.radio = m.group(1)
            before[b.port] = counters(lines)
            result(f'{b.port}: answers', b.name is not None and before[b.port] is not None,
                   f'{b.name}, {b.version}, {b.radio}')

        print(f'--- pings ({args.rounds} per badge)')
        heard = {(s.port, r.port): [] for s in badges for r in badges if s is not r}  # RSSI of each ping received
        for s in badges:
            marks = {r.port: r.mark() for r in badges}
            for _ in range(args.rounds):
                s.send('P')
                time.sleep(0.4)
            time.sleep(1)
            sent = [line for line in s.since(marks[s.port]) if re.match(r'^net: ping #\d+ sent$', line)]
            for r in badges:
                if r is s:
                    continue
                for line in r.since(marks[r.port]):
                    m = re.match(r'^net: ping #\d+ from ([0-9A-F]{8}), rssi (-?\d+) dBm', line)
                    if m:
                        s.id = s.id or m.group(1)
                        if m.group(1) == s.id:
                            heard[(s.port, r.port)].append(int(m.group(2)))
            if len(sent) != args.rounds:
                result(f'{s.port}: pings sent', False, f'{len(sent)} / {args.rounds}')
        # The matrix: rows send, columns receive
        width = max(len(b.port) for b in badges) + 2
        print('  sender \\ receiver'.ljust(22) + ''.join(b.port.rjust(16) for b in badges))
        for s in badges:
            row = f'  {s.port} ({s.name})'[:22].ljust(22)
            for r in badges:
                if r is s:
                    row += '-'.rjust(16)
                    continue
                rssi = heard[(s.port, r.port)]
                cell = f'{100 * len(rssi) // args.rounds}%' + (f' {sum(rssi) // len(rssi)}dBm' if rssi else '')
                row += cell.rjust(16)
            print(row)
        for (sp, rp), rssi in heard.items():
            percent = 100 * len(rssi) // args.rounds
            result(f'ping {sp} -> {rp}', percent >= args.min_percent,
                   f'{len(rssi)}/{args.rounds}' + (f', RSSI {min(rssi)} .. {max(rssi)} dBm' if rssi else ''))

        print('--- beacons (neighbours)')
        for b in badges:
            lines = b.diag()
            seen = {}
            for line in lines:
                m = re.match(r'^  (.+) (-?\d+) dBm( \(met\))?$', line)
                if m:
                    seen[m.group(1)] = int(m.group(2))
            others = [o.name for o in badges if o is not b]
            missing = [n for n in others if n not in seen]
            result(f'{b.port}: neighbours', not missing,
                   ', '.join(f'{n} {seen[n]} dBm' for n in others if n in seen) +
                   (f'; missing: {", ".join(missing)}' if missing else ''))
            after = counters(lines)
            if after and before[b.port]:
                dropped = after[2] - before[b.port][2]
                result(f'{b.port}: network counters', dropped == 0,
                       f'sent +{after[0] - before[b.port][0]}, received +{after[1] - before[b.port][1]}, '
                       f'dropped +{dropped}')
    finally:
        for b in badges:
            b.close()
    failed = results.count(False)
    print(f'\n{len(results) - failed} passed, {failed} failed')
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
