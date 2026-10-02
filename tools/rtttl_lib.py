"""Reading of ringtone files, like the badge does (src/menu/rtttl_parse.c): RTTTL lines (.txt / .rtttl / .rtx) and
PICAXE "tune" commands (.bas, converted to RTTTL). Used by rtttl_sort.py."""

import re

LETTERS = {'c': 0, 'd': 2, 'e': 4, 'f': 5, 'g': 7, 'a': 9, 'b': 11, 'h': 11}
DURATIONS = (1, 2, 4, 8, 16, 32, 64)
NAME_MAX = 39  # Bytes of the name kept by the badge (RTTTL_NAME_MAX - 1)


class RtttlError(Exception):
    pass


def parse_rtttl(line):
    """Returns (name, bpm, notes) of an RTTTL line, notes = [(semitone from C0 or None for a pause, duration, dots)].
    Raises RtttlError where the badge would show an error."""
    parts = line.split(':', 2)
    if len(parts) < 2:
        raise RtttlError('no ":" after the name')
    if len(parts) < 3:
        raise RtttlError('no ":" between the defaults and the notes')
    name, defaults, notes = parts[0].strip(), parts[1], parts[2]
    if not defaults.strip() and ':' in notes and '=' in notes.split(':', 1)[0]:
        defaults, notes = notes.split(':', 1)  # "Name: :d=4,o=5:c"
    d, o, b = 4, 6, 63
    for item in defaults.split(','):
        item = item.strip().replace(' ', '')
        if not item:
            continue
        m = re.fullmatch(r'([a-zA-Z])=(\d+)', item)
        if not m:
            raise RtttlError(f'default "{item}"')
        k, v = m.group(1).lower(), int(m.group(2))
        if k == 'd':
            if v not in DURATIONS:
                raise RtttlError(f'default "{item}"')
            d = v
        elif k == 'o':
            if not 3 <= v <= 8:
                raise RtttlError(f'default "{item}"')
            o = v
        elif k == 'b':
            if not 1 <= v <= 999:
                raise RtttlError(f'default "{item}"')
            b = v
    out = []
    for raw in notes.split(','):
        n = raw.replace(' ', '').replace('\t', '').lower()
        if not n:
            continue
        # [duration] [dots and sharp, some converters] letter [# or _] [dots] [octave] [dots]
        m = re.fullmatch(r'(\d+)?([.#]*)([a-hp])([#_])?(\.*)(\d+)?(\.*)', n)
        if not m or m.group(2).count('#') > 1 or (m.group(2).count('#') and m.group(4)):
            raise RtttlError(f'note "{raw.strip()}"')
        dur = int(m.group(1)) if m.group(1) else d
        if dur not in DURATIONS:
            raise RtttlError(f'duration "{raw.strip()}"')
        dots = m.group(2).count('.') + len(m.group(5)) + len(m.group(7))
        sharp = '#' in m.group(2) or bool(m.group(4))
        if dots > 2:
            raise RtttlError(f'note "{raw.strip()}"')
        if m.group(3) == 'p':
            if sharp:
                raise RtttlError(f'note "{raw.strip()}"')
            out.append((None, dur, dots))
            continue
        octave = int(m.group(6)) if m.group(6) else o
        if not 3 <= octave <= 8:
            raise RtttlError(f'octave "{raw.strip()}"')
        out.append((octave * 12 + LETTERS[m.group(3)] + (1 if sharp else 0), dur, dots))
    if not any(s is not None for s, _, _ in out):
        raise RtttlError('no note')
    return name, b, out


# ---- PICAXE "tune pin, speed, ($xx, ...)" (PICAXE manual 2, "tune") ----

PICAXE_DURATIONS = (4, 8, 1, 2)  # Bits 7-6
PICAXE_OCTAVES = (5, 6, 4)  # Bits 5-4: middle, high, low (the middle C is 523 Hz: "c5" in RTTTL, A4 = 440 Hz)
PICAXE_NOTES = ('c', 'c#', 'd', 'd#', 'e', 'f', 'f#', 'g', 'g#', 'a', 'a#', 'b')
PICAXE_TUNE = re.compile(r'^\s*tune\s+[^,]+,\s*(\w+)\s*,\s*(?:[^,(]+,\s*)?\((.*)\)', re.IGNORECASE)


def picaxe_number(text):
    text = text.strip()
    if text.startswith('$'):
        return int(text[1:], 16)
    if text.startswith('%'):
        return int(text[1:], 2)
    return int(text)


def picaxe_to_rtttl(line, name):
    """The RTTTL line of a PICAXE tune command (None if the line is not one). Same conversion as the badge."""
    m = PICAXE_TUNE.match(line)
    if not m:
        return None
    try:
        speed = picaxe_number(m.group(1))
        values = [picaxe_number(v) for v in m.group(2).split(',') if v.strip()]
    except ValueError:
        raise RtttlError('PICAXE tune: not a number')
    if not 1 <= speed <= 15 or not values or any(not 0 <= v <= 255 for v in values):
        raise RtttlError('PICAXE tune: value out of range')
    # A quarter: speed x 73.84 ms (sound and silence)
    bpm = round(60000 / (speed * 73.84))
    notes = []
    for v in values:
        dur = PICAXE_DURATIONS[v >> 6]
        oct_bits = (v >> 4) & 3
        note = v & 15
        if note >= 12:
            notes.append(f'{dur}p')
        else:
            if oct_bits == 3:
                raise RtttlError('PICAXE tune: octave 3')
            notes.append(f'{dur}{PICAXE_NOTES[note]}{PICAXE_OCTAVES[oct_bits]}')
    return f'{name}:d=4,o=5,b={bpm}:' + ','.join(notes)


def read_tunes(path):
    """The tunes of a file: [(line number, RTTTL text or None, error or None)]."""
    data = open(path, 'rb').read()
    if data.startswith(b'\xef\xbb\xbf'):
        data = data[3:]
    try:
        text = data.decode('utf-8')
    except UnicodeDecodeError:
        text = data.decode('cp1252', errors='replace')
    lines = re.split(r'\r\n|\n|\r', text)
    tunes = []
    if path.lower().endswith('.bas'):
        name = None
        for no, l in enumerate(lines, 1):
            s = l.strip()
            if s.startswith("'") or s.startswith(';'):
                if name is None and s[1:].strip():
                    name = s[1:].strip()
                continue
            try:
                r = picaxe_to_rtttl(s, name or re.sub(r'\.bas$', '', path.replace('\\', '/').split('/')[-1], flags=re.I))
            except RtttlError as e:
                tunes.append((no, None, str(e)))
                continue
            if r:
                tunes.append((no, r, None))
                name = None
        return tunes
    for no, l in enumerate(lines, 1):
        s = l.strip()
        if not s or s.startswith('#'):
            continue
        tunes.append((no, s, None))
    return tunes
