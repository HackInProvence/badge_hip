"""The distribution of the badge: everything a user needs in one folder and one .zip, ready to publish.

    python tools/make_dist.py                  (writes build/dist/badge_secsea-<version>/ and its .zip)
    python tools/make_dist.py --build          (builds the firmware first: cmake --build build --target badge_menu)

    badge_secsea-1.2.3/
      LISEZ-MOI.html            start here (French, English): flash, SD card, tools, documentation
      firmware/                 badge_menu-1.2.3.uf2 (copied on the RPI-RP2 disk of the badge)
      carte_sd/                 the example content of the SD card (docs/sd), to copy at its root, with
                                ESP32/ESP32CAM.BIN: the firmware of the ESP32-CAM module (when built), that the badge
                                installs (Badge > Télécommande > Mettre à jour le module)
      outils/                   the PC tools of a user (badge_studio.py, badge_remote.py, sd_files.py, image2epi.py...), requirements.txt
      flipper/                  the commands of the badges for a Flipper Zero (tools/flipper)
      documentation/            the documentation in HTML (tools/docs_site.py: the guides, the features, the
                                resources), open documentation/index.html
      fsk-buzzer-link.html      the page that listens to the sound identifier of the badge (Badge > Identifiant sonore)
      CHANGELOG.md, LICENSE
      SHA256SUMS.txt            the checksums of every file

The firmware is the one of the build (build/src/menu/badge_menu-<version>.uf2, written by src/menu/CMakeLists.txt).
It must be the version of the file VERSION and built from committed sources (no "+" in build/src/menu/version.h):
otherwise the script stops (--allow-dirty: a test package, its name ends with "-test"). It also says when the
commit of the build is not the one of the tag v_<version>.

Requires the Python package "markdown" (pip install markdown), like tools/docs_site.py.
"""

import argparse
import datetime
import hashlib
import html
import os
import re
import shutil
import subprocess
import sys
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import docs_site  # noqa: E402

ROOT = docs_site.ROOT
VERSION = docs_site.VERSION
BUILD = os.path.join(ROOT, 'build')
VERSION_H = os.path.join(BUILD, 'src', 'menu', 'version.h')
NAME = 'badge_secsea'
FSK_PAGE = 'docs/fsk-buzzer-link.html'
FLIPPER = 'tools/flipper'
SD = 'docs/sd'
ESP_BIN = os.path.join(ROOT, 'esp32cam', '.pio', 'build', 'esp32cam', 'firmware.bin')  # pio run in esp32cam/
ESP_ON_SD = 'ESP32/ESP32CAM.BIN'  # src/menu/webremote.h WEBREMOTE_OTA_FILE
# The Python packages of the user tools (docs_site.USER_TOOLS): serial port, images, videos
REQUIREMENTS = '''# The Python packages of the tools of the badge: pip install -r requirements.txt
pyserial          # badge_studio.py, badge_remote.py, sd_files.py, contacts_export.py (the USB serial port)
Pillow            # image2epi.py, video2epaper.py
numpy             # video2epaper.py
# audio2wav.py and video2epaper.py also need ffmpeg in the PATH (https://ffmpeg.org)
# badge_studio.py, badge_remote.py and sd_files.py use tkinter (included with Python on Windows and macOS; python3-tk on Linux)
'''


