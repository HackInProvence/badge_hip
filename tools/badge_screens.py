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
- it saves a PNG of each screen in docs/screens/ and writes docs/fr/ecrans.md (--lang en: in docs/screens/en/,
  and docs/en/screens.md, the badge set to English during the capture, then back to its language);
- with the check of the texts of the firmware (key U), it lists the texts cut ("..."), too wide or drawn under
  the footer: docs/screens/checks.txt, and at the end of the pages.

Usage: python tools/badge_screens.py [--port COM9] [--only Jeux,Social] [--lang en]
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
NOT_PAGES = {('Médias', 6), ('Radio & IR', 0), ('Badge', 3), ('Badge', 4), ('Badge', 5), ('Réglages', 2), ('Réglages', 3),
             ('Admin', 12), ('Admin', 17), ('Admin', 18)}  # Démo écran (Badge, 5), Mode démo (Admin, 12): animations, not pages

def slug(text):
    t = text.lower()
    for a, b in (('é', 'e'), ('è', 'e'), ('ê', 'e'), ('à', 'a'), ('ç', 'c'), ('ô', 'o'), ('î', 'i'), ('œ', 'oe')):
        t = t.replace(a, b)
    return re.sub(r'[^a-z0-9]+', '_', t).strip('_')


# ------ The pages that list everything: every role of the werewolf, every achievement and rank, every good of the
# smuggler (their names read in the sources, so that a new one is captured too). In the keys, '.' waits 1 s.

def source_names(path, pattern):
    src = open(os.path.join(ROOT, 'src', 'menu', path), encoding='utf-8').read()
    return re.findall(pattern, src)


def werewolf_steps():
    roles = source_names('werewolf_cards.c', r'\[WW_[A-Z_]+\] = \{N_\("([^"]+)"\)')
    steps = [('b', 'meneur', 'Mener une partie', 'a'), ('xxb', 'aide', 'Aide : une carte de rôle', None),
             ('b', 'aide_detail', 'Aide : le détail du rôle', 'b')]
    for role in roles[1:]:  # The first one (the villager) just above
        steps += [('x', 'role_' + slug(role), 'Rôle : ' + role, None),
                  ('b', 'role_' + slug(role) + '_detail', 'Rôle : ' + role + ' : détail', 'b')]
    return steps


def ranks(c, theme, page):
    """Succès > Les grades: from the rank 1 to the last one, whatever the rank of this badge"""
    t = c.t
    t.mark()
    t.keys('x', 0.5)  # The row after the last achievement: the ranks
    t.mark()
    t.keys('b', 0.5)
    m = t.expect(r'^achv: rank (\d+) shown', 3)
    level = int(m.group(1)) if m else 1
    t.keys('x' * ((10 - level + 1) % 10), 0.4)  # Rank 1
    for r in range(1, 11):
        start = len(t.lines)
        c.shot(theme, page, f'grade_{r}', f'Grade {r}', start)
        t.keys('x', 0.4)
    t.keys('a', 0.6)


def reflexes(c, theme, page):
    """Réflexes: the wait, then the signal is the LEDs only (2 s) and the result shows 1.5 s: follow the log"""
    t = c.t
    t.mark()
    t.keys('b', 0.3)
    c.shot(theme, page, 'jeu', 'La partie : attendre le signal (les LEDs)', len(t.lines), settle=0.5)
    if t.expect(r'^game: reflex go$', 8):
        t.keys('b', 0)
        start = len(t.lines)
        if t.expect(r'^game: reflex \d+ ms$', 3):
            c.shot(theme, page, 'resultat', 'Le temps de réaction', start, settle=0.4)


def achievements_steps():
    names = source_names('achievements.c', r'\[ACHV_(?!LEVELS)[A-Z0-9_]+\] = \{N_\("([^"]+)"\)')
    steps = [('b', 'rencontres', 'Les rencontres', 'b')]
    for i, name in enumerate(names):
        steps += [('x', None, None, None), ('b', f'succes_{i + 1:02d}', 'Succès : ' + name, 'b')]
    steps.append((ranks, None, None, None))
    return steps


