"""The web site of the badge: its documentation (French, English) in HTML, a home page, and its resources.

Every .md of docs/fr, docs/en and docs/ (the design notes) is converted to HTML, with a menu of the pages, a link to
the same page in the other language (the line "*English version: [...](../en/x.md).*" of each page), and the images
they show. The resources go in the ressources/ sub-folder, with their own page in each language: the firmware
(.uf2, when built), the content of the SD card (docs/sd, also as a .zip), the PC tools useful to a user
(USER_TOOLS: remote control, SD files, contacts, media conversions...; not the ones of the development), the
Flipper Zero files, the hardware (schematic, pictures, KiCad project as a .zip), the pictures. Links to other files of the
repository (source code) point to the repository on the web. docs/analyses (local notes) is never published.

    python tools/docs_site.py                    (writes build/docs_site/)
    python tools/docs_site.py --out D:/www/badge --repo https://github.com/HackInProvence/badge_hip

    site/
      index.html          the badge, links to everything
      fr/ en/ notes/      the pages
      ressources/         the files (same paths as in the repository), carte_sd.zip, firmware/...
      style.css

The folder is self-contained: copy it on any web server (or open index.html in a browser). Requires the Python
package "markdown" (pip install markdown).
"""

import argparse
import ast
import html
import os
import posixpath
import re
import shutil
import sys
import zipfile

try:
    import markdown
except ImportError:
    sys.exit('the Python package "markdown" is needed: pip install markdown')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UF2 = 'build/src/menu/badge_menu.uf2'
RES = 'ressources'
VERSION = open(os.path.join(ROOT, 'VERSION'), encoding='utf-8').read().strip()  # Of the firmware
LANGS = {
    'fr': {'name': 'Français', 'entries': ['guide_utilisateur.md', 'guide_avance.md', 'guide_developpeur.md'],
           'title': 'Badge SecSea', 'guides': 'Guides', 'pages': 'Fonctions', 'notes': 'Notes de conception',
           'res': 'Ressources', 'res_page': 'ressources.html', 'home': 'Accueil'},
    'en': {'name': 'English', 'entries': ['user_guide.md', 'advanced_guide.md', 'developer_guide.md'],
           'title': 'SecSea badge', 'guides': 'Guides', 'pages': 'Features', 'notes': 'Design notes (French)',
           'res': 'Resources', 'res_page': 'resources.html', 'home': 'Home'},
}
NOTES = 'notes'  # The output folder of docs/*.md (French)
# Published as files (same path under ressources/); docs/analyses never
PUBLISHED = ('docs/screens/', 'docs/images/', 'docs/color_reveal/', 'docs/sd/', 'imgs/', 'tools/flipper/',
             'hardware/')
NOT_PUBLISHED = ('docs/analyses/', 'hardware/kicad/')
# The scripts useful to a user of the badge (not the ones of the development: tests, files of the firmware...), and
# the modules they need; published at their path, and together (flat) in outils.zip
USER_TOOLS = ('tools/badge_remote.py', 'tools/sd_files.py', 'tools/contacts_export.py', 'src/images/image2epi.py',
              'src/video/video2epaper.py', 'src/audio/audio2wav.py', 'tools/rtttl_sort.py', 'tools/gamebook_check.py')
USER_TOOL_MODULES = ('tools/rtttl_lib.py',)
IMAGE_EXT = ('.png', '.jpg', '.jpeg', '.gif', '.svg', '.webp')
LINK = re.compile(r'(!?)\[([^\]]*)\]\(([^)\s]+)(\s+"[^"]*")?\)')
OTHER_VERSION = re.compile(r'^\*(English version|Version française)\s*:.*\*\s*$', re.M)
GALLERY = ['menu__accueil', 'social__reseau_cigales', 'jeux__menu', 'medias__livres_jeux__lecture',
           'radio_ir__station_meteo', 'badge__badge_de_talk', 'social__radar_des_cigales', 'jeux__snake__jeu']

