#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Checks the score QR codes shown by the badges at the end of the games (Jeux menu, flanks on the end page),
e.g. for a leaderboard: scan the QR codes with a phone, paste the texts in a file, then

    python tools/score_check.py scores.txt --key <32 hex digits>
    python tools/score_check.py "HIP26:SIMON:12:1A2B3C4D:Cig 33EC:0123456789ABCDEF" --key ...

Text of a QR code: HIP26:<game>:<score>:<badge id>:<cicada name>:<signature>
- game: MORPION, P4 (wins-losses-draws of the session, or "<n>V": all the wins, from the record),
  SIMON, REFLEX (average in ms, lower is better), SNAKE,
- badge id: 8 hex digits (hash of the unique id of the badge, as in the network of the cicadas),
- signature: SipHash-2-4 of everything before the last ':', with the 128 bits key of the firmware, 16 hex digits.

The key is not in this script: give it with --key or the BADGE_SCORE_KEY environment variable.
In the firmware it is only stored masked (src/menu/score_code.c); --make-key generates a new key and its masked table.
"""

import argparse
import os
import secrets
import sys

PREFIX = 'HIP26'
MASK_SEED = 0x7A3C19E5  # Same as score_code.c


def siphash24(key, data):
    """SipHash-2-4 (Aumasson, Bernstein), 64 bits result."""
    def rotl(x, b):
        return ((x << b) | (x >> (64 - b))) & 0xFFFFFFFFFFFFFFFF

    k0 = int.from_bytes(key[:8], 'little')
    k1 = int.from_bytes(key[8:], 'little')
    v = [0x736f6d6570736575 ^ k0, 0x646f72616e646f6d ^ k1, 0x6c7967656e657261 ^ k0, 0x7465646279746573 ^ k1]
    mask = 0xFFFFFFFFFFFFFFFF

    def rnd():
        v[0] = (v[0] + v[1]) & mask; v[1] = rotl(v[1], 13) ^ v[0]; v[0] = rotl(v[0], 32)
        v[2] = (v[2] + v[3]) & mask; v[3] = rotl(v[3], 16) ^ v[2]
        v[0] = (v[0] + v[3]) & mask; v[3] = rotl(v[3], 21) ^ v[0]
        v[2] = (v[2] + v[1]) & mask; v[1] = rotl(v[1], 17) ^ v[2]; v[2] = rotl(v[2], 32)

    n = len(data) - len(data) % 8
    for i in range(0, n, 8):
        m = int.from_bytes(data[i:i+8], 'little')
        v[3] ^= m
        rnd(); rnd()
        v[0] ^= m
    b = (len(data) & 0xFF) << 56 | int.from_bytes(data[n:], 'little')
    v[3] ^= b
    rnd(); rnd()
    v[0] ^= b
    v[2] ^= 0xFF
    rnd(); rnd(); rnd(); rnd()
    return v[0] ^ v[1] ^ v[2] ^ v[3]


def mask_stream(n):
    """The bytes XORed with the key in the firmware table (xorshift32)."""
    s = MASK_SEED
    out = []
    for i in range(n):
        s ^= (s << 13) & 0xFFFFFFFF
        s ^= s >> 17
        s ^= (s << 5) & 0xFFFFFFFF
        out.append((s ^ (i * 37)) & 0xFF)
    return out


def check(text, key):
    """Returns (ok, fields or error message)."""
    text = text.strip()
    body, sep, sig = text.rpartition(':')
    parts = body.split(':')
    if not sep or len(parts) != 5 or parts[0] != PREFIX:
        return False, 'not a badge score'
    try:
        ok = int(sig, 16) == siphash24(key, body.encode('utf-8'))
    except ValueError:
        return False, 'bad signature format'
    _, game, score, badge_id, name = parts
    return ok, {'game': game, 'score': score, 'id': badge_id, 'name': name}


def sort_key(row):
    game, score = row['game'], row['score']
    if game in ('MORPION', 'P4'):
        if score.endswith('V'):  # Record: wins against the cicada
            return (-int(score[:-1]), 0)
        w, l, d = (int(x) for x in score.split('-'))
        return (-w, l)
    if game == 'REFLEX':
        return (int(score.rstrip('ms')),)
    return (-int(score),)


def main():
    parser = argparse.ArgumentParser(description='Check the score QR codes of the badges')
    parser.add_argument('inputs', nargs='*', help='texts of QR codes, or files with one text per line')
    parser.add_argument('--key', default=os.environ.get('BADGE_SCORE_KEY'), help='128 bits key, 32 hex digits')
    parser.add_argument('--make-key', action='store_true', help='generate a new key and its table for score_code.c')
    args = parser.parse_args()

    if args.make_key:
        key = secrets.token_bytes(16)
        table = [k ^ m for k, m in zip(key, mask_stream(16))]
        print('key (keep it for this script):', key.hex().upper())
        print('table for score_code.c:')
        print('    ' + ', '.join(f'0x{b:02x}' for b in table))
        return
    if not args.key:
        sys.exit('the key is needed: --key or BADGE_SCORE_KEY')
    key = bytes.fromhex(args.key)
    if len(key) != 16:
        sys.exit('the key has 32 hex digits')

    texts = []
    for i in args.inputs or ['-']:
        if i == '-':
            texts += sys.stdin.read().splitlines()
        elif os.path.isfile(i):
            texts += open(i, encoding='utf-8').read().splitlines()
        else:
            texts.append(i)

    valid, bad = {}, 0
    for t in filter(str.strip, texts):
        ok, info = check(t, key)
        if not ok:
            bad += 1
            print(f'INVALID  {t.strip()}  ({info if isinstance(info, str) else "wrong signature"})')
            continue
        valid[t.strip()] = info  # The same QR code scanned twice counts once
    by_game = {}
    for info in valid.values():
        by_game.setdefault(info['game'], []).append(info)
    for game, rows in sorted(by_game.items()):
        print(f'\n{game}')
        # Best score of each badge
        best = {}
        for r in sorted(rows, key=sort_key):
            best.setdefault(r['id'], r)
        for rank, r in enumerate(sorted(best.values(), key=sort_key), 1):
            print(f'  {rank:2}. {r["score"]:>8}  {r["name"]} ({r["id"]})')
    print(f'\n{len(valid)} valid, {bad} invalid')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
