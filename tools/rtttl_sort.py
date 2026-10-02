"""Sorts a collection of ringtones for the badge (Médias > Sonneries, docs/fr/sonneries.md).

Reads every .txt / .rtttl / .rtx (RTTTL lines) and .bas (PICAXE "tune" commands) file of a folder and its
sub-folders, like the badge does (rtttl_lib.py), then writes a new folder:
- the duplicates are removed: two tunes are the same when their melodies (the intervals between the notes, so a
  transposed copy too) are the same; the copy kept is a .txt rather than a .bas, the one with the most notes;
- one file per tune, named after its best title, the RTTTL name replaced by the title (the .bas are copied as is);
- the tunes in error for the badge are left out (listed in the report);
- the tunes are sorted in the categories given by a file "path#line<TAB>letter" (D = Dessins animés, S = Génériques
  de séries, F = Musiques de films, A = Autre; the path of a file relative to the source folder, with '/', and the
  line of the tune in it); a tune goes to the category of any of its copies other than Autre, else to Autre. A category of more than --split tunes is split by first letter.

    python tools/rtttl_sort.py F:/RTTTL_origine F:/RTTTL --categories categories.tsv
    python tools/rtttl_sort.py F:/RTTTL_origine --list titles.tsv   (the groups and their titles, to classify)
"""

import argparse
import collections
import html
import os
import re
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rtttl_lib import NAME_MAX, RtttlError, parse_rtttl, read_tunes  # noqa: E402

CATEGORIES = {'D': 'Dessins animés', 'S': 'Génériques de séries', 'F': 'Musiques de films', 'A': 'Autre'}
# Prefixes of the file names that are not an artist
GENERIC = re.compile(r'^(films? and tv|theme|themes|tv|unknown|misc|other|others|ring|nokia standard tones|nokia|'
                     r'film|movie|cartoons?|anon|traditional|in development)$', re.I)
EXTENSIONS = ('.txt', '.rtttl', '.rtx', '.bas')


def pitch_key(notes):
    ps = [s for s, _, _ in notes if s is not None]
    return tuple(b - a for a, b in zip(ps, ps[1:]))


def clean(text):
    """A title without the numbering of the copies: "Knight Rider 2", "Theme (2)", "Ymca V2.0"."""
    text = html.unescape(re.sub(r'&amp(?!;)', '&', text))
    text = re.sub(r'[_]+', ' ', text)
    text = re.sub(r'\s*\(\d+\)\s*$', '', text)
    text = re.sub(r'\s+[Vv]\d+(\.\d+)?\s*$', '', text)
    # "Knight Rider 2", but not "Mambo No 5", "Part 2"
    text = re.sub(r'(?<!\b[Nn]o)(?<!\b[Nn]r)(?<!\b[Pp]art)(?<!\b[Vv]ol)(?<!\b[Oo]p)\s+\d\s*$', '', text)
    text = re.sub(r'\s+', ' ', text).strip(' -.')
    return text


# Titles that say nothing: the artist (or the film...) is the title
EMPTY_TITLES = re.compile(r'^(unknown|theme|main theme|theme tune|tune|title|soundtrack|ringtone)$', re.I)


def split_title(stem):
    """(artist or None, title) of a file name; a generic prefix ("Films And Tv - ...") is not an artist."""
    if ' - ' in stem:
        artist, title = stem.split(' - ', 1)
        if GENERIC.match(artist.strip()):
            return None, clean(title)
        if EMPTY_TITLES.match(clean(title)):
            return None, clean(artist)
        return clean(artist), clean(title)
    return None, clean(stem)


def score(stem):
    """How descriptive a file name is: words, an artist, not a cryptic short name."""
    artist, title = split_title(stem)
    s = len(re.findall(r'[A-Za-z]{2,}', title)) * 3 + (4 if artist else 0) + min(len(title), 30) / 10
    if re.fullmatch(r'[a-z0-9]{1,10}', stem):
        s -= 10  # "back2fut", "0071"
    return s


def best_title(group):
    """(artist, title) of a group of copies of a tune: the best file name, else the RTTTL name if better."""
    stems = sorted({os.path.splitext(os.path.basename(t['path']))[0] for t in group})
    stem = max(stems, key=lambda s: (score(s), -len(s), s))
    artist, title = split_title(stem)
    if score(stem) < 0:
        names = [clean(t['name']) for t in group if len(t['name']) > 3]
        if names:
            title = max(names, key=lambda n: (len(n.split()), len(n)))
    return artist, title or stem


def safe_file_name(text):
    text = re.sub(r'[<>:"/\\|?*\x00-\x1f]', ' ', text)
    text = re.sub(r'\s+', ' ', text).strip(' .')
    return text[:60].rstrip(' .') or 'Sans nom'


def cut_utf8(text, size):
    b = text.encode('utf-8')[:size]
    return b.decode('utf-8', errors='ignore')


