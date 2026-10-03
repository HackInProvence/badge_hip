"""Exports the user documentation of the badge (French and English) as a static web site.

From the user guide and the advanced guide of each language, every page of docs/<lang>/ they link to is converted
to HTML (and the pages those link to...), with a menu of the pages, a link to the same page in the other language
(the line "*English version: [...](../en/x.md).*" of each page), and the images they show (docs/screens...).
Links to files of the repository outside docs/ (source code, tools) point to the repository on the web.

    python tools/docs_site.py                    (writes build/docs_site/)
    python tools/docs_site.py --out D:/www/badge --repo https://github.com/HackInProvence/badge_hip/blob/main

The folder is self-contained: copy it on any web server (or open index.html in a browser). Requires the Python
package "markdown" (pip install markdown).
"""

import argparse
import html
import os
import posixpath
import re
import shutil
import sys

try:
    import markdown
except ImportError:
    sys.exit('the Python package "markdown" is needed: pip install markdown')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOCS = os.path.join(ROOT, 'docs')
LANGS = {
    'fr': {'name': 'Français', 'entries': ['guide_utilisateur.md', 'guide_avance.md'],
           'title': 'Badge SecSea — documentation', 'pages': 'Pages', 'home': 'Accueil',
           'other': 'English version', 'source': 'Source'},
    'en': {'name': 'English', 'entries': ['user_guide.md', 'advanced_guide.md'],
           'title': 'SecSea badge — documentation', 'pages': 'Pages', 'home': 'Home',
           'other': 'Version française', 'source': 'Source'},
}
# For the developers: not in the user documentation (their links go to the repository online)
EXCLUDE = {'guide_developpeur.md', 'developer_guide.md'}
IMAGE_EXT = ('.png', '.jpg', '.jpeg', '.gif', '.svg', '.webp')
LINK = re.compile(r'(!?)\[([^\]]*)\]\(([^)\s]+)(\s+"[^"]*")?\)')
OTHER_VERSION = re.compile(r'^\*(English version|Version française)\s*:.*\*\s*$', re.M)

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
header { display: flex; align-items: center; gap: 16px; padding: 10px 20px; border-bottom: 1px solid var(--line);
         position: sticky; top: 0; background: var(--bg); z-index: 2; }
header .site { font-weight: 700; color: var(--fg); text-decoration: none; }
header .lang { margin-left: auto; }
.layout { display: flex; max-width: 1200px; margin: 0 auto; }
nav { width: 260px; flex: none; padding: 20px; border-right: 1px solid var(--line); background: var(--side);
      min-height: calc(100vh - 50px); }
nav h2 { font-size: 13px; text-transform: uppercase; letter-spacing: .05em; color: var(--muted); margin: 0 0 8px; }
nav ul { list-style: none; padding: 0; margin: 0; }
nav li a { display: block; padding: 4px 8px; border-radius: 6px; text-decoration: none; color: var(--fg); }
nav li a.current { background: var(--line); font-weight: 600; }
main { flex: 1; min-width: 0; padding: 20px 40px 60px; }
main img { max-width: 100%; image-rendering: pixelated; border: 1px solid var(--line); }
table { border-collapse: collapse; display: block; overflow-x: auto; margin: 1em 0; }
th, td { border: 1px solid var(--line); padding: 6px 10px; text-align: left; vertical-align: top; }
th { background: var(--side); }
code { background: var(--code); padding: 1px 4px; border-radius: 4px; font-size: .92em; }
pre { background: var(--code); padding: 12px; border-radius: 8px; overflow-x: auto; }
pre code { padding: 0; background: none; }
h1, h2, h3 { line-height: 1.25; }
h2 { border-bottom: 1px solid var(--line); padding-bottom: 4px; margin-top: 2em; }
.home { max-width: 640px; margin: 10vh auto; padding: 0 20px; }
.home a.card { display: block; padding: 18px 20px; margin: 12px 0; border: 1px solid var(--line);
               border-radius: 10px; text-decoration: none; color: var(--fg); background: var(--side); }
