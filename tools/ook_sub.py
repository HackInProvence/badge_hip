#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Writes Flipper Zero Sub-GHz RAW files (.sub) that send test frames of the 433.92MHz OOK protocols decoded by the
badge (src/radio/ookdec.c), to test its receiver with a Flipper: copy the files to the SD card of the Flipper
(subghz folder), then Sub-GHz > Saved > <file> > Send.

    python tools/ook_sub.py princeton 0x123454 [--te 400]         a remote (PT2262 / EV1527), 24 bits
    python tools/ook_sub.py came 0x5A1 [--bits 24]                  CAME, 12 or 24 bits
    python tools/ook_sub.py nice 0x3F1 [--bits 24]                  Nice FLO, 12 or 24 bits
    python tools/ook_sub.py weather --temp 21.5 --hum 45 --channel 2 [--id 0x5A] [--battery-low] [--protocol nexus]
                                                                    one file per thermometer protocol (or one)
    python tools/ook_sub.py check file.sub...                       checks the format of files
    python tools/ook_sub.py selftest                                generates everything in a temporary folder and checks it

Options of all the generators: -o / --output (file, or folder for "weather"), --repeats (frames per file).

The Flipper can also send Princeton itself, without file: Sub-GHz > Add Manually > Princeton_433 (random 20 bits key,
button 0x4, te = 400µs, 24 bits), then edit the "Key:" line of the saved file to choose the code.

Format (flipperzero-firmware documentation/file_formats/SubGhzFileFormats.md, "RAW Files"): header lines, then
"RAW_Data:" lines of up to 512 non-zero durations in µs, alternating positive (carrier on) and negative (carrier off),
the first one positive.

