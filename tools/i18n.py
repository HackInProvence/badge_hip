"""Translations of the badge (docs/fr/traduction.md, skill .claude/skills/badge-translation).

The texts are written in French in the code (src/menu), marked N_("...") (the text, translated when drawn) or
_("...") (translated at once: a format for snprintf()). This script:
- extract: lists the marked texts;
- update: adds the new texts to every src/menu/lang/<code>.po (empty translation), marks the ones gone as obsolete;
- gen: writes src/menu/i18n_table.c, the table of the firmware (French text -> translations);
- check: the table is up to date, the formats (%d, %s...) of each translation match, the characters exist in the
  fonts of the badge; shows the texts not translated yet (an error with --strict).

    python tools/i18n.py update          (after adding or changing texts in the code)
    python tools/i18n.py gen             (after translating: rebuild the firmware then)
    python tools/i18n.py check [--strict]
    python tools/i18n.py stats

A new language: copy src/menu/lang/en.po to src/menu/lang/<code>.po, set its header "Language-Name" (its name in
the language itself, shown in Réglages > Langue), translate every msgstr, then gen.
"""

import argparse
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'src', 'menu')
LANG_DIR = os.path.join(SRC, 'lang')
TABLE = os.path.join(SRC, 'i18n_table.c')
SOURCE_LANG = ('fr', 'Français')

# Characters of the fonts of the badge (src/gfx/gen_fonts.py): ASCII, the accented letters and a few symbols
FONT_EXTRA = 'àâçéèêëîïôùûüÀÉÈÊÇ'

MARK = re.compile(r'(?<![A-Za-z0-9_])(N_|_)\(\s*"')
FORMAT = re.compile(r'%[-+ #0]*(\d+|\*)?(\.(\d+|\*))?(hh|h|ll|l|z|j|t)?[diouxXcspfeEgG%]')


# ---- C string literals ----

def c_unescape(body):
    """Bytes of the body of a C string literal (without the quotes)."""
    out = bytearray()
    i = 0
    while i < len(body):
        c = body[i]
        if c != '\\':
            out += c.encode('utf-8')
            i += 1
            continue
        n = body[i + 1]
        if n in 'ntr0\\"\'?abfv' and not (n == '0' and i + 2 < len(body) and body[i + 2] in '01234567'):
            out += {'n': b'\n', 't': b'\t', 'r': b'\r', '0': b'\0', '\\': b'\\', '"': b'"', "'": b"'", '?': b'?',
                    'a': b'\a', 'b': b'\b', 'f': b'\f', 'v': b'\v'}[n]
            i += 2
        elif n == 'x':
            m = re.match(r'[0-9A-Fa-f]+', body[i + 2:])
            out.append(int(m.group(0), 16) & 0xFF)
            i += 2 + len(m.group(0))
        elif n in '01234567':
            m = re.match(r'[0-7]{1,3}', body[i + 1:])
            out.append(int(m.group(0), 8) & 0xFF)
            i += 1 + len(m.group(0))
        else:
            out += n.encode('utf-8')
            i += 2
    return bytes(out)


def c_literal(text):
    """A C string literal of the UTF-8 text (the bytes over 0x7F as they are: the sources are UTF-8)."""
    out = []
    for ch in text:
        if ch == '\\':
            out.append('\\\\')
        elif ch == '"':
            out.append('\\"')
        elif ch == '\n':
            out.append('\\n')
        elif ch == '\t':
            out.append('\\t')
        elif ord(ch) < 32:
            out.append('\\%03o' % ord(ch))
        else:
            out.append(ch)
    return '"' + ''.join(out) + '"'


def read_literals(src, pos):
    """The concatenated literals from pos (just after the opening quote of the first one); returns (text, end)."""
    parts = []
    i = pos
    while True:
        j = i
        while True:
            if src[j] == '\\':
                j += 2
                continue
            if src[j] == '"':
                break
            j += 1
        parts.append(c_unescape(src[i:j]))
        k = j + 1
        m = re.match(r'(\s|//[^\n]*\n|/\*.*?\*/)*', src[k:], re.S)
        k += m.end()
        if k < len(src) and src[k] == '"':
            i = k + 1
            continue
        return b''.join(parts).decode('utf-8'), k


