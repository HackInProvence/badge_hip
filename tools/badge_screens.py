#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
All the screens of the badge (firmware badge_menu), captured through its USB serial port, for the documentation
and to check the texts:
- it goes through every theme of the menu and every entry (the admin theme too), and the pages inside the
  applications (help, game, hint, editor...) listed in PAGES below;
- it saves a PNG of each screen in docs/screens/ and writes docs/fr/ecrans.md and docs/en/screens.md;
- with the check of the texts of the firmware (key U), it lists the texts cut ("..."), too wide or drawn under
  the footer: docs/screens/checks.txt, and at the end of the pages.

Usage: python tools/badge_screens.py [--port COM9] [--only Jeux,Social]
The badge is restarted first. Nothing is sent by radio but the pages that do it by themselves (carrier, beacon of
the hot / cold, contact exchange, radio tuning); the settings are left as they were (the toggles are not pressed).
"""

import argparse
import os
import re
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..'))
sys.path.insert(0, HERE)
import badge_selftest as st  # noqa: E402
from badge_remote import Badge, find_port  # noqa: E402

OUT = os.path.join(ROOT, 'docs', 'screens')

# Entries of the menu that are not pages: toggles or actions (not pressed, the label is shown by the menu)
NOT_PAGES = {('Médias', 4), ('Badge', 3), ('Badge', 4), ('Réglages', 1), ('Réglages', 2), ('Admin', 8)}

# The pages inside an application: after it opened, the keys (a, b, x, y: short, A, B, X, Y: long press) and the
# screen to capture, with its caption. "back" at the end of a step goes back where the page was.
PAGES = {
    'Images': [('b', 'image', 'Une image de la carte SD', 'a')],
    'Vidéos': [],
    'Musique': [('b', 'lecture', 'Lecture d\'une musique', 'a')],
    'Lecture rapide': [('b', 'lecture', 'Lecture d\'un texte', 'a')],
    'Morpion': [('b', 'jeu', 'La partie', None)],
    'Puissance 4': [('b', 'jeu', 'La partie', None)],
    'Simon': [('b', 'jeu', 'La partie', None)],
    'Réflexes': [('b', 'jeu', 'La partie', None)],
    'Snake': [('b', 'jeu', 'La partie', None)],
    'Démineur': [('b', 'jeu', 'La partie', None)],
    '2048': [('b', 'jeu', 'La partie', None)],
    'Taquin': [('b', 'jeu', 'La partie', None)],
    'Sokoban': [('b', 'jeu', 'La partie', None)],
    'Mastermind': [('b', 'jeu', 'La partie', None)],
    'Pendu': [('b', 'jeu', 'La partie', None)],
    'CTF': [('b', 'code', 'Saisie d\'un code', 'a')],
    'Défis crypto': [('b', 'defi', 'Un défi', None), ('B', 'indice', 'Son indice', 'a'),
                     ('b', 'reponse', 'La saisie de la réponse', 'a'), ('a', None, None, None),
                     ('y', None, None, None), ('b', 'flag', 'Le flag final', 'a')],
    'Messages': [('b', 'destinataire', 'Choix du destinataire', None), ('b', 'choix', 'Choix du message', 'a')],
    'Contacts': [('b', 'carte', 'Ma carte (champs cochés : envoyés)', None),
                 ('b', 'saisie', 'Saisie d\'un champ', 'a'), ('a', None, None, None),
                 ('xb', 'echange', 'Echange des cartes', 'a'), ('xb', 'recus', 'Contacts reçus', 'a')],
    'Programme': [('b', 'talk', 'Un talk (et son QR code)', 'a')],
    'Radar des cigales': [],
    'Virus des cigales': [],
    'Choeur': [],
    'Décodeur 433 MHz': [],
    'Station météo': [],
    'Envoyer une image': [],
    'Recevoir une image': [],
    'Infrarouge': [],
    'Chasse 433 MHz': [],
    'Lampe': [('x', 'plus', 'Plus fort', None)],
    'Badge de talk': [('x', 'vert', 'Vert : tout va bien', None), ('x', 'orange', 'Orange : 5 min', None),
                      ('x', 'rouge', 'Rouge : fini', None), ('x', None, None, None), ('x', None, None, None)],
    'Crédits': [('x', 'p2', 'Page 2', None), ('x', 'p3', 'Page 3', None), ('x', 'p4', 'Page 4', None),
                ('x', 'p5', 'Page 5', None), ('x', 'p6', 'Page 6', None), ('x', 'p7', 'Page 7', None),
                ('x', 'p8', 'Page 8', None)],
    'Commandes radio': [],
    'LEDs des cigales': [('xxxx', 'mode', 'Le mode', None), ('b', None, None, None),
                         ('B', 'temps', 'Les temps du clignotement', None), ('B', None, None, None),
                         ('xb', None, None, None), ('B', 'fondu', 'Les temps du fondu', None), ('B', None, None, None),
                         ('b', None, None, None), ('xx', 'envoyer', 'Envoyer / rétablir', None)],
    'Annoncer un talk': [],
    'Vote (admin)': [],
    'Type du badge': [],
}

LONG_PAGES = {'Réglage radio': 20}  # Pages that work a while before their result (seconds)


def slug(text):
    t = text.lower()
    for a, b in (('é', 'e'), ('è', 'e'), ('ê', 'e'), ('à', 'a'), ('ç', 'c'), ('ô', 'o'), ('î', 'i'), ('œ', 'oe')):
        t = t.replace(a, b)
    return re.sub(r'[^a-z0-9]+', '_', t).strip('_')


def submenus():
    """[(theme, number of entries)] from src/menu/main.c"""
    src = open(os.path.join(ROOT, 'src', 'menu', 'main.c'), encoding='utf-8').read()
    return [(m.group(1), int(m.group(2))) for m in re.finditer(r'\{"([^"]+)", (\d+), \{M_', src)]


class Crawler:
    def __init__(self, t):
        self.t = t
        self.shots = []  # (theme, page, file, caption, checks)
        self.checks = []

    def ui(self, timeout=2.5):
        m = self.t.expect(r'^ui: (.+)$', timeout)
        return m.group(1) if m else None

    def checks_since(self, start):
        lines = [l[len('uicheck: '):] for l in self.t.lines[start:] if l.startswith('uicheck: ') and
                 not l.startswith('uicheck: o')]
        return sorted(set(lines))

    def shot(self, theme, page, name, caption, start):
        self.t.pump(1.5)
        fname = f'{slug(theme)}__{slug(page)}{"__" + name if name else ""}.png'
        self.t.screenshot(os.path.splitext(fname)[0], 0.5)
        checks = self.checks_since(start)
        self.shots.append((theme, page, fname, caption, checks))
        for c in checks:
            self.checks.append(f'{theme} > {page}{" > " + caption if caption else ""} : {c}')
        print(f'  {page}{" / " + caption if caption else ""}' + (f'  [{len(checks)} problème(s)]' if checks else ''))

    def back_to(self, title, tries=6):
        """Back to the menu \\p title: left wing, then its long press (games), until the menu is shown."""
        for k in range(tries):
            self.t.mark()
            self.t.keys('A' if k % 2 else 'a', 0.6)
            if self.t.expect(r'^ui: ' + re.escape(title) + '$', 2):
                return True
        return False


def main():
    parser = argparse.ArgumentParser(description='Screenshots of every screen of the badge, and the check of the texts')
    parser.add_argument('--port', default=None)
    parser.add_argument('--only', default=None, help='comma separated themes')
    args = parser.parse_args()
    port = args.port or find_port()
    os.makedirs(OUT, exist_ok=True)
    badge = Badge(port)
    badge.start()
    while not badge.connected():
        time.sleep(0.1)
    t = st.Tester(badge, OUT, False)
    c = Crawler(t)
    t.mark()
    t.keys('R')
    t.expect(r'^--- disconnected', 5)
    while not badge.connected():
        time.sleep(0.2)
    t.pump(4)
    if t.expect(r'^ui: Réglage radio$', 2):  # The first start tunes the radio
        t.expect(r'^tune: crystal', 25)
        t.pump(1)
        t.keys('a')
    t.pump(8)
    t.keys('U')  # Check of the texts
    t.expect(r'^uicheck: on', 3)
    start = len(t.lines)
    c.shot('Menu', 'Accueil', None, 'Le menu principal', start)
    only = args.only.split(',') if args.only else None
    themes = submenus()
    for ti, (theme, n) in enumerate(themes):
        if only and theme not in only:
            continue
        print(theme)
        if theme == 'Admin':
            if not st.open_admin(t):
                print('  pas de mode admin')
                continue
            t.mark()
            t.keys('b')
            if not c.ui():
                continue
        else:
            t.mark()
            t.keys('x' * ti + 'b')
            if c.ui() != theme:
                print(f'  thème {theme} non ouvert')
                t.keys('R')
                continue
        c.shot(theme, 'Menu', None, f'Le thème {theme}', len(t.lines))
        for i in range(n):
            if (theme, i) in NOT_PAGES:
                continue
            start = len(t.lines)
            t.mark()
            t.keys('x' * i + 'b', 0.4)
            page = c.ui(3)
            if not page or page == theme:
                print(f'  entrée {i}: pas de page')
                t.keys('y' * i)
                continue
            t.pump(LONG_PAGES.get(page, 0))
            c.shot(theme, page, None, None, start)
            here = page
            for keys, name, caption, back in PAGES.get(page, []):
                start = len(t.lines)
                t.mark()
                t.keys(keys, 0.5)
                if name:
                    c.shot(theme, page, name, caption, start)
                if back:
                    t.keys(back, 0.6)
            if not c.back_to(theme):
                print(f'  retour au thème {theme} impossible depuis {here}: redémarrage')
                t.mark()
                t.keys('R')
                t.expect(r'^--- disconnected', 5)
                while not badge.connected():
                    time.sleep(0.2)
                t.pump(12)
                t.keys('U')
                if theme == 'Admin':
                    st.open_admin(t)
                    t.keys('b')
                else:
                    t.keys('x' * ti + 'b')
                t.pump(2)
                continue
            t.keys('y' * i, 0.3)
        if theme == 'Admin':
            t.mark()
            t.keys('x' * (n - 1) + 'b')  # Quitter le mode admin
            t.expect(r'^admin: off', 3)
        else:
            st.close_theme(t, theme)
    t.keys('U')
    badge.stop()
    write_docs(c)


def write_docs(c):
    with open(os.path.join(OUT, 'checks.txt'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(c.checks) + '\n')
    for lang, path, title, intro, problems in (
            ('fr', os.path.join(ROOT, 'docs', 'fr', 'ecrans.md'), 'Badge SecSea — tous les écrans',
             'Généré par `tools/badge_screens.py` (captures du badge par le port série, mode utilisateur et admin).',
             'Textes à vérifier'),
            ('en', os.path.join(ROOT, 'docs', 'en', 'screens.md'), 'SecSea badge — every screen',
             'Generated by `tools/badge_screens.py` (screenshots of the badge through its serial port, user and admin '
             'modes). The texts of the badge are in French.', 'Texts to check')):
        lines = [f'# {title}', '', intro, '']
        current = None
        for theme, page, fname, caption, checks in c.shots:
            if theme != current:
                lines += ['', f'## {theme}', '']
                current = theme
            label = page if not caption else f'{page} — {caption}'
            lines += [f'### {label}', '', f'![{label}](../screens/{fname})', '']
            for ch in checks:
                lines.append(f'- ⚠ {ch}')
            if checks:
                lines.append('')
        lines += ['', f'## {problems}', '']
        lines += [f'- {ch}' for ch in c.checks] or ['(aucun)' if lang == 'fr' else '(none)']
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            f.write('\n'.join(lines) + '\n')
    print(f'{len(c.shots)} écrans, {len(c.checks)} texte(s) à vérifier')


if __name__ == '__main__':
    main()
