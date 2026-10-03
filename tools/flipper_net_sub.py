#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Flipper Zero and the network of the cicadas (GFSK 9.99 kbps, sync word 0xC16A, see src/menu/net.h):

1. Writes .sub files (RAW, custom GFSK preset) that make a Flipper send packets of the badges:
       python tools/flipper_net_sub.py command 0x02 -o mute.sub        (remote command: 0x02 mute, 0x03 unmute...;
                                                                        relayed by the cicadas, --ttl hops: 2)
       python tools/flipper_net_sub.py ping -o ping.sub
       python tools/flipper_net_sub.py raw 0x0F 01 -o any.sub           (type, then the data bytes in hex)
       python tools/flipper_net_sub.py pirates 8 --send                 (8 cicadas with pirate names, see below)
   Copy them to the SD card of the Flipper (subghz/), then Sub-GHz > Saved > the file > Send; or --send: copied and
   sent by the Flipper plugged in USB (its serial console, like flipper_weather.py).
   "pirates N": a crowd of N cicadas nearby (N <= 16), each with a pirate name, a score, skills and a level, sending
   their beacons (NET_BEACON, social.c) a few times: the badges list them (Social > Radar, the serial port "!"), to
   test the reception and the decoding of the beacons. Close to the Flipper, the badges count them as meetings (+10
   points each, kept in their memory): --power -20 makes them far (the beacons received below -80 dBm).
2. Prints the custom preset to add to the Flipper (SD card: subghz/assets/setting_user), so that the Flipper can
   also record the packets of a badge (Read RAW with the preset "SecSea") and replay them:
       python tools/flipper_net_sub.py preset