def smuggler_steps():
    goods = source_names('smuggler_goods.c', r'\{N_\("([^"]+)"\), RARITY_')
    return [('x', f'bien_{i + 2:02d}', 'Marchandise : ' + g, None) for i, g in enumerate(goods[1:])]


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
    'Réflexes': [(reflexes, None, None, None)],
    'Snake': [('b', None, None, None),  # The game itself: almost the page of the game, not captured
              ('............', 'fin', 'Fin de partie (dans le mur)', None),
              ('x', 'score', 'Le score en QR code (signé)', 'b')],
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
    'Radar des cigales': [],
    'Virus des cigales': [],
    'Choeur': [],
    'Décodeur 433 MHz': [],
    'Station météo': [],
    'Envoyer une image': [],
    'Recevoir une image': [],
    'Infrarouge': [],
    'Chasse 433 MHz': [],
    'Réseau cigales': [('b', 'nom', 'Changer le nom de la cigale (B)', 'a')],
    'Lampe': [('b', 'eteinte', 'La lampe éteinte (B)', 'b')],  # Not -/+: the level is kept, it may be at an end
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
    'Succès': achievements_steps(),
    'Sonneries': [('b', 'lecture', 'Une sonnerie', 'a')],
    'Livres-jeux': [('b', 'livre', 'Un livre', None), ('b', 'lecture', 'La lecture', 'a')],
    'Contrebande': [('b', 'cale', 'La cale', 'a')],
    'Loup-garou': werewolf_steps(),
    'Radio pirate': [('x', 'source', 'Les réglages', None)],
    'Blind test': [('b', 'morceau', 'Un morceau : deviner le titre', None), ('b', 'reponse', 'La réponse', 'b')],
    'Contrebande (admin)': smuggler_steps(),
    'Mini-bolides': [('b', 'salle', "Créer une partie : la salle d'attente", 'a'),
                     ('xb', 'recherche', 'Rejoindre une partie : la recherche', 'a')],
}

STILL_PAGES = {'Badge nominatif', 'Mon grade'}  # Shown like the screensaver (several seconds of cleaning first)
LONG_PAGES = {'Réglage radio': 20}  # Pages that work a while before their result (seconds)





def submenus():
    """[(theme, number of entries)] from src/menu/main.c"""
    src = open(os.path.join(ROOT, 'src', 'menu', 'main.c'), encoding='utf-8').read()
    return [(m.group(1), int(m.group(2))) for m in re.finditer(r'\{(?:N_\()?"([^"]+)"\)?, (\d+), \{M_', src)]


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

    def shot(self, theme, page, name, caption, start, settle=1.5):
        self.t.pump(settle)
        # A page made to stay is cleaned (black, white, then the page): wait for the page, not a step of the cleaning
        if page in STILL_PAGES and not any(l.startswith('saver: still page') for l in self.t.lines[start:]):
            self.t.expect(r'^saver: still page', 20)
            self.t.pump(1)
        if any(l.startswith('display: cleaning the page') for l in self.t.lines[start:]):
            self.t.expect(r'^display: (page cleaned|cleaning stopped)', 15)
            self.t.pump(0.5)
        fname = f'{slug(theme)}_{slug(page)}{"_" + name if name else ""}.png'
        self.t.screenshot(os.path.splitext(fname)[0], 0.5)
        checks = self.checks_since(start)
        self.shots.append((theme, page, fname, caption, checks))
        for c in checks:
            self.checks.append(f'{theme} > {page}{" > " + caption if caption else ""} : {c}')
        print(f'  {page}{" / " + caption if caption else ""}' + (f'  [{len(checks)} problème(s)]' if checks else ''))

    def back_to(self, title, tries=6):
        """Back to the menu \\p title: button A, then its long press (games), until the menu is shown."""
        for k in range(tries):
            self.t.mark()
            self.t.keys('A' if k % 2 else 'a', 0.6)
            if self.t.expect(r'^ui: ' + re.escape(title) + '$', 2):
                return True
        return False


def badge_lang(t, code):
    """The language of a badge: 'N' (next language) until it is \\p code; returns the one it had (None: unknown)"""
    had = [l.split()[2] for l in t.lines if l.startswith('i18n: language ')]
    if had and had[-1] == code:
        return code
    for _ in range(12):
        t.mark()
        t.keys('N')
        m = t.expect(r'^i18n: language (\S+)', 3)
        if not m:
            return None
        if m.group(1) == code:
            t.expect(r'^store: saved', 15)
            return had[-1] if had else 'fr'
    return None