def git(*args):
    r = subprocess.run(['git'] + list(args), cwd=ROOT, capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else ''


def firmware_build():
    """(version, commit, dirty) of the last build, from version.h"""
    if not os.path.exists(VERSION_H):
        return None, None, None
    text = open(VERSION_H, encoding='utf-8').read()
    version = re.search(r'BADGE_VERSION "([^"]*)"', text)
    build = re.search(r'BADGE_BUILD "(\S*)', text)
    commit = build.group(1) if build else ''
    return (version.group(1) if version else None), commit.rstrip('+'), commit.endswith('+')


def check_firmware(allow_dirty):
    """The .uf2 to publish; stops on a firmware that is not the release. Returns (path, test package)."""
    uf2 = os.path.join(BUILD, 'src', 'menu', f'badge_menu-{VERSION}.uf2')
    if not os.path.exists(uf2):
        sys.exit(f'no firmware {os.path.relpath(uf2, ROOT)}: build it (ninja -C build, or --build)')
    version, commit, dirty = firmware_build()
    problems = []
    if version != VERSION:
        problems.append(f'the build is version {version}, the file VERSION says {VERSION}')
    if dirty:
        problems.append(f'the build has uncommitted sources ({commit}+): commit, then build again')
    if problems and not allow_dirty:
        sys.exit('firmware not publishable:\n  ' + '\n  '.join(problems) + '\n(--allow-dirty: a test package)')
    for p in problems:
        print('warning: ' + p)
    tag = git('rev-parse', '--short', f'v_{VERSION}^{{commit}}')
    if not tag:
        print(f'warning: no tag v_{VERSION} (git tag -a v_{VERSION} -m "Version {VERSION}")')
    elif commit and not tag.startswith(commit) and not commit.startswith(tag):
        print(f'warning: the firmware is built from {commit}, the tag v_{VERSION} is {tag}')
    return uf2, bool(problems), commit


def copy_tree(src, dst):
    shutil.copytree(os.path.join(ROOT, src), dst, ignore=shutil.ignore_patterns('__pycache__', '*.pyc', '.*'))


def readme(commit, files):
    """LISEZ-MOI.html: what is in the package and how to start, in French and in English"""
    uf2 = f'firmware/badge_menu-{VERSION}.uf2'
    date = datetime.date.today().isoformat()
    tools = ''.join(f'<li><code>{html.escape(os.path.basename(t))}</code> : {html.escape(docs_site.tool_summary(os.path.join(ROOT, t)))}'
                    f'</li>' for t in docs_site.USER_TOOLS if os.path.exists(os.path.join(ROOT, t)))
    return f'''<!doctype html>
<html lang="fr"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Badge SecSea {VERSION}</title>
<style>
:root {{ --bg: #fff; --fg: #1d2120; --muted: #5f6763; --line: #d6dad7; --accent: #0b6e63; }}
@media (prefers-color-scheme: dark) {{ :root {{ --bg: #141817; --fg: #e7eae8; --muted: #a0a8a4; --line: #303835;
  --accent: #3fc1b0; }} }}
body {{ background: var(--bg); color: var(--fg); font: 16px/1.5 system-ui, -apple-system, "Segoe UI", sans-serif;
  margin: 0 auto; max-width: 52rem; padding: 24px 16px 48px; }}
h1 {{ margin: 0 0 4px; font-size: 1.6rem; }}
h2 {{ margin-top: 2rem; border-bottom: 1px solid var(--line); padding-bottom: 4px; font-size: 1.2rem; }}
a {{ color: var(--accent); }}
code {{ font: .9em ui-monospace, Consolas, monospace; }}
.muted {{ color: var(--muted); }}
li {{ margin: 4px 0; }}
</style></head><body>
<h1>Badge SecSea {VERSION}</h1>
<p class="muted">Firmware {VERSION} ({html.escape(commit or '?')}), paquet du {date}. <a href="#en">English below</a>.</p>

<h2>Démarrer</h2>
<ol>
<li><b>Le firmware</b> : maintenir le bouton BOOTSEL du badge en branchant le câble USB, puis copier
  <a href="{uf2}"><code>{uf2}</code></a> sur le disque <b>RPI-RP2</b> qui apparaît. Le badge redémarre tout seul
  (détails : <a href="documentation/fr/guide_avance.html">guide avancé</a>, § 7 « Flasher et mettre à jour le firmware »).</li>
<li><b>La carte SD</b> (FAT32 ou exFAT) : copier le contenu de <a href="carte_sd/"><code>carte_sd/</code></a> à sa
  racine (programmes des conférences, livres-jeux, sonneries). Images, vidéos et musique : voir le guide
  utilisateur, « Préparer la carte SD ».</li>
<li><b>La documentation</b> : <a href="documentation/index.html">documentation/index.html</a>, à commencer par le
  <a href="documentation/fr/guide_utilisateur.html">guide utilisateur</a>.</li>
</ol>

<h2>Dans le paquet</h2>
<ul>
<li><a href="{uf2}"><code>firmware/</code></a> : le firmware du badge.</li>
<li><a href="carte_sd/"><code>carte_sd/</code></a> : le contenu d'exemple de la carte SD.</li>
<li><a href="outils/"><code>outils/</code></a> : les outils pour le PC (Python 3 ; <code>pip install -r
  outils/requirements.txt</code>) :<ul>{tools}</ul></li>
<li><a href="flipper/"><code>flipper/</code></a> : les commandes des badges pour un Flipper Zero.</li>
<li><a href="fsk-buzzer-link.html"><code>fsk-buzzer-link.html</code></a> : écouter l'identifiant sonore d'un badge
  (Badge > Identifiant sonore) avec le micro du PC ou du téléphone.</li>
<li><a href="documentation/index.html"><code>documentation/</code></a> : les guides et les pages de chaque fonction,
  en français et en anglais.</li>
<li><code>CHANGELOG.md</code> (les nouveautés), <code>LICENSE</code>, <code>SHA256SUMS.txt</code> (les sommes de
  contrôle des {files} fichiers).</li>
</ul>

<h2 id="en">Getting started (English)</h2>
<ol>
<li><b>The firmware</b>: hold the BOOTSEL button of the badge while plugging the USB cable, then copy
  <a href="{uf2}"><code>{uf2}</code></a> to the <b>RPI-RP2</b> disk that shows up. The badge restarts by itself
  (details: <a href="documentation/en/advanced_guide.html">advanced guide</a>, § 7 "Flashing and updating the firmware").</li>
<li><b>The SD card</b> (FAT32 or exFAT): copy the content of <a href="carte_sd/"><code>carte_sd/</code></a> to its
  root (conference programmes, gamebooks, ringtones). Images, videos and music: see the user guide, "Preparing the
  SD card".</li>
<li><b>The documentation</b>: <a href="documentation/index.html">documentation/index.html</a>, starting with the
  <a href="documentation/en/user_guide.html">user guide</a>.</li>
</ol>
<p>The folders: <code>firmware/</code> the firmware, <code>carte_sd/</code> the example SD card, <code>outils/</code>
the PC tools (<code>pip install -r outils/requirements.txt</code>), <code>flipper/</code> the commands for a Flipper
Zero, <code>fsk-buzzer-link.html</code> the listener of the sound identifier, <code>documentation/</code> the
guides in French and English.</p>
</body></html>
'''


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 16), b''):
            h.update(chunk)
    return h.hexdigest()


