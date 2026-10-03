---
name: badge-translation
description: Translate the SecSea badge firmware texts, add or change a translated text, add a new language, or check the translations (tools/i18n.py, src/menu/lang/*.po, N_/_ markers). Use when the user asks to translate the badge, add a language, fix an English (or other) text on the screen, or when new French UI texts were added to src/menu and must be marked and translated.
---

# Translating the badge

Read `docs/en/translation.md` (or `docs/fr/traduction.md`) and `src/menu/i18n.h` first: they describe the mechanism.

## Rules for the code (src/menu)
- UI texts are written in French. Mark every literal that is drawn as it is with `N_("...")` (identity macro; the
  drawing functions gfx_text/gfx_text_width/ui_* translate the whole string when drawing it).
- Use `_("...")` (= `tr()`, translated now) only for a format given to snprintf() whose result is drawn, and for
  French literals inserted with %s into such a format. For a table entry used as a %s argument: `tr(TABLE[i].name)`.
- Never `_()` in a static initializer (use N_), never on data sent by radio/IR, stored, compared as a key, or on
  printf logs (the PC tools parse the logs: they must stay as they are, French titles included in "ui: ..." lines).
- Do not mark game content whose answer is typed in French (hangman words, riddle answers, crypto, CTF flags) nor
  src/menu/gamebook_builtin.c.
- `#include "i18n.h"` in each file using the markers.

## Workflow
1. Mark the texts in the code.
2. `python tools/i18n.py update` (adds the new texts to every src/menu/lang/<code>.po, untranslated).
3. Translate the empty `msgstr` entries. `python tools/i18n.py check --show 50` lists them. When editing a .po by
   script, read and write it with tools/i18n.py's read_po()/write_po() (import tools/i18n.py) rather than by hand.
4. `python tools/i18n.py gen` (writes src/menu/i18n_table.c, never edit it by hand).
5. `python src/tests/host/run_tests.py` (the `i18n` test fails if the table is stale, a format differs, or a
   character is missing in the fonts), then build the firmware.
6. Check the screens: flash a badge, `python tools/badge_screens.py --lang en` (and the `U` key text check:
   texts too wide or cut are logged as `uicheck:`), then fix the translations that are too long.

## Translation style
- Not longer than the French: the screen is 200 px wide (title ~16 characters in the medium font, footer ~30 in
  the small one). Same formats in the same order, same `\n`. Only characters of the fonts: ASCII + French accents
  (src/gfx/gen_fonts.py COMPOSED); a new language needing others must add them there and to FONT_EXTRA in
  tools/i18n.py, then regenerate the fonts (python src/gfx/gen_fonts.py).
- English glossary: G/D -> L/R (wings), flancs -> sides, "G : retour" -> "L: back", "D : choisir" -> "R: choose",
  Réglages -> Settings, Médias -> Media, Jeux solo -> Solo games, Jeux multi -> Multiplayer, Succès ->
  Achievements, Compétences -> Skills, Rencontres -> Meetings, Contrebande -> Smuggling, Loup-garou -> Werewolf,
  Tir à la corde -> Tug of war, Sonneries -> Ringtones, Radio pirate -> Pirate radio, Livres-jeux -> Gamebooks,
  Annonces -> Announcements, Mode muet -> Mute mode, Veille -> Screensaver, cigale -> cicada. No space before
  ":", "!", "?" in English.

## Adding a language
Copy src/menu/lang/en.po to src/menu/lang/<code>.po, set the header `Language: <code>` and
`Language-Name: <its own name>`, translate every msgstr, then gen, test, build. It appears in Settings > Language.
Update docs/fr/traduction.md and docs/en/translation.md if the procedure changes.

## Commit
Commit the code, the .po files and the generated src/menu/i18n_table.c together. Never commit the root
CMakeLists.txt or pico_sdk_import.cmake of the user.