The timings and bit layouts are those of src/radio/ookdec.c (references: rtl_433 src/devices, Flipper Zero
lib/subghz/protocols and the Weather Station app), and the same as the encoders of src/tests/host/test_ookdec.c:
the C tests check the decoding of these frames, this script is correct by construction and checks the file shape.
"""

import argparse
import os
import sys
import tempfile

FREQUENCY = 433920000
PRESET = 'FuriHalSubGhzPresetOok650Async'
MAX_PER_LINE = 512
FINAL_SILENCE = 30000  # µs of silence at the end of the file


# ---- Durations: list of (level, µs), merged and written as +µs / -µs ----

class Signal:
    def __init__(self):
        self.d = []  # Signed durations: > 0 carrier on, < 0 off

    def pulse(self, us):
        self._add(int(us))

    def gap(self, us):
        self._add(-int(us))

    def pair(self, pulse, gap):
        self.pulse(pulse)
        self.gap(gap)

    def _add(self, v):
        if v == 0:
            return
        if self.d and (self.d[-1] > 0) == (v > 0):
            self.d[-1] += v  # Same level: merged
        else:
            self.d.append(v)

    def finish(self):
        if self.d and self.d[-1] < 0:
            self.d[-1] = -max(-self.d[-1], FINAL_SILENCE)
        else:
            self.gap(FINAL_SILENCE)
        return self.d


def bits_of(value, nbits):
    """MSB first."""
    return [(value >> (nbits - 1 - i)) & 1 for i in range(nbits)]


def bytes_bits(data):
    return [b for byte in data for b in bits_of(byte, 8)]


def div_round(a, b):
    """Rounded division, half away from zero (like div_round() in ookdec.c)."""
    return (a + b // 2) // b if a >= 0 else -((-a + b // 2) // b)


# ---- Remotes ----

def princeton(code, te=400, repeats=10):
    """Princeton / PT2262 / EV1527 (Flipper princeton.c): 1 = (3te, te), 0 = (te, 3te), stop pulse te, guard 30te."""
    s = Signal()
    for _ in range(repeats):
        for bit in bits_of(code, 24):
            s.pair(3 * te, te) if bit else s.pair(te, 3 * te)
        s.pair(te, 30 * te)
    return s.finish()


def came_like(code, nbits, te, guard_te, repeats):
    """CAME / Nice FLO (Flipper came.c, nice_flo.c): guard gap, start pulse te, per bit 1 = (2te gap, te pulse),
    0 = (te gap, 2te pulse)."""
    s = Signal()
    for _ in range(repeats):
        s.pulse(te)
        for bit in bits_of(code, nbits):
            if bit:
                s.gap(2 * te)
                s.pulse(te)
            else:
                s.gap(te)
                s.pulse(2 * te)
        s.gap(guard_te * te)
    return s.finish()


def came(code, nbits=12, repeats=8):
    return came_like(code, nbits, 320, 47 if nbits == 12 else 76, repeats)


def nice_flo(code, nbits=12, repeats=8):
    return came_like(code, nbits, 700, 36, repeats)


# ---- Thermometers ----

def ppm(s, bits, zero, one, end, pulse=500):
    """Pulse distance: (pulse, gap) per bit, then (pulse, end)."""
    for bit in bits:
        s.pair(pulse, one if bit else zero)
    s.pair(pulse, end)


def nexus(temp_c10, hum, channel, id_, battery_low, repeats=12):
    """Nexus-TH (rtl_433 nexus.c): 36 bits [id:8] [battery ok:1] [test:1] [channel-1:2] [temp:12] [1111] [hum:8],
    pulses 500µs, gaps 0 = 1000µs, 1 = 2000µs, sync 4000µs; 12 frames."""
    v = (id_ & 0xFF) << 28 | ((not battery_low) << 3 | (channel - 1) & 3) << 24 | (temp_c10 & 0xFFF) << 12 \
        | 0xF << 8 | hum & 0xFF
    s = Signal()
    s.pair(500, 4000)
    for _ in range(repeats):
        ppm(s, bits_of(v, 36), 1000, 2000, 4000)
    return s.finish()


def ppm37(v, repeats):
    """ThermoPRO-TX4 / GT-WT02: sync (500µs, 9000µs), then 37 bits frames: gaps 0 = 2000µs, 1 = 4000µs, end 9000µs."""
    s = Signal()
    s.pair(500, 9000)
    for _ in range(repeats):
        ppm(s, bits_of(v, 37), 2000, 4000, 9000)
    return s.finish()


def thermopro_tx4(temp_c10, hum, channel, id_, battery_low, repeats=6):
    """ThermoPRO TX-4 (rtl_433 thermopro_tx2.c, Flipper thermopro_tx4.c): [type:4 = 9] [id:8] [battery low:1]
    [button:1] [channel-1:2] [temp:12] [hum:8, 0xCC = none] [0]; 6 frames."""
    v = 9 << 33 | (id_ & 0xFF) << 25 | int(battery_low) << 24 | ((channel - 1) & 3) << 21 | (temp_c10 & 0xFFF) << 9 \
        | (hum & 0xFF) << 1
    return ppm37(v, repeats)


def gt_wt02(temp_c10, hum, channel, id_, battery_low, repeats=6):
    """GT-WT02 (rtl_433 gt_wt_02.c): [id:8] [battery low:1] [button:1] [channel-1:2] [temp:12] [hum:7] [checksum:6],
    checksum = sum of the nibbles of the 31 first bits followed by a 0, modulo 64. Humidity < 20% is sent as 10, > 90%
    as 110."""
    hum = 10 if hum < 20 else 110 if hum > 90 else hum
    v = (id_ & 0xFF) << 23 | int(battery_low) << 22 | ((channel - 1) & 3) << 19 | (temp_c10 & 0xFFF) << 7 | hum
    chk = sum(((v << 1) >> (4 * i)) & 0xF for i in range(8)) & 0x3F
    return ppm37(v << 6 | chk, repeats)


def crc4(msg, poly, init):
    """CRC-4 over whole bytes, MSB first (rtl_433 bit_util.c crc4)."""
    rem = init << 4
    p = poly << 4
    for byte in msg:
        rem ^= byte
        for _ in range(8):
            rem = ((rem << 1) ^ p if rem & 0x80 else rem << 1) & 0xFF
    return rem >> 4 & 0x0F


def infactory(temp_c10, hum, channel, id_, battery_low, repeats=6):
    """inFactory (rtl_433 infactory.c): 4 x (1000µs, 1000µs), (500µs, 8000µs), 40 bits, (500µs, 16000µs).
    [id:8] [crc:4] [button:1] [battery low:1] [0:2] [temp °F*10+900:12] [hum BCD:8] [0:2] [channel:2]."""
    f10 = div_round(temp_c10 * 9, 5) + 320 + 900
    hum = min(hum, 100)
    b = [id_ & 0xFF, int(battery_low) << 2, f10 >> 4 & 0xFF, (f10 & 0xF) << 4 | hum // 10,
         (hum % 10) << 4 | channel & 3]
    m = list(b)
    m[1] = (m[1] & 0x0F) | (m[4] & 0x0F) << 4
    b[1] |= (crc4(m[:4], 0x13, 0) ^ (b[4] >> 4)) << 4
    s = Signal()
    for _ in range(repeats):
        for _ in range(4):
            s.pair(1000, 1000)
        s.pair(500, 8000)
        ppm(s, bytes_bits(b), 2000, 4000, 16000)
    return s.finish()


def lfsr_digest8_reflect(msg, gen, key):
    """rtl_433 bit_util.c lfsr_digest8_reflect."""
    total = 0
    for byte in reversed(msg):
        for i in range(8):
            if (byte >> i) & 1:
                total ^= key
            key = ((key << 1) ^ gen if key & 0x80 else key << 1) & 0xFF
    return total


def pwm(s, bits, p1, g1, p0, g0, last_gap=None):
    for i, bit in enumerate(bits):
        s.pulse(p1 if bit else p0)
        s.gap(last_gap if last_gap and i == len(bits) - 1 else g1 if bit else g0)


def lacrosse_tx141thbv2(temp_c10, hum, channel, id_, battery_low, repeats=12):
    """LaCrosse TX141TH-Bv2 (rtl_433 lacrosse_tx141x.c): 12 packets of 4 x (833µs, 833µs) and 40 bits,
    1 = (417µs, 208µs), 0 = (208µs, 417µs), then 2 x (833µs, 833µs). [id:8] [battery low:1] [test:1]
    [channel-1:2] [temp °C*10+500:12] [hum:8] [lfsr_digest8_reflect(0x31, 0xF4):8]."""
    t = temp_c10 + 500
    b = [id_ & 0xFF, int(battery_low) << 7 | ((channel - 1) & 3) << 4 | (t >> 8) & 0xF, t & 0xFF, hum & 0xFF]
    b.append(lfsr_digest8_reflect(b, 0x31, 0xF4))
    s = Signal()
    for _ in range(repeats):
        for _ in range(4):
            s.pair(833, 833)
        pwm(s, bytes_bits(b), 417, 208, 208, 417)
    s.pair(833, 833)
    s.pair(833, 833)
    return s.finish()


def even_parity(byte):
    return byte | (bin(byte).count('1') & 1) << 7


def acurite_592txr(temp_c10, hum, channel, id_, battery_low, repeats=3):
    """Acurite 592TXR (rtl_433 acurite.c): 3 packets of 4 x (620µs, 596µs) and 56 bits, 1 = (408µs, 204µs),
    0 = (220µs, 392µs), 2192µs between packets. Channel 1 = A (11), 2 = B (10), 3 = C (00); 14 bits ID;
    temperature °C*10+1000; bytes 2 to 5 with an even parity bit; last byte = sum of the 6 others."""
    t = temp_c10 + 1000
    raw_channel = {1: 3, 2: 2, 3: 0}[channel]
    b = [raw_channel << 6 | (id_ >> 8) & 0x3F, id_ & 0xFF, even_parity((not battery_low) << 6 | 0x04),
         even_parity(hum & 0x7F), even_parity(t >> 7 & 0x1F), even_parity(t & 0x7F)]
    b.append(sum(b) & 0xFF)
    s = Signal()
    for _ in range(repeats):
        for _ in range(4):
            s.pair(620, 596)
        pwm(s, bytes_bits(b), 408, 204, 220, 392, last_gap=2192)
    return s.finish()


# name: (function, channels, temperature range °C*10, has an ID of 14 bits)
WEATHER = {
    'nexus': (nexus, (1, 3), (-500, 800)),
    'thermopro_tx4': (thermopro_tx4, (1, 4), (-400, 800)),
    'gt_wt02': (gt_wt02, (1, 3), (-200, 600)),
    'infactory': (infactory, (1, 3), (-400, 700)),
    'lacrosse_tx141thbv2': (lacrosse_tx141thbv2, (1, 4), (-400, 600)),
    'acurite_592txr': (acurite_592txr, (1, 3), (-400, 700)),
}


# ---- Files ----

def sub_text(durations, comment=None):
    lines = ['Filetype: Flipper SubGhz RAW File', 'Version: 1']
    if comment:
        lines.append('# ' + comment)
    lines += ['Frequency: %d' % FREQUENCY, 'Preset: ' + PRESET, 'Protocol: RAW']
    for i in range(0, len(durations), MAX_PER_LINE):
        lines.append('RAW_Data: ' + ' '.join(str(d) for d in durations[i:i + MAX_PER_LINE]))
    return '\n'.join(lines) + '\n'


def check_file(path):
    """Parses a .sub file and checks its shape; returns the durations. Raises ValueError."""
    with open(path, encoding='utf-8') as f:
        lines = [l.rstrip('\r\n') for l in f]
    fields = {}
    durations = []
    for l in lines:
        if not l or l.startswith('#'):
            continue
        key, sep, value = l.partition(':')
        if not sep:
            raise ValueError('%s: line without ":": %r' % (path, l))
        value = value.strip()
        if key == 'RAW_Data':
            values = [int(v) for v in value.split()]
            if not 0 < len(values) <= MAX_PER_LINE:
                raise ValueError('%s: %d values in a RAW_Data line' % (path, len(values)))
            durations += values
        else:
            fields[key] = value
    expected = {'Filetype': 'Flipper SubGhz RAW File', 'Version': '1', 'Protocol': 'RAW'}
    for k, v in expected.items():
        if fields.get(k) != v:
            raise ValueError('%s: %s is %r, expected %r' % (path, k, fields.get(k), v))
    if 'Frequency' not in fields or 'Preset' not in fields:
        raise ValueError('%s: Frequency or Preset missing' % path)
    if lines[0] != 'Filetype: Flipper SubGhz RAW File' or not lines[1].startswith('Version:'):
        raise ValueError('%s: the file must start with Filetype and Version' % path)
    if not durations or durations[0] <= 0:
        raise ValueError('%s: the first duration must be positive' % path)
    for i, d in enumerate(durations):
        if d == 0:
            raise ValueError('%s: zero duration at %d' % (path, i))
        if i and (d > 0) == (durations[i - 1] > 0):
            raise ValueError('%s: durations %d and %d have the same sign' % (path, i - 1, i))
    return durations


def write(path, durations, comment):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(sub_text(durations, comment))
    d = check_file(path)
    assert d == durations
    print('%s: %d durations, %.0f ms' % (path, len(d), sum(abs(x) for x in d) / 1000))


def weather_files(args, folder):
    temp_c10 = int(round(args.temp * 10))
    names = WEATHER if args.protocol == 'all' else [args.protocol]
    os.makedirs(folder, exist_ok=True)
    for name in names:
        fn, (ch_min, ch_max), (t_min, t_max) = WEATHER[name]
        if not ch_min <= args.channel <= ch_max:
            print('%s: channel %d out of %d..%d, skipped' % (name, args.channel, ch_min, ch_max))
            continue
        if not t_min <= temp_c10 <= t_max:
            print('%s: warning, %.1f°C is out of the range of the sensor, the badge will reject it' % (name, args.temp))
        id_ = args.id if name == 'acurite_592txr' else args.id & 0xFF
        kw = {'repeats': args.repeats} if args.repeats else {}
        d = fn(temp_c10, args.hum, args.channel, id_, args.battery_low, **kw)
        comment = '%s ch%d %.1f C %d%% id 0x%X%s' % (name, args.channel, temp_c10 / 10, args.hum, id_,
                                                  ' battery low' if args.battery_low else '')
        write(os.path.join(folder, 'ook_%s.sub' % name), d, comment)


def selftest():
    with tempfile.TemporaryDirectory(prefix='ook_sub_') as folder:
        selftest_in(folder)
    print('selftest OK')


def selftest_in(folder):
    ns = argparse.Namespace(temp=-12.3, hum=45, channel=2, id=0x2ABC, battery_low=False, protocol='all', repeats=None)
    weather_files(ns, folder)
    write(os.path.join(folder, 'princeton.sub'), princeton(0x123454), 'Princeton 0x123454')
    write(os.path.join(folder, 'came.sub'), came(0x5A1), 'CAME 0x5A1')
    write(os.path.join(folder, 'nice.sub'), nice_flo(0x3F1, 24), 'Nice FLO 0x3F1')
    # Shapes: number of durations of one frame, merged with the neighbours
    assert len(princeton(0x123454, repeats=1)) == 50
    assert len(nexus(213, 45, 2, 0x5A, False, repeats=1)) == 2 + 74
    assert len(lacrosse_tx141thbv2(213, 45, 2, 0x5A, False, repeats=1)) == 88 + 4
    assert len(acurite_592txr(213, 45, 2, 0x5A, False, repeats=1)) == 120
    # Bit layouts: the GT-WT02 examples of rtl_433 gt_wt_02.c ({37} 34 00 ed 47 60 = ch1 23.7°C 35%)
    d = gt_wt02(237, 35, 1, 0x34, False, repeats=1)
    bits = [1 if -d[i] > 3000 else 0 for i in range(3, 3 + 2 * 37, 2)]
    assert int(''.join(map(str, bits)), 2) == 0x3400ed4760 >> 3, bits


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n\n')[1], formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest='cmd', required=True)
    for name in ('princeton', 'came', 'nice'):
        q = sub.add_parser(name)
        q.add_argument('code', type=lambda x: int(x, 0))
        q.add_argument('-o', '--output')
        q.add_argument('--repeats', type=int)
        if name == 'princeton':
            q.add_argument('--te', type=int, default=400, help='elementary duration in µs (default 400, as the Flipper)')
        else:
            q.add_argument('--bits', type=int, choices=(12, 24), default=12)
    q = sub.add_parser('weather')
    q.add_argument('--temp', type=float, required=True, help='°C')
    q.add_argument('--hum', type=int, default=50, help='%%')
    q.add_argument('--channel', type=int, default=1)
    q.add_argument('--id', type=lambda x: int(x, 0), default=0x5A, help='sensor ID (8 bits, 14 for Acurite)')
    q.add_argument('--battery-low', action='store_true')
    q.add_argument('--protocol', choices=['all'] + list(WEATHER), default='all')
    q.add_argument('--repeats', type=int)
    q.add_argument('-o', '--output', default='.', help='folder')
    q = sub.add_parser('check')
    q.add_argument('files', nargs='+')
    sub.add_parser('selftest')
    args = p.parse_args()

    try:
        if args.cmd == 'princeton':
            if not 0 <= args.code < 1 << 24:
                p.error('24 bits code')
            kw = {'repeats': args.repeats} if args.repeats else {}
            write(args.output or 'princeton_%06X.sub' % args.code, princeton(args.code, args.te, **kw),
                  'Princeton 0x%06X te %dus' % (args.code, args.te))
        elif args.cmd in ('came', 'nice'):
            if not 0 <= args.code < 1 << args.bits:
                p.error('%d bits code' % args.bits)
            fn = came if args.cmd == 'came' else nice_flo
            kw = {'repeats': args.repeats} if args.repeats else {}
            write(args.output or '%s_%X.sub' % (args.cmd, args.code), fn(args.code, args.bits, **kw),
                  '%s %d bits 0x%X' % (args.cmd, args.bits, args.code))
        elif args.cmd == 'weather':
            weather_files(args, args.output)
        elif args.cmd == 'check':
            for f in args.files:
                d = check_file(f)
                print('%s: OK, %d durations, %.0f ms' % (f, len(d), sum(abs(x) for x in d) / 1000))
        else:
            selftest()
    except ValueError as e:
        print('Error:', e)
        sys.exit(1)


if __name__ == '__main__':
    main()