def extract():
    """{text: [(file, line)]} of the marked texts of src/menu."""
    texts = {}
    for path in sorted(glob.glob(os.path.join(SRC, '*.c')) + glob.glob(os.path.join(SRC, '*.h'))):
        if os.path.basename(path) == 'i18n_table.c':
            continue
        src = open(path, encoding='utf-8').read()
        for m in MARK.finditer(src):
            text, end = read_literals(src, m.end())
            if src[end:end + 1] != ')':
                line = src.count('\n', 0, m.start()) + 1
                sys.exit(f'{os.path.basename(path)}:{line}: {m.group(1)}( must hold string literals only')
            line = src.count('\n', 0, m.start()) + 1
            texts.setdefault(text, []).append((os.path.relpath(path, ROOT).replace(os.sep, '/'), line))
    return texts


# ---- .po files (the subset used here: msgid, msgstr, comments, fuzzy, obsolete) ----

def po_quote(text):
    s = text.replace('\\', '\\\\').replace('"', '\\"').replace('\t', '\\t')
    if '\n' in s and s != '\n':
        lines = s.split('\n')
        chunks = [l + '\\n' for l in lines[:-1]] + ([lines[-1]] if lines[-1] else [])
        return '""\n' + '\n'.join(f'"{c}"' for c in chunks)
    return '"' + s.replace('\n', '\\n') + '"'


def po_unquote(chunks):
    return ''.join(c_unescape(c).decode('utf-8') for c in chunks)


def read_po(path):
    """(header {key: value}, [{'id', 'str', 'refs', 'fuzzy', 'obsolete', 'comments'}])"""
    entries = []
    cur = None
    field = None

    def flush():
        if cur is not None and 'id' in cur:
            entries.append(cur)

    for raw in open(path, encoding='utf-8').read().split('\n'):
        line = raw.strip()
        obsolete = line.startswith('#~')
        if obsolete:
            line = line[2:].strip()
        if not line:
            flush()
            cur, field = None, None
            continue
        if cur is None:
            cur = {'refs': [], 'fuzzy': False, 'obsolete': False, 'comments': [], 'id_chunks': [], 'str_chunks': []}
        cur['obsolete'] = cur['obsolete'] or obsolete
        if line.startswith('#:'):
            cur['refs'].append(line[2:].strip())
        elif line.startswith('#,'):
            cur['fuzzy'] = cur['fuzzy'] or 'fuzzy' in line
        elif line.startswith('#'):
            cur['comments'].append(line[1:].strip())
        elif line.startswith('msgid '):
            field = 'id_chunks'
            cur['id_chunks'].append(line[6:].strip()[1:-1])
        elif line.startswith('msgstr '):
            field = 'str_chunks'
            cur['str_chunks'].append(line[7:].strip()[1:-1])
        elif line.startswith('"') and field:
            cur[field].append(line[1:-1])
        if cur is not None and cur['id_chunks'] is not None:
            cur['id'] = po_unquote(cur['id_chunks'])
            cur['str'] = po_unquote(cur['str_chunks'])
    flush()
    header = {}
    for e in entries:
        if e['id'] == '':
            for l in e['str'].split('\n'):
                if ':' in l:
                    k, v = l.split(':', 1)
                    header[k.strip()] = v.strip()
    return header, [e for e in entries if e['id'] != '']


