#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Checks a book of the gamebooks ("Livres-jeux" of the badge, format in docs/fr/livres_jeux.md) before copying it to
the LIVRES folder of the SD card: the same reading as the badge (src/menu/gamebook_parse.c), then
- errors: no section, a section twice, a choice to a missing section, too many sections, choices or items;
- warnings: sections that can't be reached from the start, sections from which no ending can be reached, a die
  roll not covered, an item tested but never given, texts too long for the buffer of the badge, characters not in
  the fonts, the pages of each section (8 lines of the small font).

Usage:
  python tools/gamebook_check.py livre.txt [autre.txt...]    check (exit code 1 on errors)
  python tools/gamebook_check.py livre.txt --pages             also the number of pages of each section
  python tools/gamebook_check.py livre.txt --play              play it in the terminal
  python tools/gamebook_check.py livre.txt --c src/menu/gamebook_builtin.c   write the book built in the firmware
"""

import argparse
import os
import random
import re
import sys
import unicodedata
from collections import deque

# Limits of the badge (gamebook_parse.h)
MAX_SECTIONS = 400
MAX_ITEMS = 8
MAX_CHOICES = 8
TEXT_MAX = 2048 - 4  # Bytes of text, with the "..." of a cut text
CHOICE_LEN = 96
ITEM_LEN = 24
# Layout of the reading page (gamebook.c)
TEXT_WIDTH = 192
INDENT = 10
LINES_PER_PAGE = 8

HERE = os.path.dirname(os.path.abspath(__file__))
FONTS = os.path.join(HERE, '..', 'src', 'gfx', 'gfx_fonts.c')

END_NONE, END_NEUTRAL, END_WIN, END_LOSE = 0, 1, 2, 3
END_NAMES = {END_NEUTRAL: 'FIN', END_WIN: 'FIN gagné', END_LOSE: 'FIN perdu'}

REPLACE = {}
for chars, rep in (('‘’‚′ʼ', "'"), ('«»“”„‹›', '"'),
                   ('…', '...'), ('‐‑‒–—―−•·', '-'),
                   ('œ', 'oe'), ('Œ', 'OE'), ('æ', 'ae'), ('Æ', 'AE'),
                   ('    \t', ' '), ('﻿', '')):
    for c in chars:
        REPLACE[c] = rep


def normalize(s):
    """Same as gb_normalize(): the characters the badge draws."""
    out = []
    skip_spaces = False
    for c in s:
        rep = REPLACE.get(c)
        if rep is None:
            rep = '' if ord(c) < 0x20 or ord(c) == 0x7f else '?' if ord(c) > 0xffff else c
        if rep == ' ' and (skip_spaces or (out and out[-1] == ' ')):
            continue
        if c in '»›':
            while out and out[-1] == ' ':
                out.pop()
        out.append(rep)
        skip_spaces = c in '«‹'
    return ''.join(out)


def load_font():
    """Code points and advances (pixels) of the small font, None without the sources."""
    try:
        src = open(FONTS, encoding='utf-8').read()
    except OSError:
        return None
    small = src.split('gfx_font_small_bitmap')[0]
    return {int(cp, 16): int(adv) / 16 for cp, adv in re.findall(r'\{0x([0-9A-Fa-f]{4}), (\d+),', small)}


def glyph(font, c):
    """The character drawn (gfx.c: the letter without its accent when missing, '?' when unknown)."""
    if ord(c) in font:
        return c
    base = unicodedata.normalize('NFD', c)[0]
    if 0xc0 <= ord(c) < 0x180 and ord(base) in font:
        return base
    return '?'


class Choice:
    def __init__(self, target, cond, label, line):
        self.target, self.label, self.line = target, label or 'Continuer', line
        self.item, self.negate, self.dice = None, False, None
        cond = cond.strip()
        m = re.match(r'(?i)d[eéÉ]\s*([0-9])\s*(?:(?:-|a|à)\s*([0-9]))?', cond)
        if m:
            a = int(m.group(1))
            b = int(m.group(2)) if m.group(2) else a
            a, b = sorted((a, b))
            self.dice = (min(max(a, 1), 6), min(max(b, 1), 6))
        elif cond:
            if cond.startswith('!'):
                self.negate, cond = True, cond[1:].strip()
            self.item = normalize(cond)[:ITEM_LEN - 1].lower() or None


class Section:
    def __init__(self, number, end, line):
        self.number, self.end, self.line = number, end, line
        self.paragraphs = []  # Each one: list of lines
        self.choices = []
        self.gain, self.lose = [], []
        self.text = ''


class Book:
    def __init__(self, path):
        self.path = path
        self.errors, self.warnings = [], []
        self.title = self.author = ''
        self.sections = {}
        self.order = []
        self.items = []  # In the order of first use, like the badge
        raw = open(path, 'rb').read()
        try:
            text = raw.decode('utf-8')
        except UnicodeDecodeError:
            text = raw.decode('cp1252', errors='replace')
            self.warnings.append("le fichier n'est pas en UTF-8 : lu en Windows-1252")
        if text.startswith('﻿'):
            text = text[1:]
        self.parse(re.split(r'\r\n|\r|\n', text))

    def item(self, name):
        name = normalize(name.strip())[:ITEM_LEN - 1]
        key = name.lower()
        if key and key not in [i.lower() for i in self.items]:
            if len(self.items) >= MAX_ITEMS:
                self.errors.append(f'plus de {MAX_ITEMS} objets : « {name} » est ignoré par le badge')
                return key
            self.items.append(name)
        return key

    def parse(self, lines):
        sec = None
        header = 0
        paragraph = []
        for n, line in enumerate(lines, 1):
            line = line.rstrip(' \t')
            p = line.lstrip(' \t')
            if p.startswith('//'):
                continue
            m = re.match(r'==+\s*(\d+)\s*=*\s*(.*)$', p)
            if m and 1 <= int(m.group(1)) <= 65535:
                number = int(m.group(1))
                rest = m.group(2).lower()
                end = END_NONE
                if re.match(r'fin(\s|$)', rest):
                    rest = rest[3:].strip()
                    end = END_WIN if rest.startswith('gagn') else END_LOSE if rest.startswith('perdu') else END_NEUTRAL
                if number in self.sections:
                    self.errors.append(f'ligne {n} : la section {number} existe déjà (ligne {self.sections[number].line})')
                    sec = Section(number, end, n)  # Read but not reachable: the badge uses the first one
                    continue
                if len(self.order) >= MAX_SECTIONS:
                    self.errors.append(f'ligne {n} : plus de {MAX_SECTIONS} sections, la section {number} est ignorée')
                    sec = Section(number, end, n)
                    continue
                sec = Section(number, end, n)
                self.sections[number] = sec
                self.order.append(number)
                paragraph = None
                continue
            if p.startswith('=='):
                self.warnings.append(f'ligne {n} : « {p} » n\'est pas un début de section (== numéro) : lu comme du texte')
            if sec is None:
                if p:
                    if header == 0:
                        self.title = normalize(p)
                    elif header == 1:
                        self.author = normalize(p)
                    header += 1
                continue
            if not p:
                paragraph = None
                continue
            m = re.match(r'(?:->|→)\s*(\d+)\s*(?:\[([^\]]*)\]?)?\s*:?\s*(.*)$', p)
            if m and 1 <= int(m.group(1)) <= 65535:
                c = Choice(int(m.group(1)), m.group(2) or '', normalize(m.group(3)), n)
                if c.item:
                    c.item = self.item(m.group(2).strip().lstrip('!'))
                if len(sec.choices) >= MAX_CHOICES:
                    self.errors.append(f'ligne {n} : plus de {MAX_CHOICES} choix dans la section {sec.number}, ignoré')
                else:
                    sec.choices.append(c)
                    if len(c.label.encode()) > CHOICE_LEN - 1:
                        self.warnings.append(f'ligne {n} : choix trop long, coupé à {CHOICE_LEN - 1} octets')
                continue
            effects = re.findall(r'([+-])\[([^\]]*)\]', p)
            if effects and re.fullmatch(r'(\s*[+-]\[[^\]]*\])+\s*', p):
                for sign, name in effects:
                    (sec.gain if sign == '+' else sec.lose).append(self.item(name))
                continue
            if p.startswith('->'):
                self.warnings.append(f'ligne {n} : « {p} » ressemble à un choix sans numéro : lu comme du texte')
            dialogue = p.startswith('- ') or p[:1] in '—–'
            if paragraph is None or dialogue:
                paragraph = []
                sec.paragraphs.append(paragraph)
            paragraph.append(p)
        for sec in self.sections.values():
            sec.text = '\n'.join(normalize(' '.join(par)) for par in sec.paragraphs)
            if not sec.choices and sec.end == END_NONE:
                sec.end = END_NEUTRAL
                self.warnings.append(f'section {sec.number} (ligne {sec.line}) : pas de choix, considérée comme une fin '
                                     '(écrire « FIN » pour le dire)')

    @property
    def start(self):
        return self.order[0] if self.order else None

    def check(self, font):
        if not self.title:
            self.warnings.append('pas de titre (première ligne)')
        if not self.order:
            self.errors.append('aucune section (une ligne « == 1 »)')
            return
        given = {i for s in self.sections.values() for i in s.gain}
        for s in self.sections.values():
            for c in s.choices:
                if c.target not in self.sections:
                    self.errors.append(f'ligne {c.line} : la section {c.target} n\'existe pas')
                if c.item and c.item not in given and not c.negate:
                    self.warnings.append(f'ligne {c.line} : l\'objet « {c.item} » n\'est donné nulle part (+[{c.item}])')
            dice = [c.dice for c in s.choices if c.dice]
            if dice:
                missing = [v for v in range(1, 7) if not any(a <= v <= b for a, b in dice)]
                if missing:
                    self.warnings.append(f'section {s.number} : aucun choix pour le dé {", ".join(map(str, missing))}')
            size = len(s.text.encode())
            if size > TEXT_MAX:
                self.warnings.append(f'section {s.number} : texte trop long ({size} octets, {TEXT_MAX} max), coupé')
            if s.end and s.choices:
                self.warnings.append(f'section {s.number} : une fin avec des choix (ils sont ignorés)')
        # Reachability (all the choices, whatever their condition)
        seen = {self.start}
        todo = deque([self.start])
        while todo:
            for c in self.sections[todo.popleft()].choices:
                if c.target in self.sections and c.target not in seen:
                    seen.add(c.target)
                    todo.append(c.target)
        unreachable = [n for n in self.order if n not in seen]
        if unreachable:
            self.warnings.append('sections jamais atteintes depuis la section '
                                 f'{self.start} : {", ".join(map(str, unreachable))}')
        endings = [n for n in seen if self.sections[n].end]
        if not endings:
            self.errors.append('aucune fin atteignable')
        # From which sections can an ending be reached
        can_end = set(endings)
        changed = True
        while changed:
            changed = False
            for n in seen:
                if n not in can_end and any(c.target in can_end for c in self.sections[n].choices):
                    can_end.add(n)
                    changed = True
        traps = sorted(n for n in seen if n not in can_end)
        if traps:
            self.warnings.append(f'sections sans aucune fin possible ensuite (boucle) : {", ".join(map(str, traps))}')
        if font:
            missing = set()
            for s in self.sections.values():
                for text in [s.text] + [c.label for c in s.choices]:
                    missing |= {c for c in text if c != '\n' and glyph(font, c) == '?' and c != '?'}
            if missing:
                self.warnings.append('caractères absents des polices (affichés « ? ») : ' + ' '.join(sorted(missing)))

    def pages(self, font, number):
        """Pages of the reading page of the badge (approximation of gamebook.c)."""
        def width(s):
            return sum(font.get(ord(glyph(font, c)), 8) for c in s)

        def wrap(text, w, indent):
            lines = 0
            for par in text.split('\n'):
                words, line, first = par.split(' '), '', True
                for word in words:
                    cand = (line + ' ' + word) if line else word
                    if width(cand) > w - (indent if first else 0) and line:
                        lines += 1
                        line, first = word, False
                    else:
                        line = cand
                lines += 1
            return lines
        s = self.sections[number]
        lines = wrap(s.text, TEXT_WIDTH, INDENT) if s.text else 0
        lines += 1 + sum(wrap(c.label, TEXT_WIDTH - 12, 0) for c in s.choices) + (3 if s.end else 0)
        return lines, (lines + LINES_PER_PAGE - 1) // LINES_PER_PAGE


def report(book, font, show_pages):
    print(f'{book.path}')
    print(f'  « {book.title} »' + (f' par {book.author}' if book.author else ''))
    if book.order:
        ends = [n for n in book.order if book.sections[n].end]
        print(f'  {len(book.order)} sections, début : {book.start}, {len(ends)} fins '
              f'({sum(book.sections[n].end == END_WIN for n in ends)} gagnées, '
              f'{sum(book.sections[n].end == END_LOSE for n in ends)} perdues), '
              f'objets : {", ".join(book.items) or "aucun"}')
    if show_pages and font:
        for n in book.order:
            lines, pages = book.pages(font, n)
            print(f'  section {n:4} : {len(book.sections[n].text.encode()):4} octets, {lines:3} lignes, {pages} page(s)'
                  + (f'  {END_NAMES[book.sections[n].end]}' if book.sections[n].end else ''))
    for e in book.errors:
        print(f'  ERREUR : {e}')
    for w in book.warnings:
        print(f'  attention : {w}')
    if not book.errors:
        print('  OK' + (' (avec des remarques)' if book.warnings else ''))


def play(book):
    items, number = set(), book.start
    while True:
        s = book.sections.get(number)
        if not s:
            print(f'\n*** La section {number} est introuvable ***')
            return
        items |= set(s.gain)
        items -= set(s.lose)
        print(f'\n===== {number} =====\n')
        print(s.text)
        if s.end:
            print('\n~ FIN ~ ' + {END_WIN: 'Gagné !', END_LOSE: 'Perdu...'}.get(s.end, ''))
            return
        roll = random.randint(1, 6) if any(c.dice for c in s.choices) else 0
        if roll:
            print(f'\n(Le dé donne {roll})')
        choices = [c for c in s.choices
                   if (not c.dice or c.dice[0] <= roll <= c.dice[1])
                   and (not c.item or (c.item in items) != c.negate)]
        if items:
            print(f'(Objets : {", ".join(sorted(items))})')
        for i, c in enumerate(choices, 1):
            print(f'  {i}. {c.label}')
        if not choices:
            print('Aucun choix possible.')
            return
        while True:
            answer = input('> ').strip()
            if answer in ('q', 'Q'):
                return
            if answer.isdigit() and 1 <= int(answer) <= len(choices):
                number = choices[int(answer) - 1].target
                break


def write_c(book, out):
    raw = open(book.path, 'rb').read().decode('utf-8')
    if raw.startswith('﻿'):
        raw = raw[1:]
    raw = raw.replace('\r\n', '\n').replace('\r', '\n')
    lines = []
    for line in raw.split('\n'):
        esc = line.replace('\\', '\\\\').replace('"', '\\"').replace('??', '?\\?')  # No trigraph
        lines.append(f'    "{esc}\\n"')
    if lines and lines[-1] == '    "\\n"':
        lines.pop()  # The final newline of the file
    name = os.path.basename(book.path)
    with open(out, 'w', encoding='utf-8', newline='\n') as f:
        f.write('/* badge_secsea © 2025 by Hack In Provence is licensed under\n'
                ' * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.\n'
                ' * To view a copy of this license,\n'
                ' * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */\n\n'
                f'/* The book built in the firmware (no SD card needed), generated from docs/sd/LIVRES/{name} by\n'
                f' *   python tools/gamebook_check.py docs/sd/LIVRES/{name} --c src/menu/gamebook_builtin.c\n'
                ' * Edit the .txt file, then generate this file again. */\n\n'
                '#include "gamebook_builtin.h"\n\n'
                'static const char BOOK_0[] =\n')
        f.write('\n'.join(lines) + ';\n\n')
        f.write('const gamebook_builtin_t gamebook_builtins[] = {\n'
                f'    {{"builtin/{name}", BOOK_0, sizeof(BOOK_0) - 1}},\n'
                '};\n'
                'const int gamebook_builtin_count = sizeof(gamebook_builtins) / sizeof(gamebook_builtins[0]);\n')
    print(f'  -> {out}')


def main():
    parser = argparse.ArgumentParser(description='Vérifie un livre-jeu du badge (dossier LIVRES de la carte SD)')
    parser.add_argument('books', nargs='+', help='fichiers .txt')
    parser.add_argument('--pages', action='store_true', help='taille et nombre de pages de chaque section')
    parser.add_argument('--play', action='store_true', help='jouer le livre dans le terminal (q : quitter)')
    parser.add_argument('--c', metavar='FICHIER', help='écrit le livre intégré au firmware (un seul livre)')
    args = parser.parse_args()
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')
    font = load_font()
    failed = False
    for path in args.books:
        book = Book(path)
        book.check(font)
        report(book, font, args.pages)
        failed |= bool(book.errors)
        if args.c and not book.errors:
            write_c(book, args.c)
        if args.play and not book.errors:
            play(book)
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
