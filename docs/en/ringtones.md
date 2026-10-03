# Ringtones (RTTTL player)

Menu **Media > Ringtones** (Médias > Sonneries): the badge plays ringtones in the RTTTL format, the one of the
ringtones of the old Nokia phones. A dozen public domain tunes are built into the badge (Lettre à Élise (Für Elise),
Ode à la joie (Ode to Joy), Frère Jacques, Au clair de la lune, La Marseillaise, Korobeiniki, Greensleeves...), and
you can add as many as you like on the SD card.

*Version française : [sonneries.md](../fr/sonneries.md).*

## Use

- **The list**: the `SONNERIES/` and `RTTTL/` folders of the SD card, then the ringtones of the badge. In a folder:
  its sub-folders (`Name/`), then its ringtones, with the name of the file in brackets. The files are read by pages
  of 32, in alphabetical order: the `< Previous` and `Next >` rows (`< Précédents`, `Suivants >` in French) change
  the page (so a folder can hold thousands of files).
  Sides: up / down (held: scrolling), right wing (R): open the folder or play, left wing (L): parent folder, then
  back to the menu.
  A ringtone marked `(!)` has an error: R shows the line, the column and the reason.
- **The settings**, at the bottom of the main list: "Volume: 6/8" (the volume of the badge) and "LEDs: 25 %" (the
  brightness of the LEDs while playing: off, 10, 25 (default), 50 or 100 %); R: next value.
- **While playing**: the name, the note played (e.g. `La#5`, `Rest` (`Silence` in French); the note names stay
  French: Do = C, Ré = D, Mi = E, Fa = F, Sol = G, La = A, Si = B), a progress bar and the time.
  The LEDs light up at each note, with one colour per note (Do red, Ré orange, Mi light green, Sol cyan, La blue...).
  L: stop, sides: previous / next ringtone, R: start again from the beginning.
- The **mute mode** is respected: no sound nor LEDs (the playback goes on silently).
- Playing a ringtone unlocks the **Music lover** (Mélomane) achievement.

## Adding ringtones on the SD card

Create a `SONNERIES` or `RTTTL` folder (or both) at the root of the SD card and put `.txt`, `.rtttl` or `.rtx` text
files (RTTTL format) or `.bas` files (PICAXE format, see below) in it, directly or in sub-folders:

```
SD card
├── SONNERIES/
│   ├── classique.txt
│   └── exemples.rtttl
├── RTTTL/
│   ├── Films/
│   │   └── western.rtx
│   └── jeux.txt
└── ...
```

- **one ringtone per line** (a file can hold several);
- empty lines and those starting with `#` are ignored (comments);
- UTF-8 text (the accents of the names are shown) or ASCII; lines of 2048 characters at most;
- in a folder: 40 sub-folders shown, any number of files (by pages of 32), 128 ringtones per page at most;
- the playing page shows the full path of the file; the sides go to the ringtones of the same page.

Examples are in [`docs/sd/SONNERIES`](../sd/SONNERIES): copy this folder to the SD card.

## The RTTTL format

```
name:settings:notes
Ode à la joie:d=4,o=5,b=120:e,e,f,g,g,f,e,d,c,c,d,e,e.,8d,2d
```

**The name**: everything before the first `:`.

**The settings** (separated by commas, in any order, all optional):

| Setting | Meaning | Values | Default |
|---|---|---|---|
| `d` | duration of the notes without a duration | 1 (whole), 2 (half), 4 (quarter), 8, 16, 32 (64 accepted) | 4 |
| `o` | octave of the notes without an octave | 4 to 7 (3 and 8 accepted) | 6 |
| `b` | tempo, in quarter notes per minute | 1 to 999 | 63 |

Other settings (`l=` for instance) are ignored. `name::notes` (without settings) is valid.

**The notes**, separated by commas: `[duration] note [#] [.] [octave] [.]`