CSS = """
:root { --bg: #fdfcf8; --fg: #1d1d1b; --muted: #6b6a64; --line: #e3e0d6; --accent: #8a5a00; --code: #f2efe6;
        --side: #f6f3ea; }
@media (prefers-color-scheme: dark) {
  :root { --bg: #17171a; --fg: #e8e6e1; --muted: #a09d94; --line: #33323a; --accent: #f0b54a; --code: #232329;
          --side: #1d1d22; }
}
* { box-sizing: border-box; }
body { margin: 0; background: var(--bg); color: var(--fg); font: 16px/1.6 system-ui, -apple-system, "Segoe UI",
       Roboto, sans-serif; }
a { color: var(--accent); }
header { display: flex; flex-wrap: wrap; align-items: center; gap: 6px 16px; padding: 10px 20px;
         border-bottom: 1px solid var(--line); position: sticky; top: 0; background: var(--bg); z-index: 2; }
header .site { font-weight: 700; color: var(--fg); text-decoration: none; }
header .links { margin-left: auto; display: flex; gap: 16px; }
.layout { display: flex; max-width: 1200px; margin: 0 auto; }
nav { width: 260px; flex: none; padding: 20px; border-right: 1px solid var(--line); background: var(--side);
      min-height: calc(100vh - 50px); }
nav h2 { font-size: 13px; text-transform: uppercase; letter-spacing: .05em; color: var(--muted); margin: 16px 0 8px; }
nav h2:first-child { margin-top: 0; }
nav ul { list-style: none; padding: 0; margin: 0; }
nav li a { display: block; padding: 3px 8px; border-radius: 6px; text-decoration: none; color: var(--fg);
           font-size: 15px; }
nav li a.current { background: var(--line); font-weight: 600; }
main { flex: 1; min-width: 0; padding: 20px 40px 60px; }
main img { max-width: 100%; image-rendering: pixelated; border: 1px solid var(--line); }
table { border-collapse: collapse; display: block; overflow-x: auto; margin: 1em 0; }
th, td { border: 1px solid var(--line); padding: 6px 10px; text-align: left; vertical-align: top; }
th { background: var(--side); }
td.size { white-space: nowrap; text-align: right; color: var(--muted); }
code { background: var(--code); padding: 1px 4px; border-radius: 4px; font-size: .92em; }
pre { background: var(--code); padding: 12px; border-radius: 8px; overflow-x: auto; }
pre code { padding: 0; background: none; }
h1, h2, h3 { line-height: 1.25; }
h2 { border-bottom: 1px solid var(--line); padding-bottom: 4px; margin-top: 2em; }
.home { max-width: 980px; margin: 0 auto; padding: 24px 20px 60px; }
.hero { display: flex; gap: 32px; align-items: center; flex-wrap: wrap; }
.hero img { width: 320px; max-width: 100%; border-radius: 12px; }
.hero div { flex: 1; min-width: 260px; }
.cards { display: grid; grid-template-columns: repeat(auto-fill, minmax(220px, 1fr)); gap: 12px; margin: 24px 0; }
.cards a { display: block; padding: 14px 16px; border: 1px solid var(--line); border-radius: 10px;
           text-decoration: none; color: var(--fg); background: var(--side); }
.cards a b { color: var(--accent); }
.gallery { display: grid; grid-template-columns: repeat(auto-fill, minmax(150px, 1fr)); gap: 10px; }
.gallery img { width: 100%; image-rendering: pixelated; border: 1px solid var(--line); background: #fff; }
@media (max-width: 800px) {
  .layout { display: block; }
  nav { width: auto; min-height: 0; border-right: none; border-bottom: 1px solid var(--line); }
  main { padding: 16px; }
  header .links { margin-left: 0; }
}
"""


def page_title(text, fallback):
    m = re.search(r'^#\s+(.+)$', text, re.M)
    return m.group(1).strip() if m else fallback


def size_text(n):
    for unit in ('o', 'Ko', 'Mo'):
        if n < 1024 or unit == 'Mo':
            return f'{n:.0f} {unit}' if unit == 'o' else f'{n:.1f} {unit}'
        n /= 1024


def published(rel):
    return rel in USER_TOOLS or rel in USER_TOOL_MODULES or \
        rel.startswith(PUBLISHED) and not rel.startswith(NOT_PUBLISHED) or \
        (rel.startswith('docs/') and rel.count('/') == 1 and rel.endswith('.html'))


def tool_summary(path):
    """The first sentence of the docstring of a Python tool"""
    try:
        doc = ast.get_docstring(ast.parse(open(path, encoding='utf-8').read())) or ''
    except (SyntaxError, UnicodeDecodeError):
        return ''
    first = doc.split('\n\n')[0].replace('\n', ' ').strip()
    return first


