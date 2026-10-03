#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Battery life test: records the battery of the badges heard by radio, through a badge plugged in USB.

The badge under test runs on its battery, away from the USB, with Réglages > Batterie par radio: oui (its beacons
carry its battery, every ~2 s). Any other badge plugged in the PC hears them and writes on its serial port
"battery: <id> <name> raw <ADC> mv <mV> usb <0|1> rssi <dBm>"; this script keeps one sample per badge every
--interval seconds in a CSV file (time, elapsed, id, name, raw ADC, mV, USB, RSSI) and shows them.
The mV are 0 when the badge under test is not calibrated (Admin > Batterie (calibration)): the raw ADC still gives
the curve; --cal converts it afterwards (two points of the calibration of that badge: raw:mV,raw:mV).

    python tools/battery_log.py --port COM11                       (every badge heard, battery_<date>.csv)
    python tools/battery_log.py --port COM11 --name Tristan --interval 300
    python tools/battery_log.py --port COM11 --cal 2533:4150,2240:3700

Stop with Ctrl+C. The badge that listens must stay plugged in (and on its main menu or any page: the beacons are
received everywhere). If the badge under test stops being heard for long, its battery is empty: the last sample
gives the life.
"""

import argparse
import csv
import datetime
import os
import re
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from badge_remote import Badge, find_port  # noqa: E402

LINE = re.compile(r'^battery: ([0-9A-F]{8}) (.*?) raw (\d+) mv (\d+) usb (\d) rssi (-?\d+)(?: est (-?\d+))?$')


def calibration(text):
    """'raw1:mv1,raw2:mv2' -> function raw -> mV"""
    (r1, m1), (r2, m2) = [tuple(int(v) for v in p.split(':')) for p in text.split(',')]
    return lambda raw: round(m1 + (raw - r1) * (m2 - m1) / (r2 - r1))


def main():
    parser = argparse.ArgumentParser(description='Records the battery of the badges heard by radio (CSV)')
    parser.add_argument('--port', default=None, help='serial port of the listening badge (found by itself)')
    parser.add_argument('--name', default=None, help='only this badge (its name, or the start of its id)')
    parser.add_argument('--interval', type=int, default=60, help='seconds between two samples of a badge (60)')
    parser.add_argument('--cal', default=None, help='calibration of the badge under test: raw:mV,raw:mV')
    parser.add_argument('-o', '--output', default=None, help='CSV file (default battery_<date>.csv)')
    args = parser.parse_args()
    if sys.stdout.encoding and sys.stdout.encoding.lower() != 'utf-8':
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    to_mv = calibration(args.cal) if args.cal else None
    port = args.port or find_port()
    if not port:
        sys.exit('no badge found: plug a badge in USB (or --port)')
    out = args.output or 'battery_' + datetime.datetime.now().strftime('%Y%m%d_%H%M%S') + '.csv'
    badge = Badge(port)
    badge.start()
    deadline = time.time() + 5
    while not badge.connected() and time.time() < deadline:
        time.sleep(0.1)
    if not badge.connected():
        sys.exit(f'cannot open {port}: used by another application?')
    start = time.time()
    last = {}  # id -> time of its last sample
    new = not os.path.exists(out)
    with open(out, 'a', newline='', encoding='utf-8') as f:
        w = csv.writer(f)
        if new:
            w.writerow(['time', 'elapsed_min', 'id', 'name', 'raw', 'mv', 'mv_cal', 'usb', 'rssi', 'percent_est'])
        print(f'listening on {port}, writing {out} (Ctrl+C to stop)')
        try:
            while True:
                while not badge.logs.empty():
                    m = LINE.match(badge.logs.get())
                    if not m:
                        continue
                    bid, name, raw, mv, usb, rssi, est = m.groups()
                    est = int(est) if est is not None else -1  # The estimate of its automatic calibration (%)
                    if args.name and not (name.startswith(args.name) or bid.startswith(args.name.upper())):
                        continue
                    now = time.time()
                    if now - last.get(bid, 0) < args.interval:
                        continue
                    last[bid] = now
                    mv_cal = to_mv(int(raw)) if to_mv else ''
                    elapsed = (now - start) / 60
                    w.writerow([datetime.datetime.now().isoformat(timespec='seconds'), f'{elapsed:.1f}', bid, name,
                                raw, mv, mv_cal, usb, rssi, est if est >= 0 else ''])
                    f.flush()
                    level = f'{mv} mV' if int(mv) else (f'~{mv_cal} mV (cal)' if mv_cal != '' else
                                                        f'~{est} % (auto)' if est >= 0 else 'not calibrated')
                    print(f'{elapsed:7.1f} min  {name:<8} {bid}  ADC {raw:>4}  {level}  {"USB " if usb == "1" else ""}'
                          f'{rssi} dBm')
                time.sleep(0.2)
        except KeyboardInterrupt:
            pass
    badge.stop()
    for bid, t in last.items():
        print(f'{bid}: last heard {(time.time() - t) / 60:.1f} min ago')


if __name__ == '__main__':
    main()