def vote_screens(c, admin_ports, voter_ports, lang='fr'):
    """Social > Vote with two other badges (their ports): the admin badge (the one of the capture) opens a question,
    each voter answers another answer: the screen of a voter, then the histogram of the admin badge"""
    t = c.t
    voters = []
    for i, port in enumerate(voter_ports):
        b = Badge(port)
        b.start()
        while not b.connected():
            time.sleep(0.1)
        vt = st.Tester(b, t.out_dir, False)
        vt.mark()
        vt.keys('R')
        vt.expect(r'^--- disconnected', 5)
        while not b.connected():
            time.sleep(0.2)
        vt.pump(8)
        vt.lang_before = badge_lang(vt, lang)  # The voters in the language of the capture, given back at the end
        vt.keys('[')
        vt.keys('x' * st.THEMES.index('Social') + 'b' + 'x' * 4 + 'b', 0.4)  # Social > Vote
        voters.append(vt)
    if not st.open_admin(t):
        print('  vote: Admin theme not opened')
        return
    start = len(t.lines)
    t.keys('xxxb', 0.5)  # Admin > Vote (admin)
    t.mark()
    t.keys('b', 0.5)  # Opens the first question
    for i, vt in enumerate(voters):
        vt.expect(r'^vote: question', 10)
        vt.keys('x' * i + 'b', 0.5)  # Each one another answer
    vstart = len(voters[0].lines)
    voters[0].pump(3)
    path = voters[0].screenshot('social_vote_question', 1)
    if path:
        c.shots.append(('Social', 'Vote', 'social_vote_question.png',
                        "Une cigale qui vote (l'autre a choisi une autre réponse)", []))
    t.expect(r'^vote: %d voter' % len(voters), 20)
    c.shot('Admin', 'Vote (admin)', 'resultat', 'Le résultat des deux cigales qui votent', start)
    t.keys('b', 0.5)  # Closes the question
    for vt in voters:
        vt.keys('a', 0.5)
        if vt.lang_before and vt.lang_before != lang:
            badge_lang(vt, vt.lang_before)
        vt.badge.stop()
    del vstart


MULTI = ['Duel', 'Bataille navale', 'Loup-garou', 'Assassin', 'Tir à la corde', 'Mini-bolides']  # main.c


def partner_badge(port, lang, out_dir):
    """The other badge of a game, restarted, in the language of the capture"""
    b = Badge(port)
    b.start()
    while not b.connected():
        time.sleep(0.1)
    pt = st.Tester(b, out_dir, False)
    pt.mark()
    pt.keys('R')
    pt.expect(r'^--- disconnected', 5)
    while not b.connected():
        time.sleep(0.2)
    pt.pump(8)
    pt.lang_before = badge_lang(pt, lang)
    return pt


def open_multi(t, game):
    """From the main menu: Jeux multi, then the game"""
    t.mark()
    t.keys('x' * st.THEMES.index('Jeux multi') + 'b', 0.4)
    t.keys('x' * MULTI.index(game) + 'b', 0.5)


def join_party(pt, prefix, timeout=20):
    """The partner: Rejoindre une partie, then the first party heard"""
    pt.keys('xb', 0.5)
    deadline = time.time() + timeout
    while time.time() < deadline:
        pt.mark()
        pt.keys('b', 0.5)
        if pt.expect(rf'^{prefix}: joining', 1.5):
            return pt.expect(r'^party: joined', 10) is not None
        pt.pump(1)
    return False


def badge_name(t):
    """The name of a badge (its "!" diagnostic)"""
    t.mark()
    t.keys('!')
    m = t.expect(r'^social: \w+, ([^,]+),', 4)
    return m.group(1) if m else None


def neighbour_index(t, name):
    """The row of the cicada name in the cicadas near the badge (the "!" diagnostic: the order of the lists of the games)"""
    t.mark()
    t.keys('!')
    t.pump(1.5)
    rows = [re.match(r'^  (.+?) -?\d+ dBm', l) for l in t.lines[t.cursor:]]
    names = [m.group(1) for m in rows if m]
    return names.index(name) if name in names else None


