#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Generator of the crypto challenges of the badge (src/menu/crypto_ctf.c).

SPOILERS: this file is the solution file of the organizers, it contains the plaintext answers.

The texts are written here in clear, the ciphertexts are computed by the functions below (never by hand),
wrapped to the width of the screen with the metrics of the small font (src/gfx/gfx_fonts.c) and checked:
at most 7 lines of text, 3 lines of hint, a title that fits in the medium font.
The firmware only gets the SipHash-2-4 of each answer and the pieces of the final flag XORed with a SipHash stream.

Usage:
    python tools/crypto_ctf_make.py            # prints the C table to paste in src/menu/crypto_ctf.c
    python tools/crypto_ctf_make.py --update   # replaces the table between the GENERATED markers of crypto_ctf.c
    python tools/crypto_ctf_make.py --answers  # prints the challenges with their answers (for the organizers)
"""

import argparse
import base64
import hashlib
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
C_FILE = os.path.join(ROOT, 'src', 'menu', 'crypto_ctf.c')
FONTS = os.path.join(ROOT, 'src', 'gfx', 'gfx_fonts.c')

# Not a secret: the same key is in crypto_ctf.c. The answers are only hidden from "strings" on the firmware.
KEY = b'SecSea-CryptoCTF'
FLAG_INNER = 'L4_C1G4L3_CH1FFR3_4_L4_C10T4T'  # The final flag is SECSEA{FLAG_INNER}
MAX_WIDTH = 196  # Pixels, in the 200 pixels of the screen
MAX_LINES, MAX_HINT_LINES, MAX_PIECE = 7, 3, 4
MAX_ANSWER = 32  # CRYPTO_CTF_ANSWER_MAX
MAX_FLAG = 48  # CRYPTO_CTF_FLAG_MAX, with the final 0


# ---- SipHash-2-4, as score_code_siphash() ----

def siphash(key, data):
    mask = (1 << 64) - 1

    def rotl(x, b):
        return ((x << b) | (x >> (64 - b))) & mask

    k0, k1 = int.from_bytes(key[:8], 'little'), int.from_bytes(key[8:], 'little')
    v = [0x736f6d6570736575 ^ k0, 0x646f72616e646f6d ^ k1, 0x6c7967656e657261 ^ k0, 0x7465646279746573 ^ k1]

    def rnd():
        v[0] = (v[0] + v[1]) & mask; v[1] = rotl(v[1], 13); v[1] ^= v[0]; v[0] = rotl(v[0], 32)
        v[2] = (v[2] + v[3]) & mask; v[3] = rotl(v[3], 16); v[3] ^= v[2]
        v[0] = (v[0] + v[3]) & mask; v[3] = rotl(v[3], 21); v[3] ^= v[0]
        v[2] = (v[2] + v[1]) & mask; v[1] = rotl(v[1], 17); v[1] ^= v[2]; v[2] = rotl(v[2], 32)

    n = len(data) - len(data) % 8
    for i in range(0, n, 8):
        m = int.from_bytes(data[i:i + 8], 'little')
        v[3] ^= m
        rnd(); rnd()
        v[0] ^= m
    b = (len(data) & 0xFF) << 56
    for i in range(n, len(data)):
        b |= data[i] << (8 * (i - n))
    v[3] ^= b
    rnd(); rnd()
    v[0] ^= b
    v[2] ^= 0xFF
    rnd(); rnd(); rnd(); rnd()
    return v[0] ^ v[1] ^ v[2] ^ v[3]


def answer_hash(i, answer):
    """Hash checked by crypto_ctf_check(): index of the challenge, ':', normalized answer"""
    return siphash(KEY, bytes([i]) + b':' + answer.encode('ascii'))


def piece_stream(i):
    """8 bytes XORed with the piece of the flag of the challenge i (crypto_ctf.c: piece_stream())"""
    return siphash(KEY, b'piece' + bytes([i])).to_bytes(8, 'little')


def normalize(s):
    return ' '.join(s.upper().split())


# ---- Ciphers ----

def caesar(text, shift):
    return ''.join(chr((ord(c) - 65 + shift) % 26 + 65) if 'A' <= c <= 'Z' else c for c in text)


def atbash(text):
    return ''.join(chr(90 - (ord(c) - 65)) if 'A' <= c <= 'Z' else c for c in text)


def vigenere(text, key):
    out, k = [], 0
    for c in text:
        if 'A' <= c <= 'Z':
            out.append(chr((ord(c) - 65 + ord(key[k % len(key)]) - 65) % 26 + 65))
            k += 1  # The key only moves on letters
        else:
            out.append(c)
    return ''.join(out)


def vigenere_decrypt(text, key):
    return vigenere(text, ''.join(chr((26 - (ord(c) - 65)) % 26 + 65) for c in key))


MORSE = {
    'A': '.-', 'B': '-...', 'C': '-.-.', 'D': '-..', 'E': '.', 'F': '..-.', 'G': '--.', 'H': '....', 'I': '..',
    'J': '.---', 'K': '-.-', 'L': '.-..', 'M': '--', 'N': '-.', 'O': '---', 'P': '.--.', 'Q': '--.-', 'R': '.-.',
    'S': '...', 'T': '-', 'U': '..-', 'V': '...-', 'W': '.--', 'X': '-..-', 'Y': '-.--', 'Z': '--..',
    '0': '-----', '1': '.----', '2': '..---', '3': '...--', '4': '....-', '5': '.....', '6': '-....', '7': '--...',
    '8': '---..', '9': '----.',
}


def morse(text):
    """Letters separated by a space, words by " / " """
    return ' / '.join(' '.join(MORSE[c] for c in word) for word in text.split())


def unmorse(code):
    rev = {v: k for k, v in MORSE.items()}
    return ' '.join(''.join(rev[c] for c in word.split()) for word in code.split(' / '))


def scytale(text, faces):
    """The strip wound around a rod with <faces> faces: written along the rod (rows), read unwound (columns)"""
    text += 'X' * (-len(text) % faces)
    cols = len(text) // faces
    return ''.join(text[r * cols + c] for c in range(cols) for r in range(faces))


def unscytale(text, faces):
    return ''.join(text[r::faces] for r in range(faces))


def xor_hex(text, key):
    return ' '.join('%02X' % (ord(c) ^ key) for c in text)


def binary(text):
    return ' '.join(format(ord(c), '08b') for c in text)


def hexa(text):
    return ' '.join('%02X' % ord(c) for c in text)


def sha1_prefix(word):
    return hashlib.sha1(word.encode('ascii')).hexdigest()[:4].upper()


# ---- The challenges, in order of difficulty ----
# text: the paragraphs are wrapped to the width of the screen, '\n' forces a new line.
# nowrap: the lines are kept as written (acrostic), an error if one is too wide.

ACROSTIC = ['Pour qui trouve une faille,', 'Alerter est le vrai travail,',
            'Taire l\'exploit, prévenir,', 'Corriger avant de trahir :', 'Hacker, c\'est protéger.']
HASH_WORDS = ['FADA', 'MINOT', 'PITCHOUN', 'CAGOLE', 'GABIAN', 'PEUCHERE', 'ESQUICHE']

CHALLENGES = [
    dict(title='Acrostiche', answer='PATCH', nowrap=True,
         text='Lis entre les lignes :\n' + '\n'.join(ACROSTIC),
         hint='Regarde la première lettre\nde chaque vers.',
         solve=lambda c: ''.join(line[0] for line in ACROSTIC).upper()),
    dict(title='César', answer='SOLEIL',
         text='Jules a chiffré ce message\nen décalant les lettres :\n' + caesar('LE MOT EST SOLEIL', 3),
         hint='Chaque lettre est décalée\nde 3 rangs : A devient D.',
         solve=lambda c: caesar(c['text'].split('\n')[-1], -3).split()[-1]),
    dict(title='ROT13', answer='LUMIERE',
         text='Les frères qui ont tourné à\nLa Ciotat en 1895, en ROT13 :\n' + caesar('LES FRERES LUMIERE', 13),
         hint='ROT13 : décalage de 13,\nle même pour chiffrer et\ndéchiffrer. Un seul nom.',
         solve=lambda c: caesar(c['text'].split('\n')[-1], 13).split()[-1]),
    dict(title='Morse', answer='CHANTIER NAVAL',
         text='Signal capté au large du\nBec de l\'Aigle :\n' + morse('CHANTIER NAVAL'),
         morse=morse('CHANTIER NAVAL'),
         hint='Le badge peut le jouer :\nbips et LEDs. Point = court,\ntiret = long, / = espace.',
         solve=lambda c: unmorse(c['morse'])),
    dict(title='Binaire', answer='BOULE',
         text='Au jeu provençal né ici en\n1907, on lance une... (ASCII)\n' + binary('BOULE'),
         hint='Chaque octet (8 bits) est un\ncode ASCII : 01000001 = A.',
         solve=lambda c: ''.join(chr(int(b, 2)) for b in c['text'].split('\n', 2)[2].split())),
    dict(title='Hexadécimal', answer='CALANQUE',
         text='Entre Marseille et La Ciotat,\nla mer entre dans la roche :\n' + hexa('CALANQUE'),
         hint='Deux chiffres hexa = un\noctet = un caractère ASCII.\n41 = A.',
         solve=lambda c: bytes.fromhex(c['text'].split('\n', 2)[2]).decode()),
    dict(title='Base64', answer='EDEN THEATRE',
         text='Le plus vieux cinéma encore\nouvert au monde est ici :\n'
              + base64.b64encode(b'EDEN THEATRE').decode(),
         hint='Base64 : 4 caractères pour\n3 octets. CyberChef ou\n"base64 -d" le décodent.',
         solve=lambda c: base64.b64decode(c['text'].split('\n')[-1]).decode()),
    dict(title='Atbash', answer='MISTRAL',
         text='Le miroir de l\'alphabet :\nA vaut Z, B vaut Y...\n' + atbash('LE VENT FOU SE NOMME MISTRAL'),
         hint='Chaque lettre est remplacée\npar son symétrique :\nA<>Z, B<>Y, C<>X...',
         solve=lambda c: atbash(c['text'].split('\n', 2)[2].replace('\n', ' ')).split()[-1]),
    dict(title='Scytale', answer='TRAIN',
         text='Un Spartiate a enroulé la\nbande sur un bâton à 4 faces.\nQui entre en gare ?\n'
              + scytale('LAREPONSEESTLETRAIN', 4),
         hint='Lis une lettre sur 4, puis\nrecommence un cran plus loin.',
         solve=lambda c: unscytale(c['text'].split('\n')[-1], 4)[len('LAREPONSEESTLE'):].rstrip('X')),
    dict(title='XOR', answer='SARDINE',
         text='Chiffré par XOR avec un seul\noctet : l\'année de naissance\nde la pétanque, modulo 256.\n'
              + xor_hex('SARDINE', 1907 % 256),
         hint='1907 mod 256 = 115 = 0x73.\nXOR avec 0x73 octet par\noctet, puis lire en ASCII.',
         solve=lambda c: ''.join(chr(int(b, 16) ^ 0x73) for b in c['text'].split('\n')[-1].split())),
    dict(title='Vigenère', answer='PIEDS TANQUES',
         text='L\'origine de la pétanque,\nchiffrée avec Vigenère.\nClé : la cigale, en provençal.\n'
              + vigenere('PIEDS TANQUES', 'CIGALO'),
         hint='Mistral l\'écrivait CIGALO :\nc\'est la clé. Les espaces ne\nconsomment pas la clé.',
         solve=lambda c: vigenere_decrypt(c['text'].split('\n')[-1], 'CIGALO')),
    dict(title='Hash tronqué', answer='GABIAN',
         text='SHA-1 d\'un mot en majuscules,\nses 4 premiers chiffres hexa :\n' + sha1_prefix('GABIAN') + '\nLequel ?\n'
              + ' '.join(HASH_WORDS),
         hint='SHA-1 de chaque mot, sans\nretour à la ligne (echo -n),\npuis compare le début.',
         solve=lambda c: [w for w in HASH_WORDS if sha1_prefix(w) == c['text'].split('\n')[2]][0]),
]


# ---- Wrapping with the metrics of the fonts ----

def load_font(name):
    """{codepoint: advance in 1/16 pixels} of gfx_font_<name>_glyphs in gfx_fonts.c"""
    src = open(FONTS, encoding='utf-8').read()
    start = src.index('gfx_font_%s_glyphs[] = {' % name)
    table = src[start:src.index('};', start)]
    return {int(cp, 16): int(adv) for cp, adv in re.findall(r'\{0x([0-9A-Fa-f]+),\s*(\d+),', table)}


def text_width(font, text):
    for c in text:
        if ord(c) not in font:
            raise ValueError('No glyph for %r in "%s"' % (c, text))
    return (sum(font[ord(c)] for c in text) + 8) // 16


def wrap(font, text, nowrap=False):
    lines = []
    for para in text.split('\n'):
        if nowrap or text_width(font, para) <= MAX_WIDTH:
            if text_width(font, para) > MAX_WIDTH:
                raise ValueError('Line too wide: "%s"' % para)
            lines.append(para)
            continue
        line = ''
        for word in para.split(' '):
            cand = word if not line else line + ' ' + word
            if text_width(font, cand) <= MAX_WIDTH:
                line = cand
            else:
                if text_width(font, word) > MAX_WIDTH:
                    raise ValueError('Word too wide: "%s"' % word)
                lines.append(line)
                line = word
        lines.append(line)
    return lines


# ---- Output ----

def split_flag(n):
    """FLAG_INNER cut in n pieces as even as possible"""
    q, r = divmod(len(FLAG_INNER), n)
    pieces, pos = [], 0
    for i in range(n):
        size = q + (1 if i < r else 0)
        pieces.append(FLAG_INNER[pos:pos + size])
        pos += size
    return pieces


def c_string(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"'


def build():
    small, medium = load_font('small'), load_font('medium')
    if len(CHALLENGES) > 32:
        raise ValueError('At most 32 challenges (solved mask of 32 bits)')
    pieces = split_flag(len(CHALLENGES))
    if len('SECSEA{%s}' % FLAG_INNER) >= MAX_FLAG:
        raise ValueError('Flag too long for CRYPTO_CTF_FLAG_MAX')
    out = []
    for i, c in enumerate(CHALLENGES):
        answer = normalize(c['answer'])
        if answer != c['answer'] or not re.fullmatch(r'[A-Z0-9 ]{1,%d}' % MAX_ANSWER, answer):
            raise ValueError('Bad answer %r' % c['answer'])
        if text_width(medium, c['title']) > MAX_WIDTH:
            raise ValueError('Title too wide: %s' % c['title'])
        lines = wrap(small, c['text'], c.get('nowrap', False))
        if len(lines) > MAX_LINES:
            raise ValueError('%s: %d lines of text' % (c['title'], len(lines)))
        hint = wrap(small, c['hint']) if c['hint'] else []
        if len(hint) > MAX_HINT_LINES:
            raise ValueError('%s: %d lines of hint' % (c['title'], len(hint)))
        # The challenge must be solvable from its text: the solver below only reads the text shown
        solved = normalize(c['solve'](dict(c, text='\n'.join(lines))))
        if solved != answer:
            raise ValueError('%s: the text gives %r, not %r' % (c['title'], solved, answer))
        if len(pieces[i]) > MAX_PIECE:
            raise ValueError('Piece too long: %s' % pieces[i])
        stream = piece_stream(i)
        out.append(dict(title=c['title'], text='\n'.join(lines), hint='\n'.join(hint), morse=c.get('morse'),
                        hash=answer_hash(i, answer), piece=[ord(ch) ^ stream[k] for k, ch in enumerate(pieces[i])],
                        answer=answer, clear_piece=pieces[i]))
    # The hash truncated to 4 hex digits must single out the answer
    prefixes = [sha1_prefix(w) for w in HASH_WORDS]
    if len(set(prefixes)) != len(prefixes):
        raise ValueError('Two words of the hash challenge have the same prefix')
    return out


def c_table(challenges):
    lines = ['/* BEGIN GENERATED by tools/crypto_ctf_make.py, do not edit by hand */',
             'static const challenge_t CHALLENGES[] = {']
    for c in challenges:
        lines.append('    {  /* %s */' % c['title'])
        lines.append('        %s,' % c_string(c['title']))
        lines.append('        %s,' % c_string(c['text']))
        lines.append('        %s,' % c_string(c['hint']))
        lines.append('        %s,' % (c_string(c['morse']) if c['morse'] else 'NULL'))
        lines.append('        0x%016XULL,' % c['hash'])
        lines.append('        {%s}, %d,' % (', '.join('0x%02X' % b for b in c['piece']), len(c['piece'])))
        lines.append('    },')
    lines.append('};')
    lines.append('/* END GENERATED */')
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description='Generator of the crypto challenges of the badge')
    parser.add_argument('--update', action='store_true', help='replace the table in src/menu/crypto_ctf.c')
    parser.add_argument('--answers', action='store_true', help='print the answers (spoilers)')
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding='utf-8')  # Accents on a Windows console
    try:
        challenges = build()
    except ValueError as e:
        sys.exit('Error: %s' % e)
    if args.answers:
        for i, c in enumerate(challenges):
            print('%2d %-14s %-15s %s' % (i, c['title'], c['answer'], c['clear_piece']))
        print('Flag: SECSEA{%s}' % FLAG_INNER)
        return
    table = c_table(challenges)
    if not args.update:
        print(table)
        return
    src = open(C_FILE, encoding='utf-8').read()
    start = src.index('/* BEGIN GENERATED')
    end = src.index('/* END GENERATED */') + len('/* END GENERATED */')
    with open(C_FILE, 'w', encoding='utf-8', newline='\n') as f:
        f.write(src[:start] + table + src[end:])
    print('Updated', C_FILE)


if __name__ == '__main__':
    main()