The bits on the air, as sent by the CC1101 of the badge: 4 bytes of preamble 0xAA, the sync word 0xC1 0x6A, the length,
the packet [0xC1][type][id 4 bytes][data], then the CRC-16 of the CC1101 (polynomial 0x8005, initial value 0xFFFF,
on the length and the packet), most significant bit first. The Flipper sends the bits as a GFSK signal
(asynchronous serial mode): high = f + deviation, low = f - deviation.
"""

import argparse
import random
import sys

BAUD = 9992.6  # MDMCFG4 = 0xC8, MDMCFG3 = 0x93 with the 26 MHz crystal of the Flipper
FREQUENCY = 433920000
SYNC = (0xC1, 0x6A)
MAGIC = 0xC1
TYPES = {'beacon': 0x01, 'command': 0x02, 'message': 0x03, 'ping': 0x0F}

# The pirate cicadas: names of 8 bytes at most (the name in a beacon, social.c)
PIRATES = ['Rackham', 'Barbossa', 'AnneBony', 'MaryRead', 'Surcouf', 'La Buse', 'Flint', 'Silver', 'Kidd',
           'Drake', 'Morgan', 'Teach', 'Crochet', 'Sparrow', 'Bart', 'Cigaron']
SKILLS = 20  # Bits of the skills (skills.c)
# PATABLE of the CC1101 at 433 MHz for some powers (dBm)
POWERS = {10: 0xC0, 7: 0xC8, 5: 0x84, 0: 0x60, -10: 0x34, -15: 0x1D, -20: 0x0E, -30: 0x12}

# Custom preset: register / value pairs, 00 00, then the 8 bytes of the PATABLE (+10 dBm first)
PRESET = [
    (0x02, 0x0D),  # IOCFG0: GDO0 serial data (asynchronous)
    (0x03, 0x47),  # FIFOTHR
    (0x08, 0x32),  # PKTCTRL0: asynchronous serial mode, infinite length
    (0x0B, 0x06),  # FSCTRL1: IF
    (0x10, 0xC8),  # MDMCFG4: channel bandwidth 101 kHz, data rate exponent
    (0x11, 0x93),  # MDMCFG3: data rate 9.99 kbps
    (0x12, 0x10),  # MDMCFG2: GFSK, no sync word (the bits of the file contain it)
    (0x15, 0x34),  # DEVIATN: 19 kHz
    (0x18, 0x18),  # MCSM0: calibration
    (0x19, 0x16),  # FOCCFG
    (0x1B, 0x43), (0x1C, 0x40), (0x1D, 0x91),  # AGCCTRL2/1/0
    (0x20, 0xFB),  # WORCTRL
]
PATABLE = [0xC0, 0, 0, 0, 0, 0, 0, 0]


def preset_data():
    values = [b for pair in PRESET for b in pair] + [0, 0] + PATABLE  # PATABLE[0]: the power (--power)
    return ' '.join(f'{v:02X}' for v in values)


def crc16_cc1101(data):
    crc = 0xFFFF
    for byte in data:
        for i in range(8):
            bit = (byte >> (7 - i)) & 1
            msb = (crc >> 15) & 1
            crc = (crc << 1) & 0xFFFF
            if msb ^ bit:
                crc ^= 0x8005
    return crc


def packet_bits(packet):
    frame = [len(packet)] + list(packet)
    crc = crc16_cc1101(frame)
    raw = [0xAA] * 4 + list(SYNC) + frame + [crc >> 8, crc & 0xFF]
    return [(b >> (7 - i)) & 1 for b in raw for i in range(8)]


def raw_durations(bits, repeats, gap_us):
    """Runs of the same bit -> positive (high) / negative (low) durations in µs."""
    out = []
    bit_us = 1e6 / BAUD
    for r in range(repeats):
        runs = []
        for b in bits:
            if runs and runs[-1][0] == b:
                runs[-1][1] += 1
            else:
                runs.append([b, 1])
        # Rounded on the time since the start of the packet, not run by run: no drift on the long packets
        t = 0
        for b, n in runs:
            d = round((t + n) * bit_us) - round(t * bit_us)
            t += n
            out.append(d if b else -d)
        if out[-1] < 0:
            out[-1] -= gap_us  # The gap between the repeats
        else:
            out.append(-gap_us)
    return out


def write_sub(path, durations, comment):
    """durations: a list of durations, or a list of lists (one per packet): each packet then starts a RAW_Data line"""
    lines = ['Filetype: Flipper SubGhz RAW File', 'Version: 1', f'# {comment}', f'Frequency: {FREQUENCY}',
             'Preset: FuriHalSubGhzPresetCustom', 'Custom_preset_module: CC1101',
             f'Custom_preset_data: {preset_data()}', 'Protocol: RAW']
    chunks = durations if durations and isinstance(durations[0], list) else [durations]
    for chunk in chunks:
        for i in range(0, len(chunk), 512):
            lines.append('RAW_Data: ' + ' '.join(str(d) for d in chunk[i:i + 512]))
    with open(path, 'w', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')


def main():
    parser = argparse.ArgumentParser(description='Flipper Zero .sub files for the network of the cicadas')
    parser.add_argument('kind', choices=['command', 'ping', 'raw', 'pirates', 'preset'])
    parser.add_argument('args', nargs='*', help='command: the command (0x02...); raw: the type then data bytes (hex); '
                        'pirates: how many cicadas (default 6)')
    parser.add_argument('--id', type=lambda v: int(v, 0), default=0x5EC5EA26, help='id of the sender (4 bytes)')
    parser.add_argument('--repeats', type=int, default=8, help='packets in the file (default 8, 250 ms apart: '
                        'the badges listen to the OOK remotes a moment every second)')
    parser.add_argument('--gap', type=int, default=250, help='milliseconds between the packets')
    parser.add_argument('-o', '--output', default=None)
    parser.add_argument('--power', type=int, default=10, choices=sorted(POWERS),
                        help='power of the Flipper in dBm (default +10, like the badges)')
    parser.add_argument('--ttl', type=int, default=2, choices=range(0, 5),
                        help='command: hops of the relay by the cicadas (default 2, 0: no relay)')
    parser.add_argument('--send', action='store_true', help='copy the file to the Flipper plugged in USB and send it')
    parser.add_argument('--port', default=None, help='with --send: serial port of the Flipper (found by itself)')
    args = parser.parse_args()
    PATABLE[0] = POWERS[args.power]

    if args.kind == 'preset':
        print('Add to the SD card of the Flipper, file subghz/assets/setting_user:\n')
        print('Custom_preset_name: SecSea')
        print('Custom_preset_module: CC1101')
        print(f'Custom_preset_data: {preset_data()}')
        return
    header = [MAGIC]
    sender = [(args.id >> (8 * i)) & 0xFF for i in range(4)]
    if args.kind == 'pirates':
        durations, comment = pirates(int(args.args[0]) if args.args else 6, args.repeats)
        out = args.output or 'secsea_pirates.sub'
        write_sub(out, durations, comment)
        print(f'{out}: {comment}, {sum(len(d) for d in durations)} durations')
        if args.send:
            send(out, args.port)
        return
    if args.kind == 'command':
        if not args.args:
            sys.exit('the command, e.g. 0x02')
        nonce = random.randrange(65536)
        # [command][nonce 2][TTL][origin 4]: the cicadas relay it TTL times (src/menu/relay.h)
        packet = header + [TYPES['command']] + sender + [int(args.args[0], 0), nonce & 0xFF, nonce >> 8, args.ttl]             + sender
    elif args.kind == 'ping':
        packet = header + [TYPES['ping']] + sender + [1]
    else:
        if not args.args:
            sys.exit('the type, then the data bytes in hex')
        packet = header + [int(args.args[0], 0)] + sender + [int(b, 16) for b in args.args[1:]]
    durations = [raw_durations(packet_bits(packet), 1, args.gap * 1000) for _ in range(args.repeats)]
    out = args.output or f'secsea_{args.kind}.sub'
    write_sub(out, durations, f'SecSea {args.kind} {" ".join(args.args)}: packet {bytes(packet).hex()}')
    print(f'{out}: {len(packet)} bytes, {sum(len(d) for d in durations)} durations')
    if args.send:
        send(out, args.port)


def pirates(n, rounds):
    """The beacons of n pirate cicadas, \\p rounds times each (random gaps, like the beacons of the badges).
    Returns (durations per packet, comment)."""
    if not 1 <= n <= len(PIRATES):
        sys.exit(f'1 to {len(PIRATES)} pirates')
    rnd = random.Random()
    crew = []
    for k in range(n):
        crew.append({'id': 0x9A7E0000 | rnd.randrange(1, 0xFFFF) << 0, 'name': PIRATES[k],
                     'score': rnd.randrange(0, 900), 'skills': rnd.getrandbits(SKILLS) & rnd.getrandbits(SKILLS),
                     'level': rnd.randrange(1, 8), 'seq': rnd.randrange(256)})
    durations = []
    for r in range(rounds):
        for c in crew:
            name = c['name'].encode('ascii')[:8].ljust(8, b'\0')
            data = [c['seq'] & 0xFF, c['score'] & 0xFF, c['score'] >> 8] + list(name) + \
                [(c['skills'] >> (8 * i)) & 0xFF for i in range(4)] + [c['level']]
            c['seq'] += 1
            packet = [MAGIC, TYPES['beacon']] + [(c['id'] >> (8 * i)) & 0xFF for i in range(4)] + data
            durations.append(raw_durations(packet_bits(packet), 1, rnd.randint(80, 300) * 1000))
    for c in crew:
        print(f"  {c['id']:08X} {c['name']:<8} score {c['score']:3}, level {c['level']}, skills {c['skills']:05X}")
    return durations, f'SecSea pirates: {n} cicadas x {rounds} beacons'


def send(path, port):
    """Copies the file to the Flipper (subghz/) and sends it (its USB serial console)"""
    import os
    import flipper_weather as fw  # The console of the Flipper
    port = port or fw.find_flipper()
    if not port:
        sys.exit('no Flipper Zero found: plug it in USB (or --port)')
    remote = '/ext/subghz/' + os.path.basename(path)
    flipper = fw.Flipper(port)
    try:
        flipper.write_file(remote, open(path, 'rb').read())
        out = flipper.command(f'subghz tx_from_file {remote} 1 0', timeout=120)
        if 'rror' in out:
            sys.exit(f'Flipper: {out}')
        print(f'sent by the Flipper ({port}): {remote}')
    finally:
        flipper.close()


if __name__ == '__main__':
    main()