def write_po(path, header, entries):
    out = ['# Translation of the badge texts (tools/i18n.py, docs/fr/traduction.md).',
           '# Translate each msgstr; keep the formats (%d, %s...) in the same order; empty = not translated (French).',
           'msgid ""', 'msgstr ""']
    for k, v in header.items():
        out.append(f'"{k}: {v}\\n"')
    out.append('')
    for e in entries:
        prefix = '#~ ' if e['obsolete'] else ''
        for c in e['comments']:
            out.append(f'# {c}')
        if not e['obsolete']:
            for r in e['refs'][:6]:
                out.append(f'#: {r}')
        if e['fuzzy']:
            out.append('#, fuzzy')
        for l in ('msgid ' + po_quote(e['id'])).split('\n'):
            out.append(prefix + l)
        for l in ('msgstr ' + po_quote(e['str'])).split('\n'):
            out.append(prefix + l)
        out.append('')
    open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))


def languages():
    """[(code, path)] of the .po files, sorted by code."""
    return [(os.path.splitext(os.path.basename(p))[0], p) for p in sorted(glob.glob(os.path.join(LANG_DIR, '*.po')))]


# ---- Commands ----

def cmd_update(args):
    texts = extract()
    os.makedirs(LANG_DIR, exist_ok=True)
    langs = languages()
    if not langs:
        langs = [('en', os.path.join(LANG_DIR, 'en.po'))]
    for code, path in langs:
        header, entries = read_po(path) if os.path.exists(path) else ({'Language': code, 'Language-Name': code}, [])
        header.setdefault('Language', code)
        header.setdefault('Language-Name', code)
        header['Content-Type'] = 'text/plain; charset=UTF-8'
        old = {e['id']: e for e in entries}
        new = []
        for text in sorted(texts, key=lambda t: (texts[t][0], t)):
            e = old.pop(text, None) or {'id': text, 'str': '', 'fuzzy': False, 'comments': []}
            e['refs'] = [f'{f}:{l}' for f, l in texts[text]]
            e['obsolete'] = False
            new.append(e)
        for e in old.values():  # Gone from the code: kept at the end, obsolete (a text may come back)
            if e['str']:
                e['obsolete'] = True
                e['refs'] = []
                new.append(e)
        write_po(path, header, new)
        done = sum(1 for e in new if e['str'] and not e['obsolete'])
        print(f'{code}: {len(texts)} texts, {done} translated, {len(texts) - done} to translate')


def fnv1a(text):
    h = 0x811C9DC5
    for b in text.encode('utf-8'):
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


def formats(text):
    return [m.group(0) for m in FORMAT.finditer(text) if m.group(0) != '%%']


def font_chars():
    return set(chr(c) for c in range(32, 127)) | set(FONT_EXTRA) | {'\n'}


def load_all():
    texts = extract()
    langs = []
    problems = []
    ok_chars = font_chars()
    for code, path in languages():
        header, entries = read_po(path)
        tr = {}
        for e in entries:
            if e['obsolete'] or not e['str'] or e['fuzzy'] or e['id'] not in texts:
                continue
            if formats(e['id']) != formats(e['str']):
                problems.append(f'{code}: formats differ: "{e["id"]}" -> "{e["str"]}"')
                continue
            bad = sorted(set(e['str']) - ok_chars)
            if bad:
                problems.append(f'{code}: characters missing in the fonts {bad}: "{e["str"]}"')
                continue
            tr[e['id']] = e['str']
        name = header.get('Language-Name', code)
        if set(name) - ok_chars:
            problems.append(f'{code}: characters of the name "{name}" missing in the fonts')
        langs.append((code, name, tr))
    return texts, langs, problems