def invite(t, pt, prefix, timeout=30):
    """Duel, naval battle: the host invites the partner (its row in the list of the cicadas near), it accepts"""
    partner = badge_name(pt)
    pt.mark()  # Before the invitation: it comes at once
    deadline = time.time() + timeout
    while time.time() < deadline:
        i = neighbour_index(t, partner)
        if i is None:
            t.pump(2)  # The beacons of the partner, heard soon
            continue
        t.mark()
        t.keys('x' * i + 'b', 0.5)
        m = t.expect(rf'^{prefix}: inviting (.+)$', 2)
        if m and m.group(1).startswith(partner):
            break
        print(f'    invited {m.group(1) if m else "nobody"}, not {partner}')
        return False
    else:
        return False
    deadline = time.time() + 10
    while time.time() < deadline:
        if pt.expect(rf'^{prefix}: invited by', 1):
            pt.keys('b', 0.5)
            return pt.expect(rf'^{prefix}: accepted', 5) is not None
    return False


def multi_screens(c, port, lang, restart):
    """A game in progress for the multiplayer games, with another badge (\\p port)"""
    t = c.t
    games = [('Tir à la corde', tug_game), ('Mini-bolides', race_game), ('Duel', duel_game),
             ('Bataille navale', battle_game)]
    pt = None
    for game, run in games:
        print(f'  {game} (2 badges)')
        restart()
        if pt is None:
            pt = partner_badge(port, lang, t.out_dir)
        else:
            pt.mark()
            pt.keys('R')
            pt.expect(r'^--- disconnected', 5)
            while not pt.badge.connected():
                time.sleep(0.2)
            pt.pump(8)
        open_multi(t, game)
        open_multi(pt, game)
        try:
            run(c, t, pt, game)
        except Exception as e:  # A game that went wrong: the next one from restarted badges
            print(f'    {game}: {type(e).__name__}: {e}')
    if pt is not None:
        if pt.lang_before and pt.lang_before != lang:
            badge_lang(pt, pt.lang_before)
        pt.mark()
        pt.keys('R')
        pt.badge.stop()


def tug_game(c, t, pt, game):
    t.mark()
    t.keys('b', 0.5)  # Créer une partie
    if not join_party(pt, 'tug'):
        print('    no party joined')
        return
    t.expect(r'^party: .* joined', 10)
    t.mark()
    t.keys('b', 0.5)  # Lancer
    if not t.expect(r'^tug: pull!$', 20):
        print('    no start')
        return
    end = time.time() + 1.5
    while time.time() < end:  # The partner pulls
        pt.keys('ab', 0.08)
    start = len(t.lines)
    c.shot('Jeux multi', game, 'partie', 'Une partie : la corde pendant que les équipes tirent', start, settle=0.3)
    end = time.time() + 20
    while time.time() < end and not any('tug: result' in l for l in t.lines[start:]):
        pt.keys('ab', 0.08)
        t.pump()
    t.expect(r'^tug: result', 10)
    c.shot('Jeux multi', game, 'resultat', 'Le résultat de la partie', len(t.lines))


def race_game(c, t, pt, game):
    t.mark()
    t.keys('b', 0.5)
    if not join_party(pt, 'race'):
        print('    no party joined')
        return
    t.expect(r'^party: .* joined', 10)
    t.mark()
    t.keys('b', 0.5)
    if not t.expect(r'^race: \d+ cars', 15):
        print('    no start')
        return
    t.pump(7)  # The grid and the countdown
    for _ in range(6):  # A few gas presses: the car leaves the grid
        t.keys('b', 0.25)
    c.shot('Jeux multi', game, 'partie', 'Une course : sa voiture (encadrée), l\'autre, les points de passage',
           len(t.lines), settle=0.5)


def duel_game(c, t, pt, game):
    if not invite(t, pt, 'duel'):
        print('    no duel')
        return
    t.pump(1)
    c.shot('Jeux multi', game, 'partie', 'Un duel : choisir pierre, feuille ou ciseaux', len(t.lines))
    t.mark()
    t.keys('b', 0.5)
    pt.keys('xb', 0.5)
    if t.expect(r'^duel: round', 10):
        c.shot('Jeux multi', game, 'manche', 'Le résultat de la manche', len(t.lines), settle=0.5)


def battle_game(c, t, pt, game):
    if not invite(t, pt, 'battle'):
        print('    no battle')
        return
    t.pump(3)
    c.shot('Jeux multi', game, 'partie', 'Une bataille : la grille de tir', len(t.lines))