.home a.card b { color: var(--accent); }
@media (max-width: 800px) {
  .layout { display: block; }
  nav { width: auto; min-height: 0; border-right: none; border-bottom: 1px solid var(--line); }
  main { padding: 16px; }
}
"""


def page_title(text, fallback):
    m = re.search(r'^#\s+(.+)$', text, re.M)
    return m.group(1).strip() if m else fallback


def html_name(md):
    return os.path.splitext(md)[0] + '.html'


class Site:
    def __init__(self, out, repo):
        self.out = out
        self.repo = repo.rstrip('/')
        self.pages = {}  # (lang, md file) -> {'title', 'text', 'other': (lang, md) or None}
        self.assets = set()  # Paths relative to docs/

    def collect(self):
        """The pages reachable from the entry pages, in the docs of each language."""
        for lang, info in LANGS.items():
            todo = list(info['entries'])
            while todo:
                md = todo.pop(0)
                if (lang, md) in self.pages or md in EXCLUDE:
                    continue
                path = os.path.join(DOCS, lang, md)
                if not os.path.exists(path):
                    print(f'warning: {lang}/{md} missing')
                    continue
                text = open(path, encoding='utf-8').read()
                self.pages[(lang, md)] = {'title': page_title(text, md), 'text': text, 'other': None}
                for _, _, target, _ in LINK.findall(text):
                    target = target.split('#')[0]
                    if target.endswith('.md') and '/' not in target:
                        todo.append(target)
        # The same page in the other language: the "English version" / "Version française" line
        for (lang, md), p in self.pages.items():
            m = OTHER_VERSION.search(p['text'])
            if m:
                link = LINK.search(m.group(0))
                if link:
                    target = posixpath.normpath(posixpath.join(lang, link.group(3).split('#')[0]))
                    parts = target.split('/')
                    if len(parts) == 2 and (parts[0], parts[1]) in self.pages:
                        p['other'] = (parts[0], parts[1])

    def rewrite(self, lang, text):
        """The links of a page: .md -> .html, images copied, files of the repository -> the repository online."""
        def sub(m):
            bang, label, target, title = m.group(1), m.group(2), m.group(3), m.group(4) or ''
            if re.match(r'^[a-z]+:', target) or target.startswith('#'):
                return m.group(0)
            path, _, anchor = target.partition('#')
            anchor = '#' + anchor if anchor else ''
            rel = posixpath.normpath(posixpath.join(lang, path))  # Relative to docs/
            if rel.lower().endswith(IMAGE_EXT) and not rel.startswith('..'):
                self.assets.add(rel)
                return f'{bang}[{label}](../{rel}{title})'
            parts = rel.split('/')
            if path.endswith('.md') and len(parts) == 2 and (parts[0], parts[1]) in self.pages:
                href = html_name(parts[1]) if parts[0] == lang else f'../{parts[0]}/{html_name(parts[1])}'
                return f'{bang}[{label}]({href}{anchor}{title})'
            # Elsewhere in the repository: online
            repo_path = posixpath.normpath(posixpath.join('docs', rel))
            return f'{bang}[{label}]({self.repo}/{repo_path}{anchor}{title})'
        return LINK.sub(sub, text)

    def render(self, lang, md):
        p = self.pages[(lang, md)]
        info = LANGS[lang]
        text = OTHER_VERSION.sub('', p['text'])
        body = markdown.markdown(self.rewrite(lang, text),
                                 extensions=['tables', 'fenced_code', 'toc', 'sane_lists', 'attr_list'],
                                 extension_configs={'toc': {'permalink': False}})
        nav = []
        for (l, m), q in self.pages.items():
            if l == lang:
                cls = ' class="current"' if m == md else ''
                short = re.sub(r'^(Badge SecSea|SecSea badge)\s*[—-]\s*', '', q['title'])
                nav.append(f'<li><a href="{html_name(m)}"{cls}>{html.escape(short[:1].upper() + short[1:])}</a></li>')
        other = ''
        if p['other']:
            ol, om = p['other']
            other = f'<a class="lang" href="../{ol}/{html_name(om)}">{LANGS[ol]["name"]}</a>'
        return f"""<!doctype html>
<html lang="{lang}">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{html.escape(p['title'])} — {html.escape(info['title'])}</title>
<link rel="stylesheet" href="../style.css">
</head>
<body>
<header><a class="site" href="../index.html">{html.escape(info['title'])}</a>{other}</header>
<div class="layout">
<nav><h2>{info['pages']}</h2><ul>
{chr(10).join(nav)}
</ul></nav>
<main>
{body}
</main>
</div>
</body>
</html>
"""

    def index(self):
        cards = []
        for lang, info in LANGS.items():
            for md in info['entries']:
                if (lang, md) in self.pages:
                    t = self.pages[(lang, md)]['title']
                    cards.append(f'<a class="card" href="{lang}/{html_name(md)}"><b>{info["name"]}</b><br>'
                                 f'{html.escape(t)}</a>')
        return f"""<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Badge SecSea — documentation</title>
<link rel="stylesheet" href="style.css">
</head>
<body>
<div class="home">
<h1>Badge SecSea</h1>
<p>Hack In Provence — documentation / documentation</p>
{chr(10).join(cards)}
</div>
</body>
</html>
"""

    def write(self):
        if os.path.exists(self.out):
            shutil.rmtree(self.out)
        for lang in LANGS:
            os.makedirs(os.path.join(self.out, lang), exist_ok=True)
        for (lang, md) in self.pages:
            with open(os.path.join(self.out, lang, html_name(md)), 'w', encoding='utf-8') as f:
                f.write(self.render(lang, md))
        missing = 0
        for rel in sorted(self.assets):
            src = os.path.join(DOCS, rel)
            if not os.path.exists(src):
                print(f'warning: image {rel} missing')
                missing += 1
                continue
            dst = os.path.join(self.out, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copyfile(src, dst)
        with open(os.path.join(self.out, 'index.html'), 'w', encoding='utf-8') as f:
            f.write(self.index())
        with open(os.path.join(self.out, 'style.css'), 'w', encoding='utf-8') as f:
            f.write(CSS.lstrip())
        for lang in LANGS:
            n = sum(1 for l, _ in self.pages if l == lang)
            print(f'{lang}: {n} pages')
        print(f'{len(self.assets) - missing} images, written to {self.out}')


def main():
    if sys.stdout.encoding and sys.stdout.encoding.lower() != 'utf-8':
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--out', default=os.path.join(ROOT, 'build', 'docs_site'), help='folder written (replaced)')
    parser.add_argument('--repo', default='https://github.com/HackInProvence/badge_hip/blob/main',
                        help='URL of the repository files, for the links to the code')
    args = parser.parse_args()
    site = Site(args.out, args.repo)
    site.collect()
    site.write()


if __name__ == '__main__':
    main()