def scan(src):
    tunes, errors, empty = [], [], []
    for d, _, files in os.walk(src):
        for f in sorted(files):
            if not f.lower().endswith(EXTENSIONS):
                continue
            path = os.path.join(d, f)
            found = read_tunes(path)
            if not found:
                empty.append(path)
            for line_no, text, err in found:
                if err is None:
                    try:
                        name, bpm, notes = parse_rtttl(text)
                    except RtttlError as e:
                        err = str(e)
                if err is not None:
                    errors.append((path, line_no, err))
                    continue
                tunes.append({'path': path, 'line': line_no, 'text': text, 'name': name, 'notes': notes,
                              'bas': f.lower().endswith('.bas'), 'multi': len(found) > 1})
    return tunes, errors, empty


def group_tunes(tunes):
    groups = collections.defaultdict(list)
    for t in tunes:
        groups[pitch_key(t['notes'])].append(t)
    return list(groups.values())


def keep(group):
    """The copy kept: a .txt rather than a .bas, then the most notes, then the first path."""
    return min(group, key=lambda t: (t['bas'], -len(t['notes']), t['path']))


def load_categories(path):
    cats = {}
    for line in open(path, encoding='utf-8'):
        parts = line.rstrip('\n').split('\t')
        if len(parts) >= 2 and parts[1].strip().upper() in CATEGORIES:
            cats[parts[0].strip().lower()] = parts[1].strip().upper()
    return cats


def category_of(group, cats, src):
    found = collections.Counter()
    for t in group:
        c = cats.get(f"{os.path.relpath(t['path'], src).replace(os.sep, '/')}#{t['line']}".lower())
        if c:
            found[c] += 1
    # A copy classified as a film, a series or a cartoon wins over "Autre"
    for c in ('D', 'S', 'F'):
        if found[c]:
            return c
    return 'A'


def letter_folder(title):
    c = title[:1].upper()
    return c if 'A' <= c <= 'Z' else '0-9' if c.isdigit() else '#'


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('src', help='folder of the ringtones (read only)')
    parser.add_argument('dst', nargs='?', help='new folder, must not exist')
    parser.add_argument('--categories', help='file "path#line<TAB>D|S|F|A"')
    parser.add_argument('--list', help='writes the groups (one per line: their file names) and stops')
    parser.add_argument('--split', type=int, default=500, help='split a category by first letter above (500)')
    args = parser.parse_args()
    if sys.stdout.encoding and sys.stdout.encoding.lower() != 'utf-8':
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')

    tunes, errors, empty = scan(args.src)
    groups = group_tunes(tunes)
    print(f'{len(tunes)} tunes read, {len(errors)} in error, {len(empty)} empty files, {len(groups)} different')
    if args.list:
        with open(args.list, 'w', encoding='utf-8') as f:
            for g in sorted(groups, key=lambda g: best_title(g)[1].lower()):
                stems = sorted({os.path.splitext(os.path.basename(t['path']))[0] for t in g}, key=str.lower)
                f.write(' | '.join(stems) + '\n')
        return
    if not args.dst:
        sys.exit('give the destination folder (or --list)')
    if os.path.exists(args.dst):
        sys.exit(f'{args.dst} exists: give a new folder')
    cats = load_categories(args.categories) if args.categories else {}

    by_cat = collections.defaultdict(list)
    for g in groups:
        by_cat[category_of(g, cats, args.src)].append(g)
    report = []
    for c, gs in sorted(by_cat.items()):
        base = os.path.join(args.dst, CATEGORIES[c])
        used = set()
        for g in sorted(gs, key=lambda g: best_title(g)[1].lower()):
            t = keep(g)
            artist, title = best_title(g)
            file_name = safe_file_name(f'{artist} - {title}' if artist else title)
            folder = os.path.join(base, letter_folder(file_name)) if len(gs) > args.split else base
            ext = '.bas' if t['bas'] else '.txt'
            name, n = file_name, 2
            while (folder, (name + ext).lower()) in used:
                name = f'{file_name} ({n})'
                n += 1
            used.add((folder, (name + ext).lower()))
            os.makedirs(folder, exist_ok=True)
            out = os.path.join(folder, name + ext)
            if t['bas'] and not t['multi']:
                shutil.copyfile(t['path'], out)
            else:
                # The RTTTL line, its name replaced by the title (a .bas with several tunes: converted)
                rest = t['text'].split(':', 1)[1]
                rtttl_name = cut_utf8(title.replace(':', ' ').replace(',', ' '), NAME_MAX).strip()
                with open(out, 'w', encoding='utf-8', newline='\n') as f:
                    f.write(f'{rtttl_name}:{rest}\n')
            report.append(f'{CATEGORIES[c]}\t{os.path.relpath(out, args.dst)}\t'
                          + ' | '.join(sorted({os.path.relpath(x["path"], args.src) for x in g})))
    with open(os.path.join(args.dst, 'rapport.tsv'), 'w', encoding='utf-8') as f:
        f.write('catégorie\tfichier\tcopies dans la source\n' + '\n'.join(report) + '\n')
        for path, line_no, err in errors:
            f.write(f'erreur\t{os.path.relpath(path, args.src)} ligne {line_no}\t{err}\n')
        for path in empty:
            f.write(f'vide\t{os.path.relpath(path, args.src)}\t\n')
    for c in CATEGORIES:
        print(f'{CATEGORIES[c]}: {len(by_cat[c])}')


if __name__ == '__main__':
    main()
