# SecSea badge — advanced guide

This guide is for advanced users and for the event organizers (staff): people who plug the badge into a computer,
use the scripts of the repository, the USB serial console, the Admin menu, the Flipper Zero, or prepare SD cards for
many badges. No programming knowledge is needed.

- Everyday use (menus, games, social...): [user guide](user_guide.md).
- Building, changing the firmware, the architecture: [developer guide](developer_guide.md).
- The features in detail: [ringtones](ringtones.md), [pirate radio](pirate_radio.md), [smuggler](smuggler.md),
  werewolf ([loup_garou.md](../fr/loup_garou.md), in French), [gamebooks](gamebooks.md),
  and every page of the badge in pictures: [screens](screens.md).

The badge shows its texts in English or in French (Settings > Language, see [§ 8](#8-badge-language)): menu entries
and page names are given as an English badge shows them, with the French label in brackets where useful.

*Version française : [guide avancé](../fr/guide_avance.md).*

## Contents

1. [Before you start](#1-before-you-start)
2. [The PC tools](#2-the-pc-tools)
3. [The USB serial console](#3-the-usb-serial-console)
4. [The Admin menu](#4-the-admin-menu)
5. [Remote control, mute mode, sleep and demo mode](#5-remote-control-mute-mode-sleep-and-demo-mode)
6. [The SD card](#6-the-sd-card)
7. [Flashing and updating the firmware](#7-flashing-and-updating-the-firmware)
8. [Badge language](#8-badge-language)
9. [Troubleshooting](#9-troubleshooting)


## 1. Before you start

### 1.1 The buttons and their letters

The badge is seen from the front, the head of the cicada at the top. The serial console and the scripts name the
buttons with a letter:

| Button | Letter (short press) | Letter (long press) | In the menus |
|---|---|---|---|
| Left wing (L; G in French) | `a` | `A` | back |
| Right wing (R; D in French) | `b` | `B` | OK |
| Right side | `x` | `X` | down |
| Left side | `y` | `Y` | up |

A long press lasts 0.8 s on the badge.

### 1.2 What you need on the computer

| Need | For |
|---|---|
| Python 3 | every script |
| `pip install pyserial` | everything that talks to the badge over USB (`badge_remote.py`, `badge_selftest.py`, `badge_screens.py`, `badge_media_test.py`, `contacts_export.py`, `test_*.py`) |
| Tkinter | the window of `badge_remote.py` (included with Python on Windows and macOS; package `python3-tk` on Debian / Ubuntu) |
| `pip install pillow numpy` | converting pictures and videos, `skills_icons.py --preview`, `gen_fonts.py`, `image2epaper.py` |
| [ffmpeg](https://ffmpeg.org/) in the `PATH` | converting sounds and videos |
| gcc or clang | the host tests (`run_tests.py`) |
| picotool (optional) | flashing without touching the button of the badge |

The other scripts (Flipper, ringtones, gamebooks, scores, icons) only use the Python standard library. Run them from
the root of the repository: several of them read files of the repository (fonts, C sources).

### 1.3 Finding the badge and freeing its serial port

- Plugged in USB **and switched ON**, the badge shows up as a serial port: `COM9` on Windows, `/dev/ttyACM0` on
  Linux, `/dev/cu.usbmodem...` on macOS. When OFF, only charging works.
- Without `--port`, the scripts take the first Raspberry Pi device (USB id 0x2E8A): with several badges plugged in,
  give the port.
- **A serial port opens only once**: close `badge_remote.py`, the terminal or another script before starting a new
  one. This is the first cause of "cannot open" / "used by another application" messages.
- What the badge sends before the port is opened is lost (a USB limit): the `version:` line of the boot is only
  seen if the port was already open; type `!` to see it again.


## 2. The PC tools

All the scripts are in `tools/` (except the media converters, in `src/`). `-h` prints the help of each one. In the
tables, "default" is the value without the option.

### 2.1 Testing a badge

#### `tools/badge_selftest.py` — automatic test of a badge

Simulates the buttons over USB, checks the log lines and the screen of the badge: menus, games, screensaver, radio,
IR, pictures, ringtones, admin mode, battery, social... Writes a PASS / FAIL / SKIP table and PNG screenshots.

- Needs: a badge running `badge_menu`, plugged in, port free; pyserial. An SD card with pictures and ringtones
  avoids some SKIPs.
- Exit code 0 when no test failed (a SKIP is not a failure: no SD card, battery already calibrated...).

| Option | Purpose |
|---|---|
| `--port COM9` | serial port (default: the first badge found) |
| `--out folder` | screenshots folder (default: `selftest_<date>`) |
| `--only g1,g2` | only these groups (see below) |
| `--ctf` | adds the `ctf` group: types the Konami code (**marks the CTF flag as found**) |
| `--no-reboot` | does not reboot the badge (it must be on the main menu) |
| `--verbose`, `-v` | shows every line of the badge |

Groups: `diag`, `menus`, `games`, `puzzles`, `ctf` (only with `--ctf` or `--only ctf`), `settings`, `radio`, `ir`,
`images`, `ringtones`, `apps`, `admin`, `battery`, `radio433`, `social`, `net`.
What each group checks: [developer guide § 9.2](developer_guide.md#92-badge-test-over-usb).

```bash
python tools/badge_selftest.py
python tools/badge_selftest.py --port COM9 --only games,radio -v
```

Precautions:
- the badge is **rebooted** first (`R` key); if it never tuned its radio, the tuning comes first;
- the game records may change (a Simon or Snake game can end with 0 points);
- the admin mode, the mute mode and the virus are switched on during some groups, then off: **the badge ends out of
  the admin mode**, even if it was in it before;
- the `battery` group, on a **non calibrated** badge only, saves two calibration points in the factory sector, then
  clears them: a calibrated badge is not touched;
- with `BADGE_SCORE_KEY` in the environment (see `score_check.py`), the signature of the score QR code is checked.

#### `tools/badge_media_test.py` — playing every medium of the SD card

Opens Media > Videos (SD card) then Media > Music (SD card), plays every file (sub-folders included) and checks that it starts and
goes to its end (or to the requested duration): read errors, early stops, frames per second of the videos.

| Option | Purpose |
|---|---|
| `--port COM9` | serial port |
| `--videos` | only the videos |
| `--music` | only the music (without `--videos` nor `--music`: both) |
| `--max N` | seconds played of each **video** (default 0: the whole file) |
| `--music-max N` | seconds played of each **music** (default 5: the start; 0: the whole file) |

```bash
python tools/badge_media_test.py --port COM9 --videos --max 10
```

Output: a report printed and written to `media_report.txt` (current folder); exit code 1 on a failure. The badge is
rebooted first. Handy to validate an SD card before handing it out.

#### `tools/badge_screens.py` — capturing every screen

Goes through every theme, every entry (Admin included) and inner pages of the applications, saves a PNG of each
screen and checks the texts (`U` key: texts cut, too wide, under the footer).

| Option | Purpose |
|---|---|
| `--port COM9` | serial port |
| `--only Jeux,Social` | only these themes (French menu names, comma separated) |

Output: **rewrites** `docs/screens/*.png`, `docs/screens/checks.txt`, [docs/fr/ecrans.md](../fr/ecrans.md) and
[docs/en/screens.md](screens.md).

Precautions: the badge is rebooted several times; the toggles of the menus are not pressed (settings unchanged); the
pages that transmit by themselves (carrier, hot / cold beacon, contact exchange, radio tuning) transmit during the
capture; the admin mode is switched off after the Admin theme. The expected SD card has the music and the texts in
folders, and the videos at the root of `VIDEOS`.

#### Two-badge tests: `test_party_games.py`, `test_werewolf.py`, `test_smuggler.py`

Two badges plugged in USB, `badge_menu` flashed, ports free. Both badges are rebooted (unless `--no-reboot`), the
buttons simulated, the logs of both badges compared; screenshots in a dated folder. How each test runs:
[developer guide § 9.3](developer_guide.md#93-two-badge-tests).

| Script | Tests | Own options |
|---|---|---|
| `test_party_games.py` | Tug of war (Tir à la corde) and Assassin | `--ports P1 P2` (required), `--only tug,assassin`, `--out` (default `party_<date>`) |
| `test_werewolf.py` | Werewolf (Loup-garou): badge 1 narrator, badge 2 the only real player, robots complete the game | `--ports NARRATOR PLAYER` (required), `--without voleur,capitaine` (roles removed: `voyante`, `sorciere`, `chasseur`, `cupidon`, `petite-fille`, `capitaine`, `voleur`), `--out` (default `werewolf_<date>`) |
| `test_smuggler.py` | Smuggling (Contrebande): exchange, gift, refusal, cancellation | `--ports INVITER GUEST` (required), `--out` (default `smuggler_<date>`) |

Common options: `--no-reboot`, `--verbose` / `-v`.

```bash
python tools/test_party_games.py --ports COM9 COM11 --only tug
python tools/test_werewolf.py --ports COM9 COM11 --without voleur
python tools/test_smuggler.py --ports COM9 COM11
```

Precautions:
- Assassin and Smuggler need the badges **touching** (very strong signal): lay them side by side. The assassin test
  prints the measured RSSI, useful to tune the threshold on site;
- `test_party_games.py` puts badge 1 in admin mode (to play with 2), `test_werewolf.py` puts the narrator in admin
  mode and sets Admin > Werewolf (admin) to "Test: robots"; both restore the previous state at the end. If the
  script is interrupted, check these settings by hand;
- `test_smuggler.py` **changes the cargo** of both badges: run it on test badges.

#### `src/tests/host/run_tests.py` — host tests, no badge

Compiles the C modules of the firmware with the PC compiler and checks their rules (games, radio decoders, vCard,
werewolf, ringtones, gamebooks...), then the Python converters. Useful before building a firmware, or to check the
files of `docs/sd/` (the `rtttl` and `gamebook` tests read them).

| Option | Purpose |
|---|---|
| `names...` | tests to run (default: all): `gfx`, `ir`, `games`, `puzzles`, `score`, `crypto`, `ookdec`, `vcard`, `werewolf`, `rsvp`, `gamebook`, `rtttl`, `screen`, `party_games`, `smuggler`, `image2epi`, `video2epaper`, `audio2wav` |
| `--cc gcc` | C compiler (default: `$CC`, else gcc, else clang) |
| `--keep` | keeps the temporary folders |
| `--verbose`, `-v` | also shows the output of the passing tests |

```bash
python src/tests/host/run_tests.py
python src/tests/host/run_tests.py rtttl gamebook -v
```

Without a C compiler, the C tests are SKIP; without ffmpeg, `video2epaper` and `audio2wav` too.

### 2.2 Preparing the media

The format of each folder of the card: [§ 6](#6-the-sd-card).

#### `src/images/image2epi.py` — pictures → `.epi`

Any picture (JPEG, PNG...) becomes a 200 × 200, 4 gray levels picture for `IMAGES/`. Needs: Pillow.

| Option | Purpose |
|---|---|
| `pictures...` | source files (several allowed) |
| `-o`, `--output-dir` | output folder (default: current folder); one `.epi` per picture, same name |
| `--fit` | whole picture with white bars (default: cropped in the center) |
| `--bw` | pure black and white (1 bit per pixel) |
| `--contrast P` | percentage of dark / light pixels clipped to stretch the contrast (default 1) |
| `--equalize` | equalizes the histogram instead (dull pictures) |
| `--preview` | also writes a `.png` preview next to each `.epi` |

```bash
python src/images/image2epi.py photos/*.jpg -o IMAGES --preview
```

#### `src/video/video2epaper.py` — video → `.epv`

The video becomes 200 × 200 black and white, with the sound for the buzzer. Needs: ffmpeg, numpy, Pillow.

| Option | Purpose |
|---|---|
| `video` | source file (anything ffmpeg reads) |
| `-o`, `--output` | output file (default `VIDEO.EPV`): copy it into `VIDEOS/` |
| `--fps 10\|20\|30` | frames per second (default 10: the best contrast) |
| `--fit` | whole video with bars (default: cropped) |
| `--dither bayer\|fs\|threshold` | dithering (default `bayer`: stable from frame to frame, less ghosting; `fs` finer on still pictures) |
| `--gamma G` | < 1 lightens, > 1 darkens (default 1) |
| `--contrast C` | > 1 increases the contrast (default 1.2) |
| `--start S`, `--duration D` | excerpt: start and duration in seconds |
| `--preview file.gif` | animated preview |
| `--no-audio` | without the sound |

```bash
python src/video/video2epaper.py film.mp4 --start 60 --duration 30 -o VIDEOS/excerpt.epv
```

#### `src/audio/audio2wav.py` — sounds → `.wav` for the buzzer

Converts to 8 bit mono 16 kHz WAV (about 1 MB per minute), filtered and compressed to be audible on the buzzer.
Needs: ffmpeg.

| Option | Purpose |
|---|---|
| `inputs...` | files, wildcards (`"music/*.mp3"`, the quotes matter on Windows) or folders (their audio files, not the sub-folders) |
| `-o`, `--output-dir` | output folder (default: current folder) |
| `--rate R` | sample rate (default 16000) |
| `--no-filter` | keeps the whole band and dynamics (less audible on the buzzer) |
| `--start S`, `--duration D` | excerpt |

```bash
python src/audio/audio2wav.py "albums/*.mp3" -o MUSIQUE/Films
```

A file that fails does not stop the others; the failures are listed at the end.

#### `tools/rtttl_sort.py` (and `tools/rtttl_lib.py`) — sorting a ringtone collection

Sorts a large ringtone collection (thousands of `.txt`, `.rtttl`, `.rtx`, `.bas` files) for `SONNERIES/`: read like
the badge does, duplicates removed (same melody, transposed too), invalid tunes left out, one file per tune named
after its best title, sorted by category. Standard library only. `rtttl_lib.py` is not a script: it is the ringtone
reader (RTTTL and PICAXE) used by `rtttl_sort.py`, the same as the badge's.

| Option | Purpose |
|---|---|
| `src` | source folder (read only) |
| `dst` | new folder, **which must not exist** |
| `--categories file` | file `path#line<TAB>D\|S\|F\|A` (cartoons, TV series themes, film music, other); without it, everything goes to `Autre` |
| `--list file` | writes the groups of duplicates (one per line: their file names), to prepare the categories, then stops |
| `--split N` | a category of more than N tunes is split by first letter (default 500) |

```bash
python tools/rtttl_sort.py F:/RTTTL_origin --list titles.tsv
python tools/rtttl_sort.py F:/RTTTL_origin F:/SONNERIES --categories categories.tsv
```

Output: the `dst` folder and `dst/rapport.tsv` (each file written, its copies in the source, the errors, the empty
files). RTTTL and PICAXE formats: [ringtones.md](ringtones.md).

#### `tools/gamebook_check.py` — checking a gamebook

Reads a book like the badge does, then reports the errors (missing or duplicate section, choice to a missing
section, too many sections, choices or items) and the warnings (unreachable sections, sections with no possible
ending, die roll not covered, item tested but never given, text too long, character missing from the fonts).
Standard library; reads `src/gfx/gfx_fonts.c` of the repository.

| Option | Purpose |
|---|---|
| `books...` | `.txt` files |
| `--pages` | size and number of pages (8 lines) of each section |
| `--play` | play the book in the terminal (`q`: quit) |
| `--c FILE` | writes the book as the book built into the firmware (developers, one book) |

```bash
python tools/gamebook_check.py LIVRES/my_book.txt --pages
```

Exit code 1 on errors. Writing a book: [gamebooks.md](gamebooks.md).

### 2.3 During the event

#### `tools/badge_remote.py` — the screen of the badge on the PC

Shows the screen of the badge in big, updated each time it draws, and drives it with the keyboard or the mouse; log
of the badge, PNG screenshots. Needs: pyserial, Tkinter. How to use it:
[user guide § 6](user_guide.md#6-controlling-the-badge-from-a-computer).

| Option | Purpose |
|---|---|
| `--port`, `-p` | serial port (default: the first badge; otherwise the "Badge :" list of the window) |
| `--zoom`, `-z` | zoom of the screen, 2 to 4 (default: from the size of the PC display) |
| `--snapshot`, `-s` `file.png` | saves the current screen and exits, no window |

Keyboard (the window has the focus): up arrow / `y` = left side, down / `x` = right side, left / `a` = left wing,
right / Enter / `b` = right wing; Shift + key = long press; F5: ask the screen again; F12: screenshot
(`badge_YYYYMMDD_HHMMSS.png` in the current folder). Buttons "Capture", "Rafraîchir" (refresh), "Diagnostic" (`!`),
check boxes "Journal" (log), "Mode clavier (saisie de texte)" (keyboard mode) and "Mode admin".

- **Keyboard mode**: the text typed goes to the editor open on the badge (name, contact card, answers,
  announcements), including the accented letters é è ê à â ç ô î ù û ë ï É È À Ç; Enter: done, Escape: cancel,
  Backspace: erase. The arrows stay the buttons. Much faster than the 4 buttons to write announcements.
- **Admin mode**: switches the admin mode on / off without the secret sequence (discreet in front of the audience).

```bash
python tools/badge_remote.py --port COM9 --zoom 3
python tools/badge_remote.py --snapshot screen.png
```

#### `tools/contacts_export.py` — exporting the received contact cards

Sends `k` to the badge and gets the received cards (Social > Contacts) into a vCard file to import in a phone or an
address book. Needs: pyserial, port free.

| Option | Purpose |
|---|---|
| `--port COM9` | serial port |
| `-o`, `--output` | output file (default `contacts.vcf`, overwritten if it exists) |

```bash
python tools/contacts_export.py -o my_contacts.vcf
```

The skills of the received cards go in the `CATEGORIES` line.

#### `tools/battery_log.py` — battery life test

The badge under test runs on its battery, away from the USB, with Settings > Battery by radio: yes (its beacons carry
its battery every 2 s). Another badge plugged in the PC hears them and writes `battery: <id> <name> raw <ADC> mv <mV>
usb <0|1> rssi <dBm>`; the script keeps one sample per badge every `--interval` seconds in a CSV (time, elapsed
minutes, id, name, raw ADC, mV, calibrated mV, USB, RSSI). Prerequisite: `pyserial`.

| Option | Role |
|---|---|
| `--port COMx` | port of the listening badge (found by itself) |
| `--name NAME` | only this badge (its name or the start of its id) |
| `--interval S` | seconds between two samples of a badge (default 60) |
| `--cal raw:mV,raw:mV` | two calibration points of the badge under test: converts the raw ADC to mV |
| `-o FILE.csv` | file (default `battery_<date>.csv`, appended if it exists) |

```bash
python tools/battery_log.py --port COM11 --name Tristan --interval 300
```

A badge not calibrated sends 0 mV: the curve of the raw ADC is still usable (and `--cal` converts it after a
calibration). A badge no longer heard has run out of battery: the last sample gives the battery life.

#### `tools/score_check.py` — checking the scores and making a leaderboard

The games Tic-tac-toe (Morpion), Connect 4 (Puissance 4), Simon, Reflexes (Réflexes) and Snake show a signed QR code at the
end of a game (or with a long press on the game in the menu, for the record):
`HIP26:<game>:<score>:<badge id>:<name>:<signature>`. Scan them with a phone, paste the texts in a file, one line per
QR code, then run the script: it rejects the scores edited by hand and ranks each game (best score of each badge; a
QR code scanned twice counts once). Standard library.

| Option | Purpose |
|---|---|
| `inputs...` | QR code texts, or files (one text per line); `-` or nothing: standard input |
| `--key HEX` | the 128 bits key (32 hex digits); default: `BADGE_SCORE_KEY` environment variable |
| `--make-key` | generates a new key and its masked table to paste into `src/menu/score_code.c` (developers: a new firmware is needed) |

```bash
export BADGE_SCORE_KEY=0123...      (Linux / macOS; set BADGE_SCORE_KEY=... on Windows)
python tools/score_check.py scores.txt
```

Output: the ranking per game, `INVALID` lines; exit code 1 if a text is invalid. The key is not in the repository:
the organizers keep it. The puzzles have no QR code.

### 2.4 CTF and game content (generators)

These scripts rewrite source files of the firmware: the result reaches the badges only after a rebuild and a flash
([§ 7](#7-flashing-and-updating-the-firmware)).

| Script | Purpose | Options | Output |
|---|---|---|---|
| `tools/crypto_ctf_make.py` | the 13 crypto challenges (Solo games > Crypto CTF): plaintexts, enciphered by the script, checked against the width of the screen | no option: prints the C table; `--update`: replaces it in `src/menu/crypto_ctf.c`; `--answers`: prints the answers and the final flag | the C table, or the solutions |
| `tools/skills_icons.py` | 16 × 16 pictograms of the 20 skills, drawn in ASCII in the script | `--preview icons.png` (Pillow) | rewrites `src/menu/skills_icons.h` on every run |
| `tools/smuggler_icons.py` | 32 × 32 icons of the smuggler goods | `--png sheet.png` (contact sheet, instead of writing the C); `--check` (exit code 1 when the C is not up to date) | rewrites the icons in `src/menu/smuggler_goods.c` |
| `tools/werewolf_icons.py` | 32 × 32 illustrations of the werewolf cards | `--png sheet.png`; `--check` | rewrites the icons in `src/menu/werewolf_cards.c` |

Precautions:
- **`crypto_ctf_make.py` is the solution file** (spoilers): `--answers` prints every answer and the flag; do not
  project it, do not run it in front of the participants;
- `skills_icons.py` rewrites its file even without a change: only useful for development;
- `--check` and `--png` write nothing in the sources.

### 2.5 Flipper Zero and radio

#### `tools/flipper/*.sub` — the ready-made remotes

Two Princeton 24 bit files (te = 400 µs, preset `FuriHalSubGhzPresetOok650Async`, 433.92 MHz) to copy into the
`subghz` folder of the SD card of the Flipper, then Sub-GHz > Saved > the file; each button of the Flipper sends a
command:

| Flipper button | `SecSea_general.sub` (`C1 6A 01`) | `SecSea_talk.sub` (`C1 6A 11`) |
|---|---|---|
| OK | the cicada sings (0x01) | talk: green (0x11) |
| Up | mute mode (0x02) | talk: orange, 5 min (0x12) |
| Down | end of mute mode (0x04 → 0x03) | talk: angry red (0x14) |
| Right | — | talk: red, finished (0x1F → 0x13) |
| Left | — | talk: off (0x18 → 0x10) |

Hold the button for a second. The commands: [§ 5](#5-remote-control-mute-mode-sleep-and-demo-mode).

#### `tools/ook_sub.py` — OOK `.sub` files (remotes, weather sensors)

Writes Sub-GHz RAW files (preset `FuriHalSubGhzPresetOok650Async`) with the timings the badge decodes: to drive the
badges with a command the two files above do not have, or to test the 433 MHz decoder, the weather station and the
433 MHz hunt. Standard library.

| Sub-command | Arguments and options |
|---|---|
| `princeton CODE` | 24 bit code (`0xC16A30`...); `--te µs` (default 400); `--repeats N` (default 10 frames); `-o file` (default `princeton_<CODE>.sub`) |
| `came CODE`, `nice CODE` | `--bits 12\|24` (default 12); `--repeats N` (default 8); `-o file` (default `came_<CODE>.sub`, `nice_<CODE>.sub`) |
| `weather` | `--temp °C` (required), `--hum %` (default 50), `--channel N` (default 1), `--id N` (default 0x5A; 14 bits for Acurite), `--battery-low`, `--protocol all\|nexus\|thermopro_tx4\|gt_wt02\|infactory\|lacrosse_tx141thbv2\|acurite_592txr` (default `all`: one `ook_<protocol>.sub` file per sensor), `--repeats N`, `-o folder` (default: current folder) |
| `check FILES...` | checks the shape of RAW files (not "Key" files such as `tools/flipper/*.sub`) |
| `selftest` | generates everything in a temporary folder and checks it |

```bash
python tools/ook_sub.py princeton 0xC16A30 -o chorus_frere_jacques.sub    # starts song 0 of the chorus
python tools/ook_sub.py weather --temp 21.5 --hum 45 --channel 2 -o weather
```

A channel out of the range of a sensor skips it; a temperature out of its range is written with a warning (the badge
will reject it).

#### `tools/flipper_net_sub.py` — packets of the network of the cicadas

Writes RAW files with a custom GFSK preset: the Flipper then sends real packets of the network of the badges. Also
prints the "SecSea" preset that lets the Flipper record the packets of a badge. Standard library.

| Argument / option | Purpose |
|---|---|
| `command CMD` | a remote command (`NET_COMMAND`), e.g. `0x02` |
| `ping` | a ping (the badges print it on their serial port) |
| `pirates [N]` | a **network of N cicadas nearby** (6 by default, 16 at most) with pirate names (Rackham, Barbossa, AnneBony, Surcouf, La Buse...), each with a random score, skills and level, sending their beacons (`NET_BEACON`) `--repeats` times: to test the reception and the decoding of the beacons (Social > Radar, or `!` on the console of the badge) |
| `raw TYPE BYTES...` | any packet: the type, then the data bytes in hex |
| `preset` | prints the lines to add to the file `subghz/assets/setting_user` of the SD card of the Flipper |
| `--id N` | id of the sender, 4 bytes (default `0x5EC5EA26`) |
| `--repeats N` | packets in the file (default 8) |
| `--gap MS` | milliseconds between two packets (default 250) |
| `--ttl N` | `command`: hops of the relay by the cicadas, 0 to 4 (default 2, 0: no relay) |
| `-o`, `--output` | output file (default `secsea_<kind>.sub`) |
| `--power DBM` | power of the Flipper: 10 (default, like the badges), 7, 5, 0, -10, -15, -20, -30 |
| `--send` | copies the file to the Flipper plugged in USB and sends it (its serial console, like `flipper_weather.py`) |
| `--port COMx` | with `--send`: the port of the Flipper (found by itself) |

```bash
python tools/flipper_net_sub.py command 0x02 -o mute_gfsk.sub
python tools/flipper_net_sub.py preset
python tools/flipper_net_sub.py pirates 8 --repeats 6 --power -10 --send
```

Precautions:
- `pirates`: close to the Flipper (beacons received above -80 dBm, 3 times within 10 s), the badges count
  each pirate as a **meeting** (+10 points, kept in their memory). At -10 dBm, the beacons arrive around -85 dBm
  at one meter: listed, no meeting;
- **a `0x05` (sleep) command sent this way really puts the badges in range to sleep**: unlike the Princeton code,
  the network packet is obeyed. Do not generate this file outside a controlled test;
- the nonce of the command is drawn once per file: replaying the same file within 10 s has no effect, after that the
  command is executed again;
- sending a file from the PC through the Flipper command line, and recording the packets of a badge:
  [developer guide § 6.19](developer_guide.md#619-flipper-zero).

#### `tools/flipper_weather.py` — the weather on every badge, from a Flipper

Gets the forecast of a town from [Open-Meteo](https://open-meteo.com) (free, no key), turns it into an
**announcement** of the badges (the time, a short text, a QR code with the next days), then sends it with a Flipper
Zero plugged in USB: the `.sub` files are copied to its SD card through its serial console, then sent (`subghz
tx_from_file`). Every badge in range shows it like an announcement (Social > Announcements keeps it).
Prerequisites: `pyserial`, Internet; the Flipper on its main screen (no Sub-GHz application open), not used by
qFlipper.

| Argument / option | Role |
|---|---|
| `TOWN` | the town (`"La Ciotat"`, `"Marseille"`...) |
| `--lang fr\|en` | language of the text of the announcement (default `fr`) |
| `--days N` | days in the QR code, from tomorrow (default 4) |
| `--rounds N` | times each part is sent (default 4) |
| `--port COMx` | port of the Flipper (found by itself) |
| `-o FILE.sub` | name of the files written (default `build/flipper/secsea_meteo_N.sub`) |
| `--no-send` | only writes the files (to send by hand: Sub-GHz > Saved) |
| `--dry-run` | only shows the forecast and the text |
| `--id N` | id of the sender (default `0x5EC5EA27`) |
| `--ttl N` | hops of the relay by the cicadas, 0 to 4 (default 2; 0: older format, for badges of an older firmware) |

```bash
python tools/flipper_weather.py "La Ciotat"
python tools/flipper_weather.py Marseille --lang en --port COM10
python tools/flipper_weather.py "La Ciotat" --dry-run
```

Example: "Weather Marseille: 24C, cloudy, wind 12 km/h. Tomorrow 22-26C, drizzle, rain 15%.", QR code
"Sun 22-26C drizzle / Mon 21-26C rain / Tue 20-27C drizzle". The ° sign does not exist in the fonts of the badge.

Precautions:
- the announcement is 3 packets; the Flipper sends long RAW files badly (the long packets are lost), hence one small
  file per part, each sent several times: about twenty seconds;
- it shows on **every** badge in range (like an announcement of the organizers): use it with care.

### 2.6 Development helpers

| Script | Purpose | Options |
|---|---|---|
| `src/image2epaper.py` | converts a 2 or 4 color picture (or an animation) into C arrays for the firmware; called by CMake at build time, rarely by hand. Pillow | `image`; `-o`, `--output` (default: standard output); `-b`, `--back-color 00\|01\|10\|11` (fill color when the width is not a multiple of 8, default `11` = white) |
| `src/gfx/gen_fonts.py` | generates `gfx_fonts.c` (14, 18 and 26 pixel fonts, Aileron bundled with Pillow, French accents composed) on the standard output | `--preview picture.png`: sheet of the glyphs |

```bash
python src/gfx/gen_fonts.py --preview fonts.png > src/gfx/gfx_fonts.c
```

`gfx_fonts.c` is in the repository: only regenerate it to change the fonts.


## 3. The USB serial console

### 3.1 Connecting

Any serial terminal, 115200 baud (the speed does not matter over USB), 8N1, no flow control:

```bash
python -m serial.tools.miniterm COM9 115200        # comes with pyserial
picocom -b 115200 /dev/ttyACM0                      # Linux
screen /dev/cu.usbmodem1101 115200                  # macOS
```

PuTTY (Windows, "Serial" type) works too. Each character is a command, **without Enter**; line feeds and unknown
characters are ignored. Turn local echo off. The badge answers with lines of text.

### 3.2 The commands

| Key | Effect |
|---|---|
| `a` `b` `x` `y` | short press: left wing, right wing, right side, left side |
| `A` `B` `X` `Y` | long press of the same button |
| `!` | diagnostic: OLED and IR, network of the cicadas (name, score, meetings, beacons sent / received, neighbours and their dBm), CTF flags, network counters (`net: sent, received, dropped`), state of the network and the radio (`net state:`), listening to the remotes (`remote state:`), remote enabled / mute / admin mode (`remote:`), `version:`, radio (CC1101 version, crystal used and measured), battery (mV, %, ADC value, or "not calibrated") and the factory calibration points |
| `?` | sound state: open, samples played, volume, music position |
| `i` | infrared test: decodes a crafted NEC frame then sends it (about 68 ms) |
| `o` | state of the OOK receiver: active, pulses, frames, RSSI, MARCSTATE, GDO0 |
| `p` | durations of the last signal given to the OOK decoder |
| `O` | trace of the OOK decoding attempts: on / off |
| `k` | exports the received contact cards as vCards (used by `contacts_export.py`) |
| `V` | trace of every network packet sent and received: on / off |
| `r` | CC1101 registers and PATABLE |
| `M` | sends the "Radio: message" (readable with `subghz chat 433920000 0` on a Flipper) |
| `P` | ping at +10 dBm: the badges that hear it print `net: ping #n from <id>, rssi ...` |
| `L` | *loopback* mode (the packets sent come back as from a twin badge, to test alone): on / off |
| `R` | immediate reboot of the badge |
| `[` / `]` | sending the screen on every change: on / off (`@FB ...` lines) |
| `s` | sends the screen once |
| `U` | text check: traces `uicheck: ...` for every text cut, too wide or under the footer: on / off |
| Ctrl+A (0x01) then `A` / `a` | admin mode: on / off (answer `admin: on` / `admin: off`) |
| Ctrl+B (0x02) then a character | types that character in the open text editor (`\r`: done, Escape: cancel, `\b`: erase; 0x80 to 0x8F: the 16 accented letters of the editor) |

The keys also work on the SLEEP page ([§ 5.3](#53-sleep-and-manual-wake-up)). Some terminals catch Ctrl+A (picocom,
screen): for the admin mode, prefer the check box of `badge_remote.py`.

Examples:
- open the Admin menu without the sequence: Ctrl+A then `A`;
- the secret sequence from the keyboard, on the list of the themes: `yyxxyxyx`;
- check a badge: `!`, then `P` on one badge and read the `net: ping` line on another.

The screen is sent as `@FB <BW|4G|WHITE|BLACK> [<lsb plane in base64> [<msb plane in base64>]]`:
`badge_remote.py` draws it. The full protocol: [developer guide § 8](developer_guide.md#8-the-usb-serial-protocol).

### 3.3 Log lines worth knowing

| Line | Meaning |
|---|---|
| `version: 1.0.0 (5d95da4 2026-09-30)` | version, commit and date of the firmware (at boot and with `!`); a `+` after the commit: modified sources |
| `ui: <title>` | a new page is shown (the name of the application when it opens) |
| `notify: <text>` | a notification at the bottom of the screen |
| `admin: on` / `admin: off`, `admin: sending command 0x02`, `admin: badge type Staff` | admin mode, radio command sent, badge type |
| `remote: command 0x02 from <id \| Princeton \| this badge>`, `remote: muted` | remote command executed, mute mode |
| `sleep: requested, rebooting`, `sleep: on (...)`, `sleep: unlock 3/5 0/5`, `sleep: off, rebooting` | put to sleep and unlock |
| `tune: crystal ... Hz, noise ... dBm (remotes above ... dBm), frequency offset a -> b (n packets, mean m)` | result of the radio tuning |
| `browser: /MUSIQUE, 2 dir(s), 5 file(s): ...` | content of a folder opened by a player |
| `video: playing ...`, `video: mount failed ...`, `music: end at 182s of 182s`, `music: stopped at ...` | videos and music playback |
| `image: <file> (2 bit(s) per pixel)`, `saver: on (...)`, `saver: off` | pictures, screensaver |
| `rtttl: /SONNERIES: 3 dir(s), 12 file(s), 40 tune(s)`, `rtttl: playing "..."`, `rtttl: <file> line 7: error 5 (...) at column 12` | ringtones, format errors |
| `gamebook: 3 books (2 on the SD card)`, `gamebook: section 12 missing` | gamebooks |
| `battery: point 1 set, ADC raw ... = ... mV`, `store: factory settings saved (ok)` | battery calibration |
| `store: saved (ok)`, `store: initialized` | settings written to flash; flash initialized (first boot, new format) |
| `net: ping #n from <id>, rssi ...` | ping received |
| `achievement: <name> (+XP, level n)` | achievement unlocked |
| `announce: sending ...`, `announce: received ...`, `vote: ...`, `hotcold: ...`, `infection: ...`, `reset: <what>`, `demo: ...` | admin features |

Other prefixes exist (`social:`, `contacts:`, `chorus:`, `party:`, `werewolf:`, `smuggler:`, `pirate:`...): see the
[developer guide § 8](developer_guide.md#8-the-usb-serial-protocol).


## 4. The Admin menu

### 4.1 Opening and leaving it

- **Secret sequence**: in the main menu (the list of the themes, not inside a theme), tap the sides **left, left,
  right, right, left, right, left, right** within 8 seconds. "Admin mode on" (Mode admin activé) is shown and the **Admin** theme
  appears last, already selected.
- **Over USB**: the "Mode admin" check box of `badge_remote.py`, or Ctrl+A then `A` in a terminal.
- The admin mode is **kept when the badge is switched off** and after a firmware update.
- To leave it: last entry "Leave admin mode" (Quitter le mode admin), or Ctrl+A then `a`.

The admin mode also allows starting the Assassin with 2 players (for tests).

### 4.2 The entries

In the order of the menu:

| Entry | Purpose | Buttons |
|---|---|---|
| **Radio commands** (Commandes radio) | sends a command to all the badges around: Mute (talk), Mute mode off, Cicada: sing, Talk: off / green / orange (5 min) / red (over) / angry red, Going to sleep. This badge executes it too (except the sleep) | sides: choose; R: send; L: back |
| **Cicada LEDs** (LEDs des cigales) | color (9 colors or R, G, B from 0 to 255) and mode (Fixed, Blinking, Fade; times from 50 ms to 5 s) forced on the LEDs of the cicadas around, until "> Restore their LEDs" or their reboot | see the [user guide § 4.8](user_guide.md#48-admin-mode-organizers) |
| **Notices (admin)** (Annonces (admin)) | 6 announcements (time, 56 character text, QR code: link, text, phone, SMS, e-mail, Wi-Fi, GPS position), kept in flash; preview; sent 3 times to all the cicadas | sides: choose; R: open / edit; L: back |
| **Vote (admin)** | opens a predefined question, counts the votes (one per badge, the last one counts) and shows the histogram | R: open the vote, then R: close it |
| **Chorus: start** (Choeur : lancer) | starts "Frère Jacques" (4 voices) or "Ode to Joy" (3 voices); this badge sings the first voice | R: start / stop |
| **Hot-cold beacon** (Balise chaud-froid) | this badge sends a beacon every second at +10 dBm; the others look for it with Social > Hot - cold. The scale ("Burning from -74 dBm" by default, set in 5 dB steps) is kept and sent with the beacon | R: transmit / stop; sides: scale; L: quit |
| **Virus: patient 0** (Virus : patient zéro) | infects this badge to start the epidemic | R: infect; left side: cure this badge |
| **Smuggling (admin)** (Contrebande (admin)) | adds a good of your choice to the cargo of this badge ([smuggler.md](smuggler.md)) | sides: choose; R: add |
| **Werewolf (admin)** (Loup-garou (admin)) | games under 8 players: "8 players minimum" (default), "Small games (4+)", "Test: robots"; kept in flash | sides: choose; R: confirm |
| **Reset** (Remise à zéro) | clears part of the progress of this badge (see § 4.4) | R, then long R: confirm; L: no |
| **Battery (calibration)** (Batterie (calibration)) | calibrates the battery measure (see § 4.3) | |
| **Pirate radio** (Radio pirate) | transmits a melody, a 1 kHz tone or a WAV of the SD card in FM on 433 MHz ([pirate_radio.md](pirate_radio.md)); low power, short tests | |
| **Demo mode** (Mode démo) | the badge shows its features in a loop (see § 5.4) | R: start |
| **Badge type** (Type du badge) | Participant, Speaker (Orateur) or Staff, shown in the banner of the name tag | sides: choose; R: save |
| **Leave admin mode** (Quitter le mode admin) | hides the Admin theme | |

### 4.3 Calibrating the battery

Without a calibration, the badge shows no battery level ("uncalibrated", non calibrée): it never shows a wrong value. Once per
badge, with a multimeter:

1. Badge plugged in USB (charging), Admin > Battery (calibration). The page shows the ADC value (refreshed every
   2 s), the measure, and points 1 and 2.
2. Measure the voltage on the battery terminals. On the "Multimeter" row (Multimètre), set this voltage with the wings
   (L: −, R: +, by 10 mV; held: faster and faster; from 2.50 to 4.50 V).
3. Right side to "> Save the point" (> Enregistrer le point), right wing: "Need a 2nd point".
4. Unplug, let the voltage drop for a few minutes (at least 0.2 V apart, 150 ADC steps), measure, set and save:
   "Point saved", the level is shown.

A new point replaces the closest one. "> Clear" (> Effacer) asks for a confirmation ("R again: clear"). On the "Multimeter"
row, the left wing is used to set the value: leave with a long press on the left wing, or from another row.
The points are **factory settings**: kept by the reset (even "All") and by the updates.
Check from the console: `!` prints `battery: factory points ...`.

### 4.4 Reset

To do before the event on badges used for tests. Each row asks for a confirmation with a **long press on the right
wing**:

| Row | Clears |
|---|---|
| Social scores (Scores sociaux) | score and meetings of the network of the cicadas |
| Game records (Records des jeux) | records of the games and puzzles |
| CTF & crypto (Défis CTF et crypto) | CTF flags, crypto challenges solved |
| Cards received (Contacts reçus) | received contact cards (not your own card) |
| Virus | virus state (healthy) |
| Achievements (Succès et niveau) | achievements and their counters |
| Smuggling (Contrebande) | the cargo (new goods at the next opening) |
| All (Tout) | everything above, plus the gamebook progress |
| Announcements (Annonces d'origine) | the 6 admin announcements get their original texts back (not part of "All") |

Never cleared: the name, the contact card, the settings (screensaver, volume, remote, mute mode, admin mode, badge
type, radio tuning) and the battery calibration.

### 4.5 Preparing a batch of badges (checklist)

1. Flash the same version everywhere ([§ 7](#7-flashing-and-updating-the-firmware)); check Settings > Info.
2. Let the radio tuning of the first boot happen **near other badges switched on**.
3. Calibrate the battery (§ 4.3), if a multimeter is available.
4. `python tools/badge_selftest.py --port ...` on each badge (a few minutes).
5. Admin > Badge type (Speaker, Staff...), Admin > Reset > All, then Leave admin mode.
6. SD card prepared and validated with `badge_media_test.py` (§ 6).


## 5. Remote control, mute mode, sleep and demo mode

### 5.1 The remote commands

A command comes in two ways:
- **from an admin badge** (Admin > Radio commands): the badge first sends it as a Princeton remote (12 frames,
  ~0.6 s, for the talk badges that only listen in OOK), then 5 packets of the network of the cicadas over 2 s, at
  +10 dBm;
- **from a 433 MHz remote**, a Flipper Zero for example: a 24 bit Princeton code `0xC16Axx`, where `xx` is the
  command. The code must be received twice: hold the button for a second.

A command received both ways, or repeated, is executed once. The badge shows the command at the bottom of the screen
and prints `remote: command 0x.. from ...`.

| Command | Effect |
|---|---|
| `0x01` | the cicada sings for 6 s (nothing in mute mode) |
| `0x02` | mute mode |
| `0x03` | end of mute mode |
| `0x10` to `0x14` | lights of the talk badge, if its page is open: off, green, orange (5 min), red (finished), angry red |
| `0x30` + n | starts song n of the chorus (`0x30` Frère Jacques, `0x31` Ode to Joy) |
| `0x04`, `0x18`, `0x1F` (Princeton only) | Flipper buttons: `0x03`, `0x10`, `0x13` (see § 2.5) |

The badge does not obey when Settings > Remote is "no". Other commands from a Flipper: a file generated by
`ook_sub.py princeton 0xC16Axx` (§ 2.5), or Sub-GHz > Add Manually > Princeton_433, then edit the `Key:` line of the
saved file. Do not use `subghz tx` on the command line: it replaces the last 4 bits of the code with 6.

How the badge listens: it listens to the network of the cicadas all the time; when it hears a transmitter that is
not a badge (above the measured noise + 15 dB, or −90 dBm without radio tuning), it listens to the remotes for a
moment. It also opens a listening moment every 10 s for a weak remote. Hence the radio tuning and holding the button.

### 5.2 Mute mode

- Cuts the sound (buzzer, cicada, ringtones, chorus) and the LEDs. The players keep running silently.
- Switched on by the `0x02` command, off by `0x03` or Settings > Mute mode.
- **Kept when switched off and after an update**: a badge that stays mute probably got the command during a talk.
- Exception: the talk badge page keeps its LEDs, and its "STOP!" state makes the cicada sing even in mute mode.

### 5.3 Sleep and manual wake-up

- Order: Admin > Radio commands > Going to sleep, from an admin badge. It only goes through the network of the
  badges (not as Princeton: a Flipper with a plain remote cannot put the conference to sleep; a
  `flipper_net_sub.py command 0x05` file can, see § 2.5).
- The badges that receive it save the order and reboot: radio off, LEDs and sound off, services stopped, page
  "SLEEP: The badge was put to sleep by an admin..." (SOMMEIL on a badge in French). They no longer answer the
  radio commands, a reboot or a firmware update: **the state is kept in flash**.
- Do not fall asleep: the admin badge that sends the order, and a badge open on the talk badge page.
- **Manual wake-up**: **left side 5 times, then right side 5 times**, less than 5 s between two presses (another
  button starts the sequence again). The badge reboots normally. From the serial console: `yyyyyxxxxx`. The log
  follows the progress (`sleep: unlock 5/5 2/5`).

### 5.4 Demo mode

For a booth: Admin > Demo mode, right wing. The badge loops, LEDs in rainbow: name tag (8 s), achievements (6 s),
pictures of the SD card (18 s, one every 6 s), the first video (20 s), skills (5 s), the first music (10 s), screen
demo (15 s), program (6 s), radar (6 s), gamebooks (5 s), credits (6 s), info (5 s). The media steps are skipped
without an SD card or without a file. The screensaver does not start; **any button stops the demo**. It is not kept
after a reboot.

Tips: badge plugged in USB (the demo uses power), SD card with a few pictures, a short video and a short music first
in their folders (alphabetical order).


## 6. The SD card

### 6.1 The card

- Any capacity (micro-SD, SDHC, SDXC), formatted in **FAT32** or **exFAT**. The badge never formats the card and
  never writes on it: it can be prepared on a computer and duplicated.
- Long and accented file names are accepted, **63 bytes at most** (in UTF-8, an accented letter counts 2): a longer
  name is ignored. Hidden and system files and files starting with `.` (macOS `._` files) are ignored.
- The extensions are case insensitive (`.EPI` = `.epi`). The lists are sorted alphabetically.

### 6.2 The tree

```
SD card
├── IMAGES/      .epi pictures (200×200, 4 grays)
├── VIDEOS/      .epv videos
├── MUSIQUE/     .wav sounds, sub-folders allowed
├── TEXTES/      .txt texts (fast reading)
├── SONNERIES/   .txt .rtttl .rtx .bas ringtones, sub-folders allowed
├── RTTTL/       same (either, or both)
└── LIVRES/      .txt gamebooks
```

| Folder | Format | Limits and remarks | Prepare with |
|---|---|---|---|
| `IMAGES` | `.epi`: 200 × 200, 1 or 2 bits per pixel | 48 entries per folder (sub-folders + files) in the viewer; 32 pictures offered for the screensaver; 24 for "Send an image" (Envoyer une image) | `image2epi.py` |
| `VIDEOS` | `.epv`: 200 × 200 black and white frames + 8 bit sound | 48 entries per folder; the demo mode and the OLED take the first one | `video2epaper.py` |
| `MUSIQUE` | `.wav` PCM 8, 16, 24 or 32 bits, 32 bit float, mono or stereo, 4 to 192 kHz | 48 entries per folder; blind test: 16 sub-folders and 64 tracks at most; also the source of the pirate radio WAVs. Advised: 8 bit 16 kHz mono | `audio2wav.py` |
| `TEXTES` | `.txt` UTF-8 (with or without BOM) or Windows-1252 | 48 entries per folder; the reading position is remembered | a text editor |
| `SONNERIES`, `RTTTL` | one RTTTL tune per line (`#`: comment), or PICAXE `tune` commands (`.bas`) | 2048 characters per line; 40 sub-folders shown per folder; any number of files, in pages of 32; 128 tunes per page | `rtttl_sort.py`, [ringtones.md](ringtones.md) |
| `LIVRES` | `.txt` gamebook (sections `== n`, choices `-> n : ...`) | 16 books; 400 sections, 8 choices, 8 items; ~2,000 bytes of text per section | `gamebook_check.py`, [gamebooks.md](gamebooks.md) |

A player (Images, Videos, Music, Speed reading) whose folder is missing or empty shows the **root** of the card:
a `VIDEO.EPV` video (default name of `video2epaper.py`) copied at the root is played too, but rather put each file in
its folder. Beyond 48 entries in a folder, some are not listed (the first ones read on the card are kept, then
sorted): make sub-folders.

### 6.3 The examples of the repository

[`docs/sd/`](../sd) holds an example card to copy at the root:
- `docs/sd/SONNERIES/classique.txt` and `exemples.rtttl`: public domain tunes and examples of the format;
- `docs/sd/LIVRES/tresor_cigalon.txt`: "Le Trésor du capitaine Cigalon", the book built into the badge, a model to
  write your own.

### 6.4 Preparing a card for the event

1. Format in FAT32 (or exFAT), create the folders.
2. Convert the media (§ 2.2); check the books (`gamebook_check.py`) and the ringtones (the badge marks invalid lines
   with `(!)`; `rtttl_sort.py` leaves them out).
3. Put first in the lists (alphabetical order) the files wanted for the demo mode.
4. Insert the card in a badge and run `python tools/badge_media_test.py`: it plays every video and every music and
   lists the failures in `media_report.txt`.
5. Duplicate the card.

> Respect copyright: use free works or works you have the rights to.


## 7. Flashing and updating the firmware

### 7.1 The file

The firmware is a `.uf2` file: `badge_menu.uf2`, given by the organizers (releases of the repository) or built
(`build/src/menu/badge_menu.uf2`, see the [developer guide § 2](developer_guide.md#2-building-and-flashing)).

### 7.2 By drag and drop (no tool)

1. Switch ON.
2. Hold the RP2040 boot button ("BOOTLOAD" / BOOTSEL) of the badge, plug the USB cable, release after one or two
   seconds.
3. A **RPI-RP2** drive appears: copy the `.uf2` file onto it.
4. The badge reboots by itself on the new firmware (the drive disappears).

### 7.3 With picotool

Badge switched on, plugged in, **serial port free** (close `badge_remote.py` and the terminals):

```bash
picotool load -f -x badge_menu.uf2
```

`-f` reboots the badge in flash mode, `-x` starts the firmware after writing. If picotool does not find the badge,
use the button (§ 7.2).

### 7.4 After the update

- Check the version: Settings > Info ("Version 1.0.0 (5d95da4)", build date), or `!` on the console.
- If the new version brings the radio tuning, or if the settings were initialized, the Radio tuning page opens by
  itself at boot (about 15 s): let it finish near other badges switched on.

### 7.5 What survives a flash

The settings are in the last sectors of the flash, which flashing a firmware does not write:

| Data | After a normal flash |
|---|---|
| Name, scores, meetings, records, flags, achievements, cargo, gamebook in progress, settings (screensaver, volume, lamp, remote, **mute mode**, **admin mode**, badge type, **sleep**), radio tuning (crystal, noise, frequency offset) | kept, unless the new version changes the format of the main storage: everything is then reset (`store: initialized`) and the radio tuning starts again |
| Contact cards (yours and the received ones), admin announcements | kept, unless the format of this second storage changes (cleared) |
| Battery calibration (factory setting) | always kept |
| Everything | only cleared by a full flash erase (for example `flash_nuke.uf2`) |

### 7.6 Factory settings

- **Radio tuning** (Settings > Radio tuning): crystal (26 or 27 MHz), noise of the place (remote listening
  threshold = noise + 15 dB, between −95 and −70 dBm), frequency offset from the other badges heard for 10 s (at
  least 2 packets, otherwise no correction). Done at the first boot; redo it at the venue if the remotes or the
  badges are heard badly, near other badges switched on. An interrupted tuning starts again at the next boot.
- **Battery calibration**: § 4.3. Never cleared by the badge.


## 8. Badge language

The badge speaks **French** or **English** (other languages are added without touching the code): all the details
are in [translation](translation.md).

| Action | How |
|---|---|
| Choose the language | Settings > Language ("Langue / Language"): sides to choose, R to confirm |
| Back to English from any language | left wing (L) **5 times** (back to the main menu), then the left wing **held 5 s** |
| From the serial port | `E`: English; `N`: next language (log `i18n: language en (English)`) |
| Screenshots of a language | `python tools/badge_screens.py --lang en`: `docs/screens/en/` and `docs/en/screens.md`, then the badge goes back to its language |
| Add / fix a translation | `python tools/i18n.py update`, translate `src/menu/lang/<code>.po`, `python tools/i18n.py gen`, build |
| Check | `python tools/i18n.py check` (untranslated texts, formats, characters of the fonts) |

The language is kept in the memory of the badge (it survives a firmware update and the resets of the Admin
menu). The serial port logs are not translated: the PC tools work whatever the language.


## 9. Troubleshooting

| Problem | Fix |
|---|---|
| The badge does not start | Switch ON; battery charged (plug in USB). |
| The badge does not show up over USB | Switch ON; data cable (not a charge-only cable); try another USB port. |
| "cannot open COM9", "used by another application", picotool does not find the badge | The port is open elsewhere: close `badge_remote.py`, the terminal, the other script. One program at a time. |
| A script finds the wrong badge | Several badges plugged in: give `--port` (`--ports` for the two-badge tests). |
| "the badge does not answer: is the menu application (badge_menu) flashed?" | The badge runs a test application or another firmware: flash `badge_menu.uf2`. |
| Nothing shows in the terminal | Normal: the badge only talks on events. Type `!`. The lines sent before the port was opened are lost. |
| The badge shows "SLEEP" (SOMMEIL) | Put to sleep by an admin: left side 5 times then right side 5 times (§ 5.3). Neither a reboot nor a flash wakes it up. |
| No sound and no LEDs | Mute mode (command during a talk): Settings > Mute mode, or command `0x03`. |
| The Admin theme appeared by itself | The side sequence was typed by chance: Admin > Leave admin mode. |
| The Admin theme disappeared after a test | The test scripts switch the admin mode off at the end: switch it on again (§ 4.1). |
| The badge does not obey the Flipper | Settings > Remote: yes; a `.sub` file (not `subghz tx`); code `0xC16Axx`; button held for a second; badge close. `O` on the console shows the decoding attempts. |
| The badges hear each other badly, the remotes are badly received | Settings > Radio tuning on site, near other badges switched on. `!`: compare "crystal used" and "measured". |
| The other cicadas are no longer heard | Normal while a page uses the radio: 433 MHz decoder, weather station, 433 MHz hunt, talk badge, contact exchange, pirate radio. |
| No battery level | Battery not calibrated: § 4.3. |
| "No SD card" (Pas de carte SD), `video: mount failed` | Card fully inserted, FAT32 or exFAT. |
| A file does not show up | Right folder and extension; name of 63 bytes at most; no more than 48 entries in the folder; file not hidden. |
| A ringtone is marked `(!)` | Right wing: line, column and reason; the console prints `rtttl: ... error ...` ([ringtones.md](ringtones.md)). |
| A gamebook does not show up or stops | `python tools/gamebook_check.py LIVRES/book.txt`; 16 books at most. |
| A video or a music stops before its end | `badge_media_test.py` to find it; convert it again with `video2epaper.py` / `audio2wav.py`. |
| Sound too low | Media > Volume; WAV converted by `audio2wav.py` without `--no-filter`. |
| `score_check.py` says every score is invalid | Wrong key, or key of another firmware version (`--make-key` changes the key). |
| Assassin "Too far or absent", smuggler with no cicada "Within reach" | Put the badges against each other. The RSSI thresholds are set at build time (developers). |
| `test_werewolf.py` interrupted, games with robots | Admin > Werewolf (admin): set "8 players minimum" again. |
| The screen keeps ghosts | Normal after fast refreshes: it is cleaned at the next full refresh. |
| The badge no longer answers | RESET button, or switch off / on; `R` on the console. |

For development problems (build, radio, lost packets): [developer guide § 11](developer_guide.md#11-development-troubleshooting).