class Site:
    def __init__(self, out, repo, uf2):
        self.out = out
        self.repo = repo.rstrip('/')
        self.uf2 = uf2
        self.pages = {}  # repo path of the .md -> {'out', 'lang', 'title', 'text', 'other'}
        self.files = set()  # repo paths copied under ressources/

    # ------ What is published

    def collect(self):
        for folder, lang in (('docs/fr', 'fr'), ('docs/en', 'en'), ('docs', NOTES)):
            for name in sorted(os.listdir(os.path.join(ROOT, folder))):
                if not name.endswith('.md'):
                    continue
                rel = f'{folder}/{name}'
                text = open(os.path.join(ROOT, rel), encoding='utf-8').read()
                out = f'{lang}/{os.path.splitext(name)[0]}.html'
                self.pages[rel] = {'out': out, 'lang': lang, 'title': page_title(text, name), 'text': text,
                                   'other': None}
        for rel, p in self.pages.items():
            m = OTHER_VERSION.search(p['text'])
            link = LINK.search(m.group(0)) if m else None
            if link:
                target = posixpath.normpath(posixpath.join(posixpath.dirname(rel), link.group(3).split('#')[0]))
                if target in self.pages:
                    p['other'] = target
        for prefix in PUBLISHED:
            for dirpath, dirs, names in os.walk(os.path.join(ROOT, prefix)):
                for name in names:
                    rel = os.path.relpath(os.path.join(dirpath, name), ROOT).replace(os.sep, '/')
                    if published(rel):
                        self.files.add(rel)
        for name in os.listdir(os.path.join(ROOT, 'docs')):
            if published('docs/' + name):
                self.files.add('docs/' + name)
        self.files.update(r for r in USER_TOOLS + USER_TOOL_MODULES if os.path.exists(os.path.join(ROOT, r)))

    # ------ The pages

    def href(self, page_out, target_out):
        return posixpath.relpath(target_out, posixpath.dirname(page_out) or '.')

    def rewrite(self, rel, text):
        """The links of a page: .md -> .html, published files -> ressources/, the rest -> the repository online"""
        page_out = self.pages[rel]['out']

        def sub(m):
            bang, label, target, title = m.group(1), m.group(2), m.group(3), m.group(4) or ''
            if re.match(r'^[a-z]+:', target) or target.startswith('#'):
                return m.group(0)
            path, _, anchor = target.partition('#')
            anchor = '#' + anchor if anchor else ''
            res = posixpath.normpath(posixpath.join(posixpath.dirname(rel), path))
            if res.startswith('..'):
                return m.group(0)
            if res in self.pages:
                return f'{bang}[{label}]({self.href(page_out, self.pages[res]["out"])}{anchor}{title})'
            if res in self.files:
                return f'{bang}[{label}]({self.href(page_out, RES + "/" + res)}{title})'
            if os.path.isdir(os.path.join(ROOT, res)) and published(res + '/'):
                lang = self.pages[rel]['lang']
                lang = lang if lang in LANGS else 'fr'
                return f'{bang}[{label}]({self.href(page_out, lang + "/" + LANGS[lang]["res_page"])}' \
                       f'#{self.group_of(res)}{title})'
            kind = 'tree' if os.path.isdir(os.path.join(ROOT, res)) else 'blob'
            return f'{bang}[{label}]({self.repo}/{kind}/main/{res}{anchor}{title})'
        return LINK.sub(sub, text)

    @staticmethod
    def group_of(rel):
        if rel.startswith('docs/sd'):
            return 'sd'
        if rel.startswith('tools/flipper'):
            return 'flipper'
        if rel in USER_TOOLS or rel in USER_TOOL_MODULES:
            return 'tools'
        if rel.startswith('hardware'):
            return 'hardware'
        return 'images'

    def nav(self, lang, current_out):
        def items(rels):
            out = []
            for r in rels:
                p = self.pages[r]
                cls = ' class="current"' if p['out'] == current_out else ''
                short = re.sub(r'^(Badge SecSea|SecSea badge)\s*[—-]\s*', '', p['title'])
                short = short[:1].upper() + short[1:]
                out.append(f'<li><a href="{self.href(current_out, p["out"])}"{cls}>{html.escape(short)}</a></li>')
            return '\n'.join(out)
        info = LANGS[lang if lang in LANGS else 'fr']
        folder = f'docs/{lang}' if lang in LANGS else 'docs'
        mine = [r for r, p in self.pages.items() if p['lang'] == lang]
        parts = []
        if lang in LANGS:
            guides = [f'{folder}/{e}' for e in info['entries'] if f'{folder}/{e}' in self.pages]
            others = sorted((r for r in mine if r not in guides), key=lambda r: self.pages[r]['title'].lower())
            parts.append(f'<h2>{info["guides"]}</h2><ul>{items(guides)}</ul>')
            parts.append(f'<h2>{info["pages"]}</h2><ul>{items(others)}</ul>')
            res_out = f'{lang}/{info["res_page"]}'
            cls = ' class="current"' if res_out == current_out else ''
            parts.append(f'<h2>{info["res"]}</h2><ul><li><a href="{self.href(current_out, res_out)}"{cls}>'
                         f'{info["res"]}</a></li></ul>')
        else:
            notes = sorted(mine, key=lambda r: self.pages[r]['title'].lower())
            parts.append(f'<h2>{LANGS["fr"]["notes"]}</h2><ul>{items(notes)}</ul>')
        return '\n'.join(parts)

    def frame(self, lang, out, title, body, other=None):
        info = LANGS[lang if lang in LANGS else 'fr']
        links = []
        if other:
            ol, oout = other
            links.append(f'<a href="{self.href(out, oout)}">{LANGS[ol]["name"]}</a>')
        if lang == NOTES:
            links.append(f'<a href="{self.href(out, "fr/" + LANGS["fr"]["res_page"])}">{LANGS["fr"]["res"]}</a>')
        return f"""<!doctype html>
<html lang="{lang if lang in LANGS else 'fr'}">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{html.escape(title)} — {html.escape(info['title'])}</title>
<link rel="stylesheet" href="{self.href(out, 'style.css')}">
</head>
<body>
<header><a class="site" href="{self.href(out, 'index.html')}">{html.escape(info['title'])}</a>
<span class="links">{' '.join(links)}</span></header>
<div class="layout">
<nav>
{self.nav(lang, out)}
</nav>
<main>
{body}
</main>
</div>
</body>
</html>
"""

    def render(self, rel):
        p = self.pages[rel]
        text = OTHER_VERSION.sub('', p['text'])
        body = markdown.markdown(self.rewrite(rel, text),
                                 extensions=['tables', 'fenced_code', 'toc', 'sane_lists', 'attr_list'],
                                 extension_configs={'toc': {'permalink': False}})
        other = None
        if p['other']:
            q = self.pages[p['other']]
            other = (q['lang'], q['out'])
        return self.frame(p['lang'], p['out'], p['title'], body, other)

    # ------ The resources

    def file_rows(self, out, rels, describe=None):
        rows = []
        for r in sorted(rels):
            size = os.path.getsize(os.path.join(ROOT, r)) if os.path.exists(os.path.join(ROOT, r)) else \
                os.path.getsize(os.path.join(self.out, RES, r))
            desc = html.escape(describe(r)) if describe else ''
            rows.append(f'<tr><td><a href="{self.href(out, RES + "/" + r)}">{html.escape(r)}</a></td>'
                        f'<td>{desc}</td><td class="size">{size_text(size)}</td></tr>')
        return '<table><tr><th>{}</th><th>{}</th><th></th></tr>' + '\n'.join(rows) + '</table>'

    def resources(self, lang):
        fr = lang == 'fr'
        out = f'{lang}/{LANGS[lang]["res_page"]}'
        head = (lambda a, b: a if fr else b)
        file_col, desc_col = head('Fichier', 'File'), head('Description', 'Description')

        def table(rels, describe=None):
            return self.file_rows(out, rels, describe).format(file_col, desc_col)
        body = [f'<h1>{LANGS[lang]["res"]}</h1>',
                head('<p>Tous les fichiers du badge. Les chemins sont ceux du dépôt.</p>',
                     '<p>Every file of the badge. The paths are the ones of the repository.</p>')]
        body.append(f'<h2 id="firmware">{head("Firmware", "Firmware")}</h2>')
        if self.uf2_out:
            body.append(head(
                '<p>Badge éteint, maintenir le bouton BOOTSEL en branchant l\'USB, puis copier le fichier sur le '
                'disque <code>RPI-RP2</code>.</p>',
                '<p>Badge off, hold the BOOTSEL button while plugging the USB cable, then copy the file to the '
                '<code>RPI-RP2</code> drive.</p>'))
            body.append(table([self.uf2_out]))
        else:
            body.append(head('<p>Pas encore publié : à compiler (guide développeur).</p>',
                             '<p>Not published yet: build it (developer guide).</p>'))
        sd = [r for r in self.files if r.startswith('docs/sd/')]
        body.append(f'<h2 id="sd">{head("Carte SD", "SD card")}</h2>')
        body.append(head(f'<p>Le contenu d\'exemple de la carte SD : décompresser <a href="../{RES}/carte_sd.zip">'
                         f'carte_sd.zip</a> à la racine de la carte (FAT32).</p>',
                         f'<p>The example content of the SD card: unzip <a href="../{RES}/carte_sd.zip">'
                         f'carte_sd.zip</a> at the root of the card (FAT32).</p>'))
        body.append(table(sd))
        tools = [r for r in USER_TOOLS if r in self.files]
        body.append(f'<h2 id="tools">{head("Outils du PC", "PC tools")}</h2>')
        body.append(head(f'<p>Scripts Python 3 (le détail de chacun : <code>--help</code> et le guide avancé). '
                         f'Tous ensemble : <a href="../{RES}/outils.zip">outils.zip</a>.</p>',
                         f'<p>Python 3 scripts (each one: <code>--help</code> and the advanced guide). '
                         f'All of them: <a href="../{RES}/outils.zip">outils.zip</a>.</p>'))
        body.append(table(tools, lambda r: tool_summary(os.path.join(ROOT, r))))
        flipper = [r for r in self.files if r.startswith('tools/flipper/')]
        body.append(f'<h2 id="flipper">Flipper Zero</h2>')
        body.append(head('<p>Fichiers Sub-GHz (<code>.sub</code>) à copier dans <code>subghz/</code> du Flipper.</p>',
                         '<p>Sub-GHz files (<code>.sub</code>) to copy to <code>subghz/</code> on the Flipper.</p>'))
        body.append(table(flipper))
        hw = [r for r in self.files if r.startswith('hardware/')]
        body.append(f'<h2 id="hardware">{head("Matériel", "Hardware")}</h2>')
        body.append(head('<p>Schéma, vues de la carte, projet KiCad complet.</p>',
                         '<p>Schematic, views of the board, full KiCad project.</p>'))
        body.append(table(hw))
        imgs = [r for r in self.files if self.group_of(r) == 'images' and not r.startswith('docs/screens/')]
        body.append(f'<h2 id="images">{head("Images", "Pictures")}</h2>')
        body.append(table(imgs))
        return self.frame(lang, out, LANGS[lang]['res'], '\n'.join(body))

    def zip_dir(self, src, dst):
        with zipfile.ZipFile(dst, 'w', zipfile.ZIP_DEFLATED) as z:
            for dirpath, dirs, names in os.walk(src):
                dirs[:] = [d for d in dirs if d != '__pycache__']
                for name in names:
                    path = os.path.join(dirpath, name)
                    z.write(path, os.path.relpath(path, src))

    # ------ The home page

    def index(self):
        def page(lang, entry):
            rel = f'docs/{lang}/{entry}'
            return self.pages[rel]['out'] if rel in self.pages else None
        cards = []
        for lang, info in LANGS.items():
            for e in info['entries']:
                o = page(lang, e)
                if o:
                    cards.append(f'<a href="{o}"><b>{info["name"]}</b><br>'
                                 f'{html.escape(self.pages[f"docs/{lang}/{e}"]["title"])}</a>')
            cards.append(f'<a href="{lang}/{info["res_page"]}"><b>{info["name"]}</b><br>{info["res"]}</a>')
        notes = sorted((r for r, p in self.pages.items() if p['lang'] == NOTES),
                       key=lambda r: self.pages[r]['title'].lower())
        if notes:
            cards.append(f'<a href="{self.pages[notes[0]]["out"]}"><b>Français</b><br>'
                         f'{LANGS["fr"]["notes"]}</a>')
        gallery = ''.join(f'<img src="{RES}/docs/screens/{g}.png" alt="{g}">' for g in GALLERY
                          if f'docs/screens/{g}.png' in self.files)
        photo = f'<img src="{RES}/imgs/badge_front.webp" alt="Badge SecSea">' \
            if 'imgs/badge_front.webp' in self.files else ''
        return f"""<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Badge SecSea</title>
<meta name="description" content="La cigale, badge électronique de SecSea (Hack In Provence) : documentation et ressources">
<link rel="stylesheet" href="style.css">
</head>
<body>
<div class="home">
<div class="hero">
{photo}
<div>
<h1>Badge SecSea — la cigale</h1>
<p>Version {VERSION}</p>
<p>Le badge électronique de <a href="https://secsea.org/">SecSea</a>, la conférence de hacking de
Hack In Provence à La Ciotat. Un RP2040, un écran e-Paper 200 × 200, une radio CC1101 433 MHz, deux LEDs RGB, un
buzzer, quatre boutons, une carte micro-SD et deux ports d'extension. Les cigales se rencontrent par radio, jouent
ensemble, chantent en chœur, affichent le programme d'une conférence, et servent d'outil pédagogique de radio et
d'infrarouge.</p>
<p lang="en">The electronic badge of SecSea, the hacking conference of Hack In Provence in La Ciotat (France):
an RP2040, a 200 × 200 e-Paper screen, a CC1101 433 MHz radio, two RGB LEDs, a buzzer, four buttons, a micro-SD
card and two extension ports. The cicadas meet by radio, play together, sing in chorus, show the program of a
conference, and are a teaching tool for radio and infrared.</p>
</div>
</div>
<div class="cards">
{chr(10).join(cards)}
</div>
<h2>Écrans / Screens</h2>
<div class="gallery">{gallery}</div>
<p><a href="{self.repo}">{html.escape(self.repo)}</a> — CC BY-NC-SA 4.0</p>
</div>
</body>
</html>
"""

    # ------ Writing

    def write(self):
        if os.path.exists(self.out):
            shutil.rmtree(self.out)
        for lang in list(LANGS) + [NOTES]:
            os.makedirs(os.path.join(self.out, lang), exist_ok=True)
        res = os.path.join(self.out, RES)
        for rel in sorted(self.files):
            dst = os.path.join(res, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copyfile(os.path.join(ROOT, rel), dst)
        self.zip_dir(os.path.join(ROOT, 'docs', 'sd'), os.path.join(res, 'carte_sd.zip'))
        with zipfile.ZipFile(os.path.join(res, 'outils.zip'), 'w', zipfile.ZIP_DEFLATED) as z:
            for rel in USER_TOOLS + USER_TOOL_MODULES:
                if rel in self.files:
                    z.write(os.path.join(ROOT, rel), posixpath.basename(rel))  # Flat: the modules beside
        kicad = os.path.join(ROOT, 'hardware', 'kicad')
        if os.path.isdir(kicad):
            os.makedirs(os.path.join(res, 'hardware'), exist_ok=True)
            self.zip_dir(kicad, os.path.join(res, 'hardware', 'kicad.zip'))
            self.files.add('hardware/kicad.zip')
        self.uf2_out = None
        if self.uf2 and os.path.exists(self.uf2):
            self.uf2_out = f'firmware/badge_menu-{VERSION}.uf2'  # The name says its version
            os.makedirs(os.path.join(res, 'firmware'), exist_ok=True)
            shutil.copyfile(self.uf2, os.path.join(res, self.uf2_out))
        else:
            print(f'warning: no firmware ({self.uf2}): build it first')
        for rel, p in self.pages.items():
            with open(os.path.join(self.out, p['out']), 'w', encoding='utf-8') as f:
                f.write(self.render(rel))
        for lang, info in LANGS.items():
            with open(os.path.join(self.out, lang, info['res_page']), 'w', encoding='utf-8') as f:
                f.write(self.resources(lang))
        with open(os.path.join(self.out, 'index.html'), 'w', encoding='utf-8') as f:
            f.write(self.index())
        with open(os.path.join(self.out, 'style.css'), 'w', encoding='utf-8') as f:
            f.write(CSS.lstrip())
        for lang in list(LANGS) + [NOTES]:
            print(f'{lang}: {sum(1 for p in self.pages.values() if p["lang"] == lang)} pages')
        total = sum(os.path.getsize(os.path.join(d, n)) for d, _, ns in os.walk(self.out) for n in ns)
        print(f'{len(self.files)} resources, {size_text(total)}, written to {self.out}')


def main():
    if sys.stdout.encoding and sys.stdout.encoding.lower() != 'utf-8':
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--out', default=os.path.join(ROOT, 'build', 'docs_site'), help='folder written (replaced)')
    parser.add_argument('--repo', default='https://github.com/HackInProvence/badge_hip',
                        help='URL of the repository, for the links to the source code')
    parser.add_argument('--uf2', default=os.path.join(ROOT, UF2), help='the firmware published (default: the build)')
    args = parser.parse_args()
    site = Site(args.out, args.repo, args.uf2)
    site.collect()
    site.write()


if __name__ == '__main__':
    main()
