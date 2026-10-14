#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""Capacity of the radio network (docs/analyses/reseau_100_badges.md). Monte-Carlo of the beacons of N badges in one room (all hear all): share of the beacons a badge receives from a
given neighbour, and the probability of a meeting (3 beacons received within 10 s), for several options.
Simplified: one room (every badge hears every other one), no capture effect, the beacons only (no other traffic),
the table of the neighbours not modelled. Usage: python tools/net_capacity_sim.py [badges...]"""
import random

PREAMBLE, SYNC, LEN, CRC = 4, 2, 1, 2


def airtime_ms(payload_bytes, kbps):
    return (PREAMBLE + SYNC + LEN + payload_bytes + CRC) * 8 / kbps + 0.8  # + the calibration before TX


def run(n, period_ms, jitter_ms, air_ms, sense_ms, backoff_ms, seconds=120, seed=1):
    """sense_ms: a sender sees the channel busy once another transmission has lasted this long (None: no sensing).
    Returns (load, p_receive)."""
    rnd = random.Random(seed)
    events = []  # (start, end, sender)
    nxt = [rnd.uniform(0, period_ms) for _ in range(n)]
    busy = []  # transmissions in progress (start, end)
    t_end = seconds * 1000
    # Sort-of discrete event: process the next beacon attempt in time order
    import heapq
    heap = [(nxt[i], i) for i in range(n)]
    heapq.heapify(heap)
    while heap:
        t, i = heapq.heappop(heap)
        if t > t_end:
            break
        busy = [b for b in busy if b[1] > t]
        if sense_ms is not None and any(t - b[0] >= sense_ms for b in busy):
            heapq.heappush(heap, (t + rnd.uniform(5, backoff_ms), i))  # Busy: try again a bit later
            continue
        events.append((t, t + air_ms, i))
        busy.append((t, t + air_ms))
        heapq.heappush(heap, (t + period_ms - jitter_ms + rnd.uniform(0, 2 * jitter_ms), i))
    events.sort()
    # A beacon (s, e, i) is received by r when no other transmission overlaps it and r does not send meanwhile
    ok = total = 0
    rec = {}
    for k, (s, e, i) in enumerate(events):
        clean = True
        j = k - 1
        while j >= 0 and events[j][0] > s - 100:
            if events[j][1] > s:
                clean = False
            j -= 1
        j = k + 1
        while j < len(events) and events[j][0] < e:
            clean = False
            j += 1
        # Receivers: all the others (collisions hit everyone in the same room)
        total += n - 1
        if clean:
            ok += n - 1
        rec.setdefault(i, []).append((s, clean))
    load = sum(e - s for s, e, _ in events) / t_end
    p = ok / total if total else 0
    # Meeting: 3 clean beacons of the same sender within 10 s, a 10 s window starting at the first close beacon
    met = []
    for i, lst in rec.items():
        times = [s for s, c in lst if c]
        found = None
        for a in range(len(times) - 2):
            if times[a + 2] - times[a] <= 10000:
                found = times[a + 2]
                break
        met.append(found is not None and found < 60000)
    return load, p, sum(met) / len(met)


OPTIONS = [
    ('Aujourd\'hui (attend seulement une synchro)', dict(pl=27, kbps=9.99, period=2000, jit=500, sense=4.8)),
    ('+ écoute avant d\'émettre (CCA du CC1101)', dict(pl=27, kbps=9.99, period=2000, jit=500, sense=1.0)),
    ('+ balise courte (10 o)', dict(pl=10, kbps=9.99, period=2000, jit=500, sense=1.0)),
    ('+ 38,4 kbit/s', dict(pl=10, kbps=38.4, period=2000, jit=500, sense=0.5)),
    ('+ période adaptative (5 s à 100 voisines)', dict(pl=10, kbps=38.4, period=5000, jit=1250, sense=0.5)),
    ('10 kbit/s, 21 o, période 5 s + CCA', dict(pl=27, kbps=9.99, period=5000, jit=1250, sense=1.0)),
]

if __name__ == '__main__':
    import sys
    sys.stdout.reconfigure(encoding='utf-8')
    for n in [int(a) for a in sys.argv[1:]] or (20, 50, 100):
        print(f'--- {n} badges')
        for name, o in OPTIONS:
            air = airtime_ms(o['pl'], o['kbps'])
            load, p, met = run(n, o['period'], o['jit'], air, o['sense'], 60)
            print(f'  {name:48s} air {air:5.1f} ms  charge {100 * load:5.1f} %  balise reçue {100 * p:5.1f} %'
                  f'  rencontre < 60 s {100 * met:5.1f} %')