def check_shots(c, out):
    """Every screenshot: there, not empty (the page below its title has something), not twice the same image"""
    import hashlib
    try:
        from PIL import Image
    except ImportError:
        print('(pip install pillow to check the screenshots)')
        return []
    problems, seen = [], {}
    for theme, page, fname, caption, _ in c.shots:
        path = os.path.join(out, fname)
        if not os.path.exists(path):
            problems.append(f'{fname}: missing')
            continue
        im = Image.open(path).convert('L')
        body = im.crop((0, im.height * 15 // 100, im.width, im.height * 88 // 100))
        if sum(1 for v in body.getdata() if v < 128) < body.width * body.height // 500:
            problems.append(f'{fname}: empty')
        seen.setdefault(hashlib.md5(im.tobytes()).hexdigest(), []).append(fname)
    problems += ['the same image: ' + ', '.join(v) for v in seen.values() if len(v) > 1]
    for p in problems:
        print('  ⚠ ' + p)
    return problems


def main():
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')  # The accents and the ⚠ of the report
    parser = argparse.ArgumentParser(description='Screenshots of every screen of the badge, and the check of the texts')
    parser.add_argument('--port', default=None)
    parser.add_argument('--only', default=None, help='comma separated themes (French names)')
    parser.add_argument('--lang', default='fr', help='language of the badge during the capture (fr, en...)')
    parser.add_argument('--multi', default=None, help='another badge (COM13): a game in progress for the '
                        'multiplayer games')
    parser.add_argument('--voters', default=None, help='two more badges (COM11,COM13): the screens of a vote')
    parser.add_argument('--out', default=None, help='folder of the screenshots and checks.txt, the docs not written '
                        '(to check the texts of a language)')
    args = parser.parse_args()
    port = args.port or find_port()
    out = args.out or (OUT if args.lang == 'fr' else os.path.join(OUT, args.lang))
    os.makedirs(out, exist_ok=True)
    badge = Badge(port)
    badge.start()
    while not badge.connected():
        time.sleep(0.1)
    t = st.Tester(badge, out, False)
    c = Crawler(t)

    def set_lang(code):
        """The language of the badge: 'N' (next language) until it is \\p code; returns the one it had"""
        # The language at the start (logged by the badge when it starts: "i18n: language fr (Français), ...")
        had = [l.split()[2] for l in t.lines if l.startswith('i18n: language ')]
        if had and had[-1] == code:
            return code
        for _ in range(12):
            t.mark()
            t.keys('N')
            m = t.expect(r'^i18n: language (\S+)', 3)
            if not m:
                sys.exit('the badge does not answer the key N: firmware without the translations?')
            if m.group(1) == code:
                t.expect(r'^store: saved', 15)  # Saved later: before a restart (or the end), it must be done
                return had[-1] if had else 'fr'
        sys.exit(f'language {code} not in the firmware')

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
    t.mark()
    t.keys('!')
    m = t.expect(r'admin (on|off)', 4)  # "remote: ..., admin on": given back at the end
    admin_was_on = bool(m and m.group(1) == 'on')
    before = set_lang(args.lang)
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
                if callable(keys):  # A step that captures by itself (the ranks)
                    keys(c, theme, page)
                    continue
                start = len(t.lines)
                t.mark()
                for k in keys:
                    if k == '.':
                        t.pump(1)  # '.': wait 1 s (the end of a game)
                    else:
                        t.keys(k, 0.5)
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
    if args.voters and (not only or 'Social' in only or 'Admin' in only):
        print('Vote')
        restart()
        n = len(c.shots)
        vote_screens(c, None, args.voters.split(','), args.lang)
        # Each new screen after the other screens of its page (the docs are grouped by page)
        for shot in c.shots[n:]:
            c.shots.remove(shot)
            last = max((k for k, s in enumerate(c.shots) if s[:2] == shot[:2]), default=len(c.shots) - 1)
            c.shots.insert(last + 1, shot)
    if args.multi and (not only or 'Jeux multi' in only):
        print('Jeux multi (2 badges)')
        n = len(c.shots)
        multi_screens(c, args.multi, args.lang, restart)
        for shot in c.shots[n:]:  # After the other screens of the same game
            c.shots.remove(shot)
            last = max((k for k, s in enumerate(c.shots) if s[:2] == shot[:2]), default=len(c.shots) - 1)
            c.shots.insert(last + 1, shot)
    t.keys('U')
    restore = before if before else 'fr'
    if restore != args.lang:
        set_lang(restore)  # The language the badge had
    t.mark()
    t.badge.send('\x01A' if admin_was_on else '\x01a')  # The admin mode the badge had
    t.expect(r'^admin: ', 3)
    badge.stop()
    problems = check_shots(c, out)
    if not only and not args.out:  # A whole run: the screenshots of older runs (other names, pages gone) removed
        keep = {s[2] for s in c.shots}
        old = [f for f in os.listdir(out) if f.endswith('.png') and f not in keep]
        for f in old:
            os.remove(os.path.join(out, f))
        print(f'{len(old)} old screenshot(s) removed')
    write_docs(c, args.lang, out, docs=not args.out)
    if problems:
        print(f'{len(problems)} screenshot(s) to look at (above)')


def english_names():
    """French text -> English, from src/menu/lang/en.po (the titles of the pages in docs/en/screens.md)"""
    import i18n
    path = os.path.join(i18n.LANG_DIR, 'en.po')
    if not os.path.exists(path):
        return {}
    _, entries = i18n.read_po(path)
    return {e['id']: e['str'] for e in entries if e['str'] and not e['obsolete']}


# The captions of PAGES and of the themes, in English (docs/en/screens.md)
CAPTIONS_EN = {
    'Le menu principal': 'The main menu', 'Une image de la carte SD': 'An image of the SD card',
    'Lecture d\'une vidéo': 'Playing a video', 'Un dossier de musiques': 'A folder of music',
    'Lecture d\'une musique': 'Playing a music', 'Un dossier de textes': 'A folder of texts',
    'Lecture d\'un texte': 'Reading a text', 'La partie': 'The game', 'Saisie d\'un code': 'Typing a code',
    'Un défi': 'A challenge', 'Son indice': 'Its hint', 'La saisie de la réponse': 'Typing the answer',
    'Le flag final': 'The final flag', 'Choix du destinataire': 'Choosing the recipient',
    'Choix du message': 'Choosing the message', 'Ma carte (champs cochés : envoyés)': 'My card (checked fields: sent)',
    'Saisie d\'un champ': 'Typing a field', 'Echange des cartes': 'Exchanging the cards',
    'Contacts reçus': 'Contacts received', 'Un talk (et son QR code)': 'A talk (and its QR code)',
    'Plus fort': 'Brighter', 'Vert : tout va bien': 'Green: all is well', 'Orange : 5 min': 'Orange: 5 min',
    'Rouge : fini': 'Red: time is up', 'Le mode': 'The mode', 'Les temps du clignotement': 'The blink times',
    'Les temps du fondu': 'The fade times', 'Envoyer / rétablir': 'Send / restore',
    'Une annonce : ses champs': 'An announcement: its fields', "Saisie de l'heure": 'Typing the time',
    'Saisie du texte (lettres accentuées)': 'Typing the text (accented letters)',
    "Aperçu (l'écran des cigales)": 'Preview (the screen of the cicadas)', 'La confirmation': 'The confirmation',
    'Mes compétences': 'My skills', 'Qui les partage ?': 'Who shares them?',
    'Comment obtenir un succès': 'How to get an achievement', 'Une sonnerie': 'A ringtone', 'Un livre': 'A book',
    'La lecture': 'Reading', 'La cale': 'The hold', 'Mener une partie': 'Leading a game',
    'Aide : une carte de rôle': 'Help: a role card', 'Aide : le détail du rôle': 'Help: the role in detail',
    'Les réglages': 'The settings', 'Accueil': 'Home', 'Menu': 'Menu',
    'Page 2': 'Page 2', 'Page 3': 'Page 3', 'Page 4': 'Page 4', 'Page 5': 'Page 5', 'Page 6': 'Page 6',
    'Page 7': 'Page 7', 'Page 8': 'Page 8',
    'Rôle': 'Role', 'détail': 'details', 'Succès': 'Achievement', 'Marchandise': 'Goods',
    'Les rencontres': 'The meetings', 'Fin de partie (dans le mur)': 'Game over (into the wall)',
    'Le score en QR code (signé)': 'The score as a QR code (signed)',
    'La partie : attendre le signal': 'The game: wait for the signal', 'Le signal : appuyer !': 'The signal: press!',
    'Le temps de réaction': 'The reaction time',
    'La partie : attendre le signal (les LEDs)': 'The game: wait for the signal (the LEDs)', 'Moins fort': 'Dimmer',
    'Une partie : la corde pendant que les équipes tirent': 'A game: the rope while the teams pull',
    'Le résultat de la partie': 'The result of the game',
    "Une course : sa voiture (encadrée), l'autre, les points de passage":
        'A race: its car (framed), the other one, the checkpoints',
    'Un duel : choisir pierre, feuille ou ciseaux': 'A duel: choose rock, paper or scissors',
    'Le résultat de la manche': 'The result of the round', 'Une bataille : la grille de tir': 'A battle: the grid of the shots', 'Changer le nom de la cigale (B)': 'Change the name of the cicada (B)', 'La lampe éteinte (B)': 'The lamp off (B)',
    'Un morceau : deviner le titre': 'A track: guess the title', 'La réponse': 'The answer',
    "Une cigale qui vote (l'autre a choisi une autre réponse)": 'A cicada voting (the other one chose another answer)',
    'Le résultat des deux cigales qui votent': 'The result of the two cicadas voting', "L'aide pendant la partie": 'The help during the game',
    "Créer une partie : la salle d'attente": 'Create a party: the waiting room',
    'Rejoindre une partie : la recherche': 'Join a party: the search',
    **{f'Grade {r}': f'Rank {r}' for r in range(1, 11)},
}


def write_docs(c, lang_run, out, docs=True):
    # The rows of the lists cut with "..." are previews (the page of the row shows it all): listed apart
    problems = [ch for ch in c.checks if ': list row cut ' not in ch]
    rows = [ch for ch in c.checks if ': list row cut ' in ch]
    with open(os.path.join(out, 'checks.txt'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(problems + [''] + ['(list rows cut, previews)'] + rows) + '\n')
    en = english_names() if lang_run == 'en' else {}

    def name(text):
        if lang_run != 'en':
            return text
        if text.startswith('Le thème '):
            return 'The theme ' + en.get(text[len('Le thème '):], text[len('Le thème '):])
        if text in CAPTIONS_EN or text in en:
            return CAPTIONS_EN.get(text) or en[text]
        # "Rôle : Voyante : détail": each part (the names come from the translations of the firmware)
        return ': '.join(CAPTIONS_EN.get(p) or en.get(p) or p for p in text.split(' : '))

    shots_dir = '../screens/' if lang_run == 'fr' else f'../screens/{lang_run}/'
    for lang, path, title, intro, t_problems, t_rows in (
            ('fr', os.path.join(ROOT, 'docs', 'fr', 'ecrans.md'), 'Badge SecSea — tous les écrans',
             'Généré par `tools/badge_screens.py` (captures du badge par le port série, mode utilisateur et admin).',
             'Textes à corriger', 'Lignes de listes raccourcies (aperçus : la page de la ligne montre tout)'),
            ('en', os.path.join(ROOT, 'docs', 'en', 'screens.md'), 'SecSea badge — every screen',
             'Generated by `tools/badge_screens.py --lang en` (screenshots of the badge in English through its serial '
             'port, user and admin modes). The language is chosen in Settings > Language '
             '([translation](translation.md)).', 'Texts to fix',
             'Rows of lists shortened (previews: the page of the row shows it all)')):
        if lang != lang_run or not docs:
            continue
        lines = [f'# {title}', '', intro, '']
        if lang == 'fr':
            lines += ['*English version: [every screen](../en/screens.md).*', '']
        else:
            lines += ['*Version française : [tous les écrans](../fr/ecrans.md).*', '']
        current = None
        for theme, page, fname, caption, checks in c.shots:
            if theme != current:
                lines += ['', f'## {name(theme)}', '']
                current = theme
            label = name(page) if not caption else f'{name(page)} — {name(caption)}'
            lines += [f'### {label}', '', f'![{label}]({shots_dir}{fname})', '']
            for ch in checks:
                if not ch.startswith('list row cut'):
                    lines.append(f'- ⚠ {ch}')
            if checks:
                lines.append('')
        # The texts to fix and the rows cut are in checks.txt (for the developers), not in the page of the users
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            f.write('\n'.join(lines) + '\n')
    print(f'{len(c.shots)} écrans, {len(problems)} texte(s) à corriger, {len(rows)} ligne(s) de liste raccourcie(s)')


if __name__ == '__main__':
    main()