def main():
    if sys.stdout.encoding and sys.stdout.encoding.lower() != 'utf-8':
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--out', default=os.path.join(BUILD, 'dist'), help='folder of the package and its .zip')
    parser.add_argument('--build', action='store_true', help='build the firmware first (cmake --build build)')
    parser.add_argument('--allow-dirty', action='store_true',
                        help='accept a firmware of uncommitted sources or of another version (a test package)')
    parser.add_argument('--repo', default='https://github.com/HackInProvence/badge_hip',
                        help='URL of the repository, for the links of the documentation to the source code')
    args = parser.parse_args()

    if args.build:
        r = subprocess.run(['cmake', '--build', BUILD, '--target', 'badge_menu'], cwd=ROOT)
        if r.returncode:
            sys.exit('the build failed')
    uf2, test, commit = check_firmware(args.allow_dirty)

    name = f'{NAME}-{VERSION}' + ('-test' if test else '')
    out = os.path.join(args.out, name)
    if os.path.exists(out):
        shutil.rmtree(out)
    os.makedirs(os.path.join(out, 'firmware'))
    shutil.copyfile(uf2, os.path.join(out, 'firmware', os.path.basename(uf2)))

    # The documentation (its resources keep the firmware, the SD card and the tools, for the links of its pages)
    site = docs_site.Site(os.path.join(out, 'documentation'), args.repo, uf2)
    site.collect()
    site.write()

    copy_tree(SD, os.path.join(out, 'carte_sd'))
    if os.path.exists(ESP_BIN):
        dst = os.path.join(out, 'carte_sd', ESP_ON_SD)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(ESP_BIN, dst)
    else:
        print(f'warning: no firmware of the ESP32-CAM module ({os.path.relpath(ESP_BIN, ROOT)}): '
              'cd esp32cam, pio run')
    copy_tree(FLIPPER, os.path.join(out, 'flipper'))
    tools = os.path.join(out, 'outils')
    os.makedirs(tools)
    for rel in docs_site.USER_TOOLS + docs_site.USER_TOOL_MODULES:
        if os.path.exists(os.path.join(ROOT, rel)):
            shutil.copyfile(os.path.join(ROOT, rel), os.path.join(tools, os.path.basename(rel)))  # Flat: they import
    with open(os.path.join(tools, 'requirements.txt'), 'w', encoding='utf-8') as f:
        f.write(REQUIREMENTS)
    shutil.copyfile(os.path.join(ROOT, FSK_PAGE), os.path.join(out, os.path.basename(FSK_PAGE)))
    for rel in ('CHANGELOG.md', 'LICENSE'):
        shutil.copyfile(os.path.join(ROOT, rel), os.path.join(out, rel))

    files = sorted(os.path.relpath(os.path.join(d, n), out).replace(os.sep, '/')
                   for d, _, names in os.walk(out) for n in names)
    with open(os.path.join(out, 'LISEZ-MOI.html'), 'w', encoding='utf-8') as f:
        f.write(readme(commit, len(files) + 1))
    files = sorted(files + ['LISEZ-MOI.html'])
    with open(os.path.join(out, 'SHA256SUMS.txt'), 'w', encoding='utf-8', newline='\n') as f:
        for rel in files:
            f.write(f'{sha256(os.path.join(out, rel))}  {rel}\n')

    archive = out + '.zip'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
        for rel in files + ['SHA256SUMS.txt']:
            z.write(os.path.join(out, rel), f'{name}/{rel}')
    print(f'{name}: {len(files) + 1} files, {docs_site.size_text(os.path.getsize(archive))} zipped')
    print(f'  {out}')
    print(f'  {archive}')


if __name__ == '__main__':
    main()
