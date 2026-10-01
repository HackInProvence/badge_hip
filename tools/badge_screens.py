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
NOT_PAGES = {('Médias', 6), ('Radio & IR', 0), ('Badge', 3), ('Badge', 4), ('Badge', 5), ('Réglages', 1), ('Réglages', 2),
             ('Admin', 10), ('Admin', 12)}  # Démo écran (Badge, 5), Mode démo (Admin, 10): animations, not pages

# The pages inside an application: after it opened, the keys (a, b, x, y: short, A, B, X, Y: long press) and the
# screen to capture, with its caption. "back" at the end of a step goes back where the page was.
PAGES = {
    'Images': [('b', 'image', 'Une image de la carte SD', 'a')],
    # The SD card of the badge: the musics and the texts are in folders (the first rows), the videos at the root
    'Vidéos': [('b', 'lecture', 'Lecture d\'une vidéo', 'a')],
    'Musique': [('b', 'dossier', 'Un dossier de musiques', None), ('b', 'lecture', 'Lecture d\'une musique', 'a')],
    'Lecture rapide': [('b', 'dossier', 'Un dossier de textes', None),
                       ('b', 'lecture', 'Lecture d\'un texte', 'a')],
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
    'Annonces (admin)': [('b', 'detail', 'Une annonce : ses champs', None), ('b', 'heure', "Saisie de l'heure", 'a'),
                         ('xb', 'texte', 'Saisie du texte (lettres accentuées)', 'a'), ('xx', None, None, None),
                         ('xb', 'apercu', "Aperçu (l'écran des cigales)", 'a'), ('a', None, None, None)],
    'Remise à zéro': [('b', 'confirmation', 'La confirmation', 'a')],
    'Annonces': [],
    'Vote (admin)': [],
    'Type du badge': [],
    'Compétences': [('b', 'miennes', 'Mes compétences', 'a'), ('xb', 'partage', 'Qui les partage ?', 'a')],
    'Succès': [('b', 'detail', 'Comment obtenir un succès', 'a')],
    'Sonneries': [('b', 'lecture', 'Une sonnerie', 'a')],
    'Livres-jeux': [('b', 'livre', 'Un livre', None), ('b', 'lecture', 'La lecture', 'a')],
    'Contrebande': [('b', 'cale', 'La cale', 'a')],
    'Loup-garou': [('b', 'meneur', 'Mener une partie', 'a')],
    'Radio pirate': [('x', 'source', 'Les réglages', None)],
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

    def restart():
        """A restarted badge, on the main menu, with the check of the texts"""
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
        t.mark()
        t.keys('U')
        t.expect(r'^uicheck: on', 3)

    def enter(ti, theme):
        """Restarted, then the theme open"""
        restart()
        if theme == 'Admin':
            return st.open_admin(t)  # It opens the Admin theme
        t.mark()
        t.keys('x' * ti + 'b')
        return c.ui() == theme

    restart()
    c.shot('Menu', 'Accueil', None, 'Le menu principal', len(t.lines))
    only = args.only.split(',') if args.only else None
    for ti, (theme, n) in enumerate(submenus()):
        if only and theme not in only:
            continue
        print(theme)
        if not enter(ti, theme):
            print(f'  thème {theme} non ouvert')
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
                if not enter(ti, theme):
                    break
                continue
            t.pump(LONG_PAGES.get(page, 0))
            c.shot(theme, page, None, None, start)
            for keys, name, caption, back in PAGES.get(page, []):
                start = len(t.lines)
                t.mark()
                t.keys(keys, 0.5)
                if name:
                    c.shot(theme, page, name, caption, start)
                if back:
                    t.keys(back, 0.6)
            if c.back_to(theme):
                t.keys('y' * i, 0.3)
            elif not enter(ti, theme):  # Lost: from a restarted badge
                print(f'  impossible de revenir au thème {theme} après {page}')
                print('   ', [l for l in t.lines[-25:] if not l.startswith('buttons')])
                break
        if theme == 'Admin':
            t.mark()
            t.badge.send('\x01a')
            t.expect(r'^admin: off', 3)
    t.keys('U')
    badge.stop()
    write_docs(c)


def write_docs(c):
    # The rows of the lists cut with "..." are previews (the page of the row shows it all): listed apart
    problems = [ch for ch in c.checks if ': list row cut ' not in ch]
    rows = [ch for ch in c.checks if ': list row cut ' in ch]
    with open(os.path.join(OUT, 'checks.txt'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(problems + [''] + ['(list rows cut, previews)'] + rows) + '\n')
    for lang, path, title, intro, t_problems, t_rows in (
            ('fr', os.path.join(ROOT, 'docs', 'fr', 'ecrans.md'), 'Badge SecSea — tous les écrans',
             'Généré par `tools/badge_screens.py` (captures du badge par le port série, mode utilisateur et admin).',
             'Textes à corriger', 'Lignes de listes raccourcies (aperçus : la page de la ligne montre tout)'),
            ('en', os.path.join(ROOT, 'docs', 'en', 'screens.md'), 'SecSea badge — every screen',
             'Generated by `tools/badge_screens.py` (screenshots of the badge through its serial port, user and admin '
             'modes). The texts of the badge are in French.', 'Texts to fix',
             'Rows of lists shortened (previews: the page of the row shows it all)')):
        lines = [f'# {title}', '', intro, '']
        current = None
        for theme, page, fname, caption, checks in c.shots:
            if theme != current:
                lines += ['', f'## {theme}', '']
                current = theme
            label = page if not caption else f'{page} — {caption}'
            lines += [f'### {label}', '', f'![{label}](../screens/{fname})', '']
            for ch in checks:
                if not ch.startswith('list row cut'):
                    lines.append(f'- ⚠ {ch}')
            if checks:
                lines.append('')
        lines += ['', f'## {t_problems}', '']
        lines += [f'- {ch}' for ch in problems] or ['(aucun)' if lang == 'fr' else '(none)']
        lines += ['', f'## {t_rows}', '']
        lines += [f'- {ch}' for ch in rows] or ['(aucune)' if lang == 'fr' else '(none)']
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            f.write('\n'.join(lines) + '\n')
    print(f'{len(c.shots)} écrans, {len(problems)} texte(s) à corriger, {len(rows)} ligne(s) de liste raccourcie(s)')


if __name__ == '__main__':
    main()
