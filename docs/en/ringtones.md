# Ringtones (RTTTL player)

Menu **Médias > Sonneries** (Media > Ringtones): the badge plays ringtones in the RTTTL format, the one of the
ringtones of the old Nokia phones. A dozen public domain tunes are built into the badge (Lettre à Élise (Für Elise),
Ode à la joie (Ode to Joy), Frère Jacques, Au clair de la lune, La Marseillaise, Korobeiniki, Greensleeves...), and
you can add as many as you like on the SD card.

*Version française : [sonneries.md](../fr/sonneries.md).*

## Use

- **The list**: the `SONNERIES/` and `RTTTL/` folders of the SD card, then the ringtones of the badge. In a folder:
  its sub-folders (`Name/`), then its ringtones, with the name of the file in brackets. The files are read by pages
  of 32, in alphabetical order: the `< Précédents` (previous) and `Suivants >` (next) rows change the page (so a
  folder can hold thousands of files).
  Flanks: up / down (held: scrolling), right wing (D): open the folder or play, left wing (G): parent folder, then
  back to the menu.
  A ringtone marked `(!)` has an error: D shows the line, the column and the reason.
- **While playing**: the name, the note played (e.g. `La#5`, `Silence`; French note names: Do = C, Ré = D, Mi = E,
  Fa = F, Sol = G, La = A, Si = B), a progress bar and the time.
  The LEDs light up at each note, with one colour per note (Do red, Ré orange, Mi light green, Sol cyan, La blue...).
  G: stop, flanks: previous / next ringtone, D: start again from the beginning.
- The **mute mode** is respected: no sound nor LEDs (the playback goes on silently).
- Playing a ringtone unlocks the **Mélomane** (music lover) achievement.

## Adding ringtones on the SD card

Create a `SONNERIES` or `RTTTL` folder (or both) at the root of the SD card and put `.txt`, `.rtttl` or `.rtx` text
files in it, directly or in sub-folders:

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
- in a folder: 16 sub-folders shown, any number of files (by pages of 32), 128 ringtones per page at most;
- the playing page shows the full path of the file; the flanks go to the ringtones of the same page.

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

Examples:

```
# A scale with the default settings
Gamme::c,d,e,f,g,a,b,c7
# Dotted eighth, sixteenth, sharp, pause, half note of octave 6
Exemple:d=8,o=5,b=100:c.,16d,f#,p,2g6
```

## The errors

An invalid line stays in the list, marked `(!)`; D shows its line, its column (in bytes from the beginning of the
line) and the reason (in French): `':' manquant après le nom` (':' missing after the name), `':' manquant avant les
notes` (':' missing before the notes), `réglage d, o ou b invalide` (invalid d, o or b setting), `durée invalide`
(invalid duration), `note invalide` (invalid note), `octave invalide` (invalid octave), `',' attendue après la note`
(',' expected after the note), `aucune note` (no note), `ligne trop longue` (line too long). The serial port (USB)
also prints the details: `rtttl: ...` lines.

## For the developers

- `src/menu/rtttl_parse.c/.h`: the RTTTL parser, in plain C, and the ringtones of the badge (`RTTTL_BUILTIN`);
  tested on the PC by `python src/tests/host/run_tests.py rtttl` (including the files of `docs/sd/SONNERIES`).
- `src/menu/rtttl.c`: the application. The sound is synthesized (square wave, attack, decay, short cut between the
  notes) and written ~150 ms ahead in the audio buffer (`audio.h`): the main loop never waits.
  The note shown and the LEDs follow `audio_played()`: what you hear, not what is written.
