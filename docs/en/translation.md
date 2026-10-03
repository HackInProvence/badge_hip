# Translation of the badge

*Version française : [traduction](../fr/traduction.md).*

The badge shows its texts in **French** (the language of the code) or in another language: **English** to begin
with. The system is modular: adding a language means adding a file, without touching the code.

## For the user

- **Choosing the language**: Settings > Language (Réglages > Langue). Each language is written in itself
  ("English", "Français") and the title of the page in all of them ("Langue / Language"): R chooses, L goes back.
  The choice is kept (even after a firmware update).
- **Back to English** (a badge left in a language you can't read):
  1. press the **left wing (L) 5 times**: you come back to the main menu, whatever the page;
  2. then the sides in turn, **left, right, left, right, left, right, left, right** (8 presses within 8 seconds,
     without touching the wings).
  The badge switches to English and shows "Language: English".
- **From the serial port** (USB): the key `E` sets the badge to English, `N` goes to the next language.
- The texts received from other badges (preset messages, announcements...) are shown in the language of each badge
  when they are among the translated texts; names, free messages and files of the SD card stay as they are.

## What is translated (and what is not)

Every text of the interface: menus, titles, footers, messages, help pages, werewolf cards, achievements, skills,
errors. These stay in French, as they are contents whose answer is in French: the hangman words, the riddles with a
typed answer, the cryptography challenges and the CTF flags, the built-in gamebook. The media of the SD card (books,
texts, ringtones) are in the language they were written in.

## How it works

- In the code (`src/menu`), the texts are written in French and **marked**:
  - `N_("Réglages")`: the text as it is (tables, labels...). It is translated **when it is drawn**: `gfx_text()`,
    `gfx_text_width()` and the `ui_*()` functions look up the translation of what they draw. So a text stored or
    sent by radio stays in French, and each badge shows it in its own language.
  - `_("Sonnerie %d / %d")`: translated at once, for a **format** given to `snprintf()` (the formatted result would
    not be found), or a text inserted with `%s` into such a format.
  - The serial port logs (`printf`) are not translated: the PC tools read them.
- The translations are in `src/menu/lang/<code>.po` (gettext format: `msgid` = the French, `msgstr` = the
  translation; empty = not translated, the French is shown).
- [`tools/i18n.py`](../../tools/i18n.py) extracts the marked texts, updates the `.po` files and generates
  `src/menu/i18n_table.c`: the table of the firmware (FNV-1a hash of the French text, binary search, then the text
  compared). The chosen language is kept in the store (`store_t.lang`).
- The PC tests (`python src/tests/host/run_tests.py i18n`) check that the table is up to date, that the formats
  (`%d`, `%s`...) of each translation are those of the French, that every character exists in the fonts of the
  badge, then the lookup in every language.

## Adding or changing a text

1. Write the text in French in the code, marked `N_("...")` (or `_("...")` for a format).
2. `python tools/i18n.py update`: the new texts are added to the `.po` files, untranslated.
3. Translate the empty `msgstr` (`python tools/i18n.py check` lists them).
4. `python tools/i18n.py gen`, then build the firmware.

## Adding a language

1. Copy `src/menu/lang/en.po` to `src/menu/lang/<code>.po` (`de.po`, `es.po`...).
2. In its header, set `Language: <code>` and `Language-Name: <name in the language>` ("Deutsch").
3. Translate every `msgstr` (from the English or the French).
4. `python tools/i18n.py gen`, build: the language appears in Settings > Language.
5. **The fonts** only have ASCII and the French accented letters (`à â ç é è ê ë î ï ô ù û ü À É È Ê Ç`):
   `tools/i18n.py check` reports the missing characters. For a language that needs others (ä ö ß ñ...), add them
   to `COMPOSED` (base letter + accent) in [`src/gfx/gen_fonts.py`](../../src/gfx/gen_fonts.py) and to `FONT_EXTRA`
   in `tools/i18n.py`, then generate the fonts again.

## Translation tips

- The screen is 200 × 200 pixels: a translation should not be longer than the French (title: about 16 characters,
  footer: about 30). `tools/badge_screens.py` with the `U` key reports the texts that are cut.
- Keep the same formats (`%d`, `%s`, `%02u`...) in the same order, and the same line breaks (`\n`).
- The buttons: G = left wing, D = right wing, flancs = sides (in English: L, R, sides).
- Screenshots of a language: `python tools/badge_screens.py --lang en` (see the [advanced guide](advanced_guide.md)).
