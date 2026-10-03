#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
The weather forecast of a town on every badge, sent by a Flipper Zero plugged in USB.

1. Gets the forecast of the town from Open-Meteo (free, no key: geocoding then forecast).
2. Makes an announcement of the badges (src/menu/announce.c): the time, a short text ("Météo La Ciotat : 24C,
   ensoleillé. Demain 18-27C, pluie 10 %"), and a QR code with the next days. Every badge in range shows it like an
   announcement of the organizers (Social > Annonces keeps it).
3. Writes it as Flipper .sub files (the packets of the network of the cicadas, see flipper_net_sub.py; one file
   per part of the announcement), copies them to the SD card of the Flipper through its USB serial console, and sends
   them in turn, several rounds (subghz tx_from_file): about 30 s.

    python tools/flipper_weather.py "La Ciotat"                 (French, Flipper found by itself)
    python tools/flipper_weather.py Marseille --lang en --port COM10
    python tools/flipper_weather.py "La Ciotat" --no-send -o meteo.sub   (only the file: Sub-GHz > Saved > Send)
    python tools/flipper_weather.py "La Ciotat" --dry-run       (only shows the forecast and the text)

Needs the Python package pyserial (pip install pyserial) to send with the Flipper, and Internet for the forecast.
The Flipper must be on its main screen (no Sub-GHz application open) and not used by qFlipper.
"""

import argparse
import datetime
import json
import os
import random
import sys
import time
import unicodedata
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import flipper_net_sub as fns  # noqa: E402
import i18n  # noqa: E402  (the characters of the fonts of the badge)

NET_ANNOUNCE = 0x0D
PART = 46  # Bytes of an announcement per packet, with the relay (announce.c)
PART_OLD = 48  # The older format, without relay (--ttl 0: for the badges of an older firmware)
RELAYED = 0x80  # In the byte of the parts: the format with the TTL and the origin
MAX_PARTS = 5
TEXT_MAX = 111  # store_announce_t.text: 112 bytes with the final 0
QR_MAX = 63
QR_TEXT = 2  # ANNOUNCE_QR_TEXT
QR_NONE = 0
PART_REPEATS = 3  # Each part file: the packet 3 times...
PART_REPEAT_GAP_MS = 250  # ...250 ms apart
FLIPPER_PATH = '/ext/subghz/secsea_meteo.sub'

GEOCODING = 'https://geocoding-api.open-meteo.com/v1/search'
FORECAST = 'https://api.open-meteo.com/v1/forecast'

# WMO weather codes (Open-Meteo) -> (French, English), short
WEATHER = [
    ((0,), 'ensoleillé', 'sunny'),
    ((1, 2), 'éclaircies', 'sunny spells'),
    ((3,), 'couvert', 'cloudy'),
    ((45, 48), 'brouillard', 'fog'),
    ((51, 53, 55, 56, 57), 'bruine', 'drizzle'),
    ((61, 63, 65, 66, 67), 'pluie', 'rain'),
    ((71, 73, 75, 77), 'neige', 'snow'),
    ((80, 81, 82), 'averses', 'showers'),
    ((85, 86), 'averses de neige', 'snow showers'),
    ((95, 96, 99), 'orage', 'storm'),
]
DAYS = {'fr': ['lun', 'mar', 'mer', 'jeu', 'ven', 'sam', 'dim'], 'en': ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun']}


def weather_word(code, lang):
    for codes, fr, en in WEATHER:
        if code in codes:
            return fr if lang == 'fr' else en
    return '?'


def get_json(url, params):
    full = url + '?' + urllib.parse.urlencode(params)
    with urllib.request.urlopen(full, timeout=15) as r:
        return json.loads(r.read().decode('utf-8'))


def forecast(town, lang, days):
    geo = get_json(GEOCODING, {'name': town, 'count': 1, 'language': lang, 'format': 'json'})
    if not geo.get('results'):
        sys.exit(f'town not found: {town}')
    place = geo['results'][0]
    data = get_json(FORECAST, {
        'latitude': place['latitude'], 'longitude': place['longitude'], 'timezone': 'auto',
        'current': 'temperature_2m,weather_code,wind_speed_10m',
        'daily': 'weather_code,temperature_2m_min,temperature_2m_max,precipitation_probability_max,wind_speed_10m_max',
        'forecast_days': max(2, days),
    })
    return place, data


def badge_text(text):
    """Only the characters of the fonts of the badge: the others without their accent, or '?'."""
    ok = i18n.font_chars()
    out = []
    for ch in text.replace('°', '').replace('’', "'"):
        if ch in ok:
            out.append(ch)
            continue
        base = unicodedata.normalize('NFKD', ch).encode('ascii', 'ignore').decode('ascii')
        out.append(base if base else '?')
    return ''.join(out)


def cut_utf8(text, size):
    b = text.encode('utf-8')
    if len(b) <= size:
        return text
    return b[:size - 3].decode('utf-8', errors='ignore').rstrip() + '...'


def messages(place, data, lang, days):
    """(time, text, qr) of the announcement."""
    cur, daily = data['current'], data['daily']
    name = place['name']
    now_t = round(cur['temperature_2m'])
    now_w = weather_word(cur['weather_code'], lang)
    wind = round(cur['wind_speed_10m'])
    i = 1  # Tomorrow
    lo, hi = round(daily['temperature_2m_min'][i]), round(daily['temperature_2m_max'][i])
    rain = daily['precipitation_probability_max'][i]
    tw = weather_word(daily['weather_code'][i], lang)
    if lang == 'fr':
        text = (f'Météo {name} : {now_t}C, {now_w}, vent {wind} km/h. '
                f'Demain {lo}-{hi}C, {tw}' + (f', pluie {rain} %' if rain is not None else '') + '.')
    else:
        text = (f'Weather {name}: {now_t}C, {now_w}, wind {wind} km/h. '
                f'Tomorrow {lo}-{hi}C, {tw}' + (f', rain {rain}%' if rain is not None else '') + '.')
    text = cut_utf8(badge_text(text), TEXT_MAX)
    # The QR code: the next days, one per line ("sam 18-27 pluie")
    rows = []
    for d in range(1, min(days, len(daily['time']))):
        day = datetime.date.fromisoformat(daily['time'][d])
        rows.append(f"{DAYS[lang][day.weekday()]} {round(daily['temperature_2m_min'][d])}-"
                    f"{round(daily['temperature_2m_max'][d])}C {weather_word(daily['weather_code'][d], lang)}")
    qr = ''
    for r in rows:  # As many days as fit (ASCII: any phone reads it)
        r = unicodedata.normalize('NFKD', r).encode('ascii', 'ignore').decode('ascii')
        if len(qr) + len(r) + (1 if qr else 0) > QR_MAX:
            break
        qr += ('\n' if qr else '') + r
    hhmm = datetime.datetime.now().strftime('%H:%M')
    return hhmm, text, qr


def announcement_packets(hhmm, text, qr, sender, ttl=2):
    """The NET_ANNOUNCE packets: "time\\0text\\0<type>qr\\0" in parts, one nonce. With a TTL: parts of 46 bytes,
    [nonce 2][part][parts | 0x80][TTL][origin 4], relayed TTL times by the cicadas; ttl 0: the older format (parts of
    48 bytes, no relay), for the badges of an older firmware."""
    body = hhmm.encode() + b'\0' + text.encode('utf-8') + b'\0' + bytes([QR_TEXT if qr else QR_NONE]) + \
        qr.encode() + b'\0'
    size = PART if ttl else PART_OLD
    parts = [body[i:i + size] for i in range(0, len(body), size)]
    if len(parts) > MAX_PARTS:
        sys.exit('announcement too long')
    nonce = random.randrange(65536)
    ids = [(sender >> (8 * i)) & 0xFF for i in range(4)]
    if not ttl:
        return [[fns.MAGIC, NET_ANNOUNCE] + ids + [nonce & 0xFF, nonce >> 8, k, len(parts)] + list(p)
                for k, p in enumerate(parts)]
    return [[fns.MAGIC, NET_ANNOUNCE] + ids + [nonce & 0xFF, nonce >> 8, k, len(parts) | RELAYED, ttl] + ids + list(p)
            for k, p in enumerate(parts)]


# ---- The Flipper Zero, through its USB serial console (CLI) ----

def find_flipper():
    from serial.tools import list_ports
    for p in list_ports.comports():
        if (p.serial_number or '').upper().startswith('FLIP') or 'flipper' in (p.description or '').lower() or \
                (p.vid, p.pid) == (0x0483, 0x5740):
            return p.device
    return None


class Flipper:
    PROMPT = b'>: '

    def __init__(self, port):
        import serial
        self.s = serial.Serial(port, 230400, timeout=0.2)
        self.s.write(b'\r')
        self.read_until(self.PROMPT, 5)

    def read_until(self, token, timeout):
        end = time.time() + timeout
        buf = b''
        while time.time() < end:
            buf += self.s.read(4096)
            if token in buf:
                return buf
        raise RuntimeError(f'Flipper: no answer ({buf[-200:]!r})')

    def command(self, line, timeout=10):
        self.s.write(line.encode() + b'\r')
        out = self.read_until(self.PROMPT, timeout).decode('utf-8', errors='replace')
        return out.replace(line, '', 1).replace('>: ', '').strip()

    def write_file(self, path, data):
        self.command(f'storage remove {path}')  # write_chunk appends
        self.s.write(f'storage write_chunk {path} {len(data)}\r'.encode())
        self.read_until(b'Ready', 5)
        self.s.write(data)
        out = self.read_until(self.PROMPT, 20).decode('utf-8', errors='replace')
        if 'rror' in out:
            raise RuntimeError(f'Flipper: {out.strip()}')

    def close(self):
        self.s.close()


def main():
    if sys.stdout.encoding and sys.stdout.encoding.lower() != 'utf-8':
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    parser = argparse.ArgumentParser(description='Weather forecast of a town shown on the badges, sent by a Flipper')
    parser.add_argument('town', help='the town ("La Ciotat", "Marseille, FR"...)')
    parser.add_argument('--lang', choices=['fr', 'en'], default='fr', help='language of the text (fr)')
    parser.add_argument('--days', type=int, default=4, help='days in the QR code, from tomorrow (4)')
    parser.add_argument('--rounds', type=int, default=4, help='times each part is sent (4; 3 packets each time)')
    parser.add_argument('--id', type=lambda v: int(v, 0), default=0x5EC5EA27, help='sender id (4 bytes)')
    parser.add_argument('--ttl', type=int, default=2, choices=range(0, 5),
                        help='hops of the relay by the cicadas (default 2; 0: older format, no relay)')
    parser.add_argument('--port', default=None, help='serial port of the Flipper (found by itself)')
    parser.add_argument('-o', '--output', default=None, help='also writes the .sub file here')
    parser.add_argument('--no-send', action='store_true', help='only write the .sub file (-o)')
    parser.add_argument('--dry-run', action='store_true', help='only show the forecast and the text')
    args = parser.parse_args()

    place, data = forecast(args.town, args.lang, args.days + 1)
    hhmm, text, qr = messages(place, data, args.lang, args.days + 1)
    print(f'{place["name"]} ({place.get("admin1", "")}, {place.get("country", "")}) '
          f'{place["latitude"]:.2f}, {place["longitude"]:.2f}')
    print(f'announcement at {hhmm}: "{text}" ({len(text.encode("utf-8"))} bytes)')
    print('QR code: ' + (qr.replace('\n', ' | ') if qr else '(none)'))
    if args.dry_run:
        return
    packets = announcement_packets(hhmm, text, qr, args.id, args.ttl)
    # One small file per part (its packet PART_REPEATS times): the Flipper plays the long RAW files badly (the long
    # packets were lost), the short ones well; the badges put the parts together, whatever the order
    base = args.output or os.path.join(HERE, '..', 'build', 'flipper', 'secsea_meteo.sub')
    os.makedirs(os.path.dirname(os.path.abspath(base)), exist_ok=True)
    files = []
    for k, p in enumerate(packets):
        path = base if len(packets) == 1 else f'{os.path.splitext(base)[0]}_{k + 1}.sub'
        d = fns.raw_durations(fns.packet_bits(p), PART_REPEATS, PART_REPEAT_GAP_MS * 1000)
        fns.write_sub(path, d, f'SecSea weather {place["name"]}: part {k + 1}/{len(packets)}')
        files.append(path)
    print(f'{len(files)} file(s): ' + ', '.join(os.path.basename(f) for f in files) +
          f' (sent {args.rounds} times each)')
    if args.no_send:
        print('Copy them to the Flipper (subghz/), then Sub-GHz > Saved > each file > Send, several times')
        return

    port = args.port or find_flipper()
    if not port:
        sys.exit('no Flipper Zero found: plug it in USB (or --port)')
    flipper = Flipper(port)
    try:
        remote = []
        for k, f in enumerate(files):
            path = FLIPPER_PATH.replace('.sub', f'_{k + 1}.sub')
            flipper.write_file(path, open(f, 'rb').read())
            remote.append(path)
        print(f'copied to the Flipper ({port}): {", ".join(remote)}')
        for r in range(args.rounds):
            for path in remote:
                out = flipper.command(f'subghz tx_from_file {path} 1 0', timeout=30)
                if 'rror' in out or 'not found' in out.lower():
                    sys.exit(f'Flipper: {out}')
            print(f'round {r + 1}/{args.rounds} sent')
    finally:
        flipper.close()

if __name__ == '__main__':
    main()
