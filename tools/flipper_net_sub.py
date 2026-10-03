#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Flipper Zero and the network of the cicadas (GFSK 9.99 kbps, sync word 0xC16A, see src/menu/net.h):

1. Writes .sub files (RAW, custom GFSK preset) that make a Flipper send packets of the badges:
       python tools/flipper_net_sub.py command 0x02 -o mute.sub        (remote command: 0x02 mute, 0x03 unmute...)
       python tools/flipper_net_sub.py ping -o ping.sub
       python tools/flipper_net_sub.py raw 0x0F 01 -o any.sub           (type, then the data bytes in hex)
   Copy them to the SD card of the Flipper (subghz/), then Sub-GHz > Saved > the file > Send.
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
    values = [b for pair in PRESET for b in pair] + [0, 0] + PATABLE
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
    parser.add_argument('kind', choices=['command', 'ping', 'raw', 'preset'])
    parser.add_argument('args', nargs='*', help='command: the command (0x02...); raw: the type then data bytes (hex)')
    parser.add_argument('--id', type=lambda v: int(v, 0), default=0x5EC5EA26, help='id of the sender (4 bytes)')
    parser.add_argument('--repeats', type=int, default=8, help='packets in the file (default 8, 250 ms apart: '
                        'the badges listen to the OOK remotes a moment every second)')
    parser.add_argument('--gap', type=int, default=250, help='milliseconds between the packets')
    parser.add_argument('-o', '--output', default=None)
    args = parser.parse_args()

    if args.kind == 'preset':
        print('Add to the SD card of the Flipper, file subghz/assets/setting_user:\n')
        print('Custom_preset_name: SecSea')
        print('Custom_preset_module: CC1101')
        print(f'Custom_preset_data: {preset_data()}')
        return
    header = [MAGIC]
    sender = [(args.id >> (8 * i)) & 0xFF for i in range(4)]
    if args.kind == 'command':
        if not args.args:
            sys.exit('the command, e.g. 0x02')
        nonce = random.randrange(65536)
        packet = header + [TYPES['command']] + sender + [int(args.args[0], 0), nonce & 0xFF, nonce >> 8]
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


if __name__ == '__main__':
    main()