def table_source(texts, langs):
    keys = sorted(texts, key=lambda t: (fnv1a(t), t))
    out = ['/* Generated by tools/i18n.py gen from src/menu/lang/*.po: do not edit (docs/fr/traduction.md). */',
           '', '#include <stddef.h>', '', '#include "i18n.h"', '',
           f'const int I18N_N_LANGS = {len(langs) + 1};',
           'const i18n_lang_t I18N_LANGS[] = {',
           f'    {{{c_literal(SOURCE_LANG[0])}, {c_literal(SOURCE_LANG[1])}}},']
    for code, name, _ in langs:
        out.append(f'    {{{c_literal(code)}, {c_literal(name)}}},')
    out += ['};', '', f'const int I18N_N_TEXTS = {len(keys)};', '',
            '/* The French texts, by FNV-1a hash */', 'const i18n_text_t I18N_TEXTS[] = {']
    for k in keys:
        out.append(f'    {{0x{fnv1a(k):08X}u, {c_literal(k)}}},')
    out += ['};', '']
    for code, name, tr in langs:
        out.append(f'/* {name} ({code}): NULL = not translated, the French text is shown */')
        out.append(f'static const char *const TEXTS_{code.upper().replace("-", "_")}[] = {{')
        for k in keys:
            out.append(f'    {c_literal(tr[k]) if k in tr else "NULL"},')
        out += ['};', '']
    out.append('/* The translations of each language after the French one */')
    out.append('const char *const *const I18N_TRANSLATIONS[] = {')
    for code, _, _ in langs:
        out.append(f'    TEXTS_{code.upper().replace("-", "_")},')
    out += ['};', '']
    return '\n'.join(out)


def cmd_gen(args):
    texts, langs, problems = load_all()
    for p in problems:
        print('warning:', p)
    open(TABLE, 'w', encoding='utf-8', newline='\n').write(table_source(texts, langs))
    for code, name, tr in langs:
        print(f'{code} ({name}): {len(tr)} / {len(texts)} translated')
    print(f'wrote {os.path.relpath(TABLE, ROOT)}')


def cmd_check(args):
    texts, langs, problems = load_all()
    errors = list(problems)
    hashes = {}
    for t in texts:
        if fnv1a(t) in hashes:
            print(f'note: same hash for "{t}" and "{hashes[fnv1a(t)]}" (handled, compared at run time)')
        hashes[fnv1a(t)] = t
    current = open(TABLE, encoding='utf-8').read() if os.path.exists(TABLE) else ''
    if current != table_source(texts, langs):
        errors.append('src/menu/i18n_table.c is not up to date: python tools/i18n.py update, translate, gen')
    for code, name, tr in langs:
        missing = [t for t in texts if t not in tr]
        print(f'{code} ({name}): {len(tr)} / {len(texts)} translated')
        for t in missing[:args.show]:
            print(f'  not translated: "{t}" ({texts[t][0][0]}:{texts[t][0][1]})')
        if missing and args.strict:
            errors.append(f'{code}: {len(missing)} texts not translated')
    for e in errors:
        print('error:', e)
    sys.exit(1 if errors else 0)


def cmd_stats(args):
    texts = extract()
    by_file = {}
    for t, refs in texts.items():
        for f, _ in refs:
            by_file[f] = by_file.get(f, 0) + 1
    for f, n in sorted(by_file.items(), key=lambda x: -x[1]):
        print(f'{n:5} {f}')
    print(f'{len(texts)} different texts')


def cmd_extract(args):
    for t, refs in sorted(extract().items(), key=lambda x: x[1][0]):
        print(f'{refs[0][0]}:{refs[0][1]}\t{t!r}')


def main():
    if sys.stdout.encoding and sys.stdout.encoding.lower() != 'utf-8':
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    sub = parser.add_subparsers(dest='cmd', required=True)
    sub.add_parser('extract', help='list the marked texts').set_defaults(func=cmd_extract)
    sub.add_parser('update', help='update the .po files from the code').set_defaults(func=cmd_update)
    sub.add_parser('gen', help='write src/menu/i18n_table.c').set_defaults(func=cmd_gen)
    p = sub.add_parser('check', help='check the table and the translations')
    p.add_argument('--strict', action='store_true', help='a text not translated is an error')
    p.add_argument('--show', type=int, default=10, help='texts not translated shown per language (10)')
    p.set_defaults(func=cmd_check)
    sub.add_parser('stats', help='marked texts per file').set_defaults(func=cmd_stats)
    args = parser.parse_args()
    args.func(args)


if __name__ == '__main__':
    main()