- duration: 1, 2, 4, 8, 16, 32 (otherwise the one of `d`);
- note: `c d e f g a b` (`h` = b, the German way), `p` for a pause;
- `#`: sharp (`c#` = C sharp; no flat in RTTTL: write `a#` for B flat);
- `.`: dotted note (duration × 1.5), before or after the octave (`4c.6` and `4c6.` are the same); two dots: × 1.75;
- octave: digit from 4 to 7 (otherwise the one of `o`). `a4` = 440 Hz, `a5` = 880 Hz.

Upper and lower case are the same, spaces are allowed between the elements.

Variants of some converters, accepted too: `_` for the sharp (`f_5` = `f#5`), the sharp or the dot before the note
(`8#d4`, `8.c6`), an empty part between the name and the defaults (`Name: :d=4,o=5,b=112:...`).

Examples:

```
# A scale with the default settings
Gamme::c,d,e,f,g,a,b,c7
# Dotted eighth, sixteenth, sharp, pause, half note of octave 6
Exemple:d=8,o=5,b=100:c.,16d,f#,p,2g6
```

## The PICAXE format (.bas)

The `.bas` files are BASIC programs of the PICAXE microcontrollers; the badge reads their `tune` commands (made by
the PICAXE "Tune Wizard"), each one named by the `'` comment before it (else by the name of the file). The other
lines are ignored.

```
'Jingle Bells
tune 0, 2,($EB,$EB,$EB,$EC,$EB,$EB,$EB,$EC,$EB,$C2,$E7,$E9,$AB)
```

`tune pin, speed, [LED mask,] (notes)`: the speed goes from 1 to 15 (a quarter lasts speed x 73.84 ms); each note is
a byte (`$` hexadecimal, `%` binary or decimal):

| Bits | Meaning |
|---|---|
| 7-6 | duration: `00` quarter, `01` eighth, `10` whole, `11` half |
| 5-4 | octave: `00` middle (C = 523 Hz), `01` high, `10` low |
| 3-0 | note: 0 = C ... 11 = B, 12 to 15 = pause |

The badge converts the command to RTTTL (`'Jingle Bells` → `Jingle Bells:d=4,o=5,b=406:2b4,2b4,...`). No dotted
notes nor sixteenths in this format.

## Sorting a collection

[`tools/rtttl_sort.py`](../../tools/rtttl_sort.py) sorts a large collection of ringtones (thousands of files) for the
badge: it reads the files like the badge, removes the duplicates (same melody, even transposed or at another tempo),
leaves out the invalid ringtones, writes one file per ringtone named after its best title and sorts them into
`Dessins animés` (cartoons), `Génériques de séries` (TV series), `Musiques de films` (films) and `Autre` (other, split
by initial) from a categories file `path#line<TAB>D|S|F|A`. A `rapport.tsv` lists each file written, its copies in
the source, the errors and the empty files.

```
python tools/rtttl_sort.py F:/RTTTL_origine F:/RTTTL --categories categories.tsv
```

## The errors

An invalid line stays in the list, marked `(!)`; R shows its line, its column (in bytes from the beginning of the
line) and the reason: `':' missing after name` (`':' manquant après le nom` in French), `':' missing before the
notes` (`':' manquant avant les notes`), `bad d, o or b setting` (`réglage d, o ou b invalide`), `bad duration`
(`durée invalide`), `bad note` (`note invalide`), `bad octave` (`octave invalide`), `',' expected after note`
(`',' attendue après la note`), `no note` (`aucune note`), `line too long` (`ligne trop longue`), `bad PICAXE tune
command` (`commande tune PICAXE invalide`). The serial port (USB)
also prints the details: `rtttl: ...` lines.

## For the developers

- `src/menu/rtttl_parse.c/.h`: the RTTTL parser, in plain C, and the ringtones of the badge (`RTTTL_BUILTIN`);
  tested on the PC by `python src/tests/host/run_tests.py rtttl` (including the files of `docs/sd/SONNERIES`).
- `src/menu/rtttl.c`: the application. The sound is synthesized (square wave, attack, decay, short cut between the
  notes) and written ~150 ms ahead in the audio buffer (`audio.h`): the main loop never waits.
  The note shown and the LEDs follow `audio_played()`: what you hear, not what is written.
