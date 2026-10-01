# SecSea badge — developer guide

This guide is for anyone who wants to build, modify or extend the badge software.
It covers the architecture, the mechanisms involved, the file formats, the protocols and the tests.
For using the badge, see the [user guide](user_guide.md).
SDK installation is detailed in the [README](../../README.md).

*Version française : [guide développeur](../fr/guide_developpeur.md).*


## 1. Hardware

| Item | Component | Connection (GPIO) |
|---|---|---|
| Microcontroller | RP2040, 16 MB W25Q128 flash | — |
| Display | 1.54" 200×200 e-Paper, SSD1681 controller | SPI0: SCK 6, MOSI 7, MISO 4; DC 8, BUSY 9, RST 10 |
| SD card | soldered micro-SD socket | shared SPI0, CS driven manually |
| SPI0 select | 74HC139 decoder | A0/A1 = 18/19, CSn = 5 |
| Radio | CC1101 433 MHz | SPI1: SCK 26, MOSI 27, MISO 24, CS 25; GDO0/GDO2 |
| Buttons | 4 push buttons | Y (left flank) 0, A (left wing) 1, B (right wing) 14, X (right flank) 15 |
| LEDs | 2 × WS2812 (RGB) | 11 (PIO) |
| Buzzer | through a 2N7002 transistor | 28 (PWM) |
| Battery | TP4056 (charger), 100k/200k divider + LM321 | 29 (ADC3) |
| Extensions | 2 × 2×6 headers | right port J2 (IR), left port J3 (I2C1: SDA 2, SCL 3) |

Pin assignments are in [src/pinouts.h](../../src/pinouts.h) and the board definition in [src/badge_secsea.h](../../src/badge_secsea.h)
(`-DPICO_BOARD=badge_secsea`). The schematics are in [hardware/](../../hardware).

Things to watch out for:
- **Radio crystal**: the original code assumed 26.998 MHz; the badges we measured have a 26 MHz crystal.
  The firmware measures it at boot and snaps to the closest nominal value (26 or 27 MHz),
  since the measurement itself varies by about 500 ppm. The automatic tuning of the radio (§ 6.21) then corrects the
  small frequency offset between the badges.
- **Battery measurement**: the LM321 is powered from the battery itself. Its input and output cannot go above
  roughly V_bat − 1.5 V, whereas the divider feeds it 2/3 V_bat: it saturates.
  The measurement must therefore be calibrated (see [§ 6.9](#69-battery)).
  For a future board revision: use a rail-to-rail op-amp (MCP6001...) or connect the divider straight to the ADC.
- **Power switch**: in the OFF position the badge is not powered, even over USB (only charging works).


## 2. Building and flashing

Prerequisites: Pico SDK 2.x (through the "Raspberry Pi Pico" VS Code extension or `pico_setup.sh`, see the README),
Python 3 with Pillow (images are converted at build time).

```bash
mkdir build && cd build
cmake .. -G Ninja -DPICO_BOARD=badge_secsea
ninja badge_menu            # the main application: build/src/menu/badge_menu.uf2
```

On Windows, if `python3` is only the Microsoft Store alias, point CMake to the interpreter explicitly:
`-DPython3_EXECUTABLE=C:/.../python.exe`.

To flash:
- **with picotool**, badge switched on and plugged in: `picotool load -f -x build/src/menu/badge_menu.uf2`
  (`-f` reboots the badge into flash mode, `-x` starts the application);
- **without picotool**: hold BOOTLOADER while plugging in the badge, then copy the `.uf2` to the "RPI-RP2" drive that shows up.

Other executables: the per-module test applications (`src/tests/*.c`, targets `test_screen`, `test_radio`...).

**Firmware version**: `BADGE_VERSION` in [src/menu/CMakeLists.txt](../../src/menu/CMakeLists.txt), to raise for
each version installed on the badges. At each build, [version.cmake](../../src/menu/version.cmake) writes
`version.h` (in the build folder) with this number, the short git commit and the date; a "+" after the commit
means sources of `src/` or `tools/` changed since (`sans-git` when git does not answer). The badge shows it in
Réglages > Infos ("Version 1.0.0 (5d95da4)" and "Compilée le 2026-09-30"), at boot and with `!` on the serial port
(`version: 1.0.0 (5d95da4 2026-09-30)`).


## 3. Repository layout

```
src/
├── pinouts.h, badge_*.h   pin assignments and board definitions
├── screen/     SSD1681 e-Paper display driver (waveforms, windows, fast refresh)
├── gfx/        1-bit drawing: rectangles, UTF-8 text, generated fonts (gen_fonts.py)
├── sd/         SPI SD driver + FatFs R0.15 (read-only)
├── audio/      PWM + DMA sound output, WAV player, audio2wav.py
├── video/      .EPV player (image + sound), video2epaper.py
├── images/     image2epi.py (.EPI images for the SD card)
├── radio/      CC1101 driver, 433 MHz OOK decoders (ookdec.c: remote controls, weather sensors)
├── ir/         infrared: reception, 38 kHz transmission, NEC decoding
├── qrcode/     Project Nayuki's qrcodegen (QR codes of the scores and of the program)
├── oled/       SSD1306 I2C OLED display
├── leds/       WS2812 and animations
├── btns/       buttons
├── music/, noise_gen/   melodies and cicada song
├── log/        switchable logs
├── menu/       main application badge_menu (see § 5)
├── tests/      on-badge test applications + host tests (tests/host)
└── image2epaper.py   conversion of the images embedded in the firmware
tools/
├── badge_remote.py     badge screen on the PC + keyboard control
├── badge_selftest.py   automated badge test over USB
├── badge_screens.py    captures of all the screens and check of the texts (docs/screens, screens.md)
├── badge_media_test.py plays every video and music of the SD card, report media_report.txt
├── score_check.py      checks the score QR codes, leaderboard
├── contacts_export.py  business cards received by the badge → vCard file
├── crypto_ctf_make.py  generates the crypto challenges (solution file: spoilers)
├── ook_sub.py          Flipper Zero .sub files: test remote controls and weather sensors
├── flipper_net_sub.py  Flipper Zero .sub files: packets of the cicada network, "SecSea" preset
├── skills_icons.py     pictograms of the skills → src/menu/skills_icons.h
├── smuggler_icons.py   icons of the goods of the smuggling game → src/menu/smuggler_goods.c
├── gamebook_check.py   checks a gamebook, plays it in the terminal, generates the built-in book
├── test_party_games.py two-badge test: tug of war and assassin
├── test_werewolf.py    two-badge test: werewolf (robots in admin mode)
├── test_smuggler.py    two-badge test: smuggling
└── flipper/            Flipper remote controls: SecSea_general.sub, SecSea_talk.sub (§ 6.13)
docs/           documentation, ideas, PVSR, synchronization of the choir (chorus_sync.md, in French),
                screens of the badge (fr/ecrans.md, en/screens.md and screens/, generated by badge_screens.py),
                detailed features (en/smuggler.md, en/gamebooks.md, en/pirate_radio.md, en/ringtones.md and their
                French originals, fr/loup_garou.md), examples for the SD card (sd/LIVRES, sd/SONNERIES)
hardware/       schematics and KiCad project
```

Each module is a CMake library (`add_library(... INTERFACE)`), linked by the applications that need it,
just like the SDK libraries.


## 4. The golden rule: never block

There is no operating system. `main()` is a loop that calls each module's task;
**no function may block for more than 20 ms** (initialization aside).
Anything that takes time is a state machine, advanced on every loop iteration:

```c
while (true) {
    absolute_time_t now = get_absolute_time();
    uint8_t pressed = buttons_pressed(now);   // button edges (20 ms debounce) + USB keys
    ... button handling depending on the page shown (app), or cur_app->buttons() for an application ...
    radio_tools_task(now); net_task(now); remote_task(now); ook_rx_task(now);
    vote_task(now); infection_task(now); chorus_task(now); ... notifications ...
    social_task(now); party_task(now); smuggler_task(now); tug_service(now); ... battery_task(now);
    achv_task(); store_task(now); ...
    switch (app) { ... video_task(now) / rsvp_task(now) / games_task(now) / cur_app->task(now) / display_task(now) ... }
}
```

Sound, LEDs and infrared reception run on interrupts and DMA, independently of the loop.
Examples of how work is split up:
- the Connect 4 player evaluates one column per loop iteration;
- the display is driven in steps (send, wait for BUSY, finish);
- flash saving is deferred by 5 s.

GPIO interrupts: the SDK only has one GPIO callback per core. Modules therefore use
`gpio_add_raw_irq_handler()` (one handler per pin), so they can coexist without overriding each other.


## 5. The `badge_menu` application

[src/menu/main.c](../../src/menu/main.c) holds the user interface: the `app` state (menu, file browser, player,
game...), the pages drawn into a frame buffer, and the button actions.

| File | Role |
|---|---|
| `main.c` | themed menus (`SUBMENUS`), admin mode, notifications, SD browser, players, settings, screen saver, cicada name, CTF, USB protocol; opens and runs the applications |
| `app.h`, `apps.h`, `apps.c` | the application framework (`app_t`) and the list of the applications of the menus |
| `ui.c` | shared drawing helpers (title, footer, lists, gauge...) and 4-button text editor |
| `display.c` | non-blocking display of a frame buffer (fast or full refresh, display sleep) |
| `games.c` | the mini games, hardware-independent (sound, LEDs, randomness supplied through *hooks*) |
| `puzzles.c`, `puzzles_app.c` | the puzzles (Démineur, 2048, Taquin, Sokoban, Mastermind, Pendu), hardware-independent, and their applications |
| `rsvp.c` | PVSR fast reading ([docs/pvsr.md](../pvsr.md)) |
| `net.c` | radio network layer shared by all the badge-to-badge features (§ 6.12) |
| `social.c` | cicada network (radio beacons, encounters, score, neighbours) |
| `remote.c` | remote commands (admin badge, Flipper Zero), mute mode (§ 6.13) |
| `ook_rx.c`, `radio433.c` | 433 MHz OOK receiver; 433 MHz decoder and weather station pages (§ 6.14) |
| `radio_tools.c` | radio message and carrier, GFSK profile, crystal measurement, frequency correction |
| `radio_tune.c` | automatic tuning of the radio: crystal, noise, frequency (§ 6.21) |
| `messages.c`, `contacts.c`, `vcard.c`, `program.c`, `vote.c` | relayed messages, business cards and their vCard format (§ 6.20), program, votes |
| `hotcold.c`, `infection.c`, `chorus.c` | hot - cold hunt, radar and 433 MHz hunt (§ 6.14), cicada virus, choir (§ 6.15) |
| `ledcast.c`, `announce.c` | LEDs of the cicadas driven by an admin badge (§ 6.22), announcements (§ 6.23) |
| `duel.c`, `battle.c` | rock-paper-scissors and battleship between two badges (§ 6.16) |
| `image_radio.c` | sending and receiving an image over the radio (§ 6.17) |
| `crypto_ctf.c`, `crypto_app.c` | cryptography challenges and their pages (§ 6.18) |
| `lamp.c`, `nametag.c`, `talk.c`, `admin.c` | lamp, name tag, talk badge, radio commands and badge type (admin) |
| `skills.c`, `skills_icons.h` | skills and their pictograms (§ 6.24) |
| `achievements.c` | achievements, XP and level of the cicada (§ 6.24) |
| `party.c` | lobby of the group games (§ 6.25) |
| `tug.c`, `tug_logic.c` | tug of war (§ 6.25) |
| `assassin.c`, `assassin_logic.c` | assassin (§ 6.25) |
| `werewolf.c`, `werewolf_logic.c` | werewolf (§ 6.25, [loup_garou.md](../fr/loup_garou.md), in French) |
| `smuggler.c`, `smuggler_goods.c`, `smuggler_trade.c` | the smuggler cicada (§ 6.26, [smuggler.md](smuggler.md)) |
| `pirate_radio.c` | pirate radio: FM transmission and listening (§ 6.27, [pirate_radio.md](pirate_radio.md)) |
| `rtttl.c`, `rtttl_parse.c` | RTTTL ringtones (§ 6.28, [ringtones.md](ringtones.md)) |
| `gamebook.c`, `gamebook_parse.c`, `gamebook_builtin.c` | gamebooks (§ 6.28, [gamebooks.md](gamebooks.md)) |
| `demo.c` | page of the demo mode (admin), the demo itself is in `main.c` (§ 6.29) |
| `reset.c` | reset of the scores and of the progress (admin) |
| `credits.c` | credits pages |
| `score_code.c` | signed scores shown as a QR code (§ 6.11) |
| `battery.c`, `battcal.c` | battery level, calibration page (admin) |
| `store.c` | settings, scores, progress and business cards in flash, factory settings |
| `ctf.c`, `oled_demo.c`, `screen_demo.c` | CTF, OLED demos, display demo |

### Adding an application

New features are written as **applications** ([app.h](../../src/menu/app.h)): `main.c` opens them from the menus,
gives them the buttons, runs them in the loop and draws their page when they ask for it.
An application is a `const app_t`:

| Field | Role |
|---|---|
| `name` | name in the menus, logged as `ui: <name>` when opened |
| `label` | optional: dynamic text in the menus ("Lampe : 50 %") |
| `start(now)` | when opened |
| `buttons(b, now)` | on each press, release or long press, and while a button is held; `false`: back to the menu |
| `task(now)` | optional: on every loop iteration; `true` when the page must be redrawn |
| `render(fb, now)` | draws the page into the frame buffer (already cleared to white) |
| `calm()` | optional: `false` while the page changes often (no slow full refresh) |
| `stop()` | optional: when leaving (LEDs, sound, radio...) |
| `no_saver` | the screensaver must not start (name tag, radar, decoder...) |
| `owns_leds` | the application drives the LEDs itself, even in mute mode (talk badge) |

The buttons come in an `app_buttons_t`: `pressed` (pressed since the last call), `released_short` (released without
a long press: use it for a button that also has a long press), `long_pressed` (held for `APP_LONG_PRESS_MS` = 800 ms,
reported once), `held` and `held_ms[]` (to repeat while held).
Bits: `UI_BTN_A` (left wing), `UI_BTN_B` (right wing), `UI_BTN_X` (right flank), `UI_BTN_Y` (left flank).

For sound and LEDs, use `app_tone()` and `app_leds()`: they respect the mute mode; `app_leds(0, 0, 0)` gives the LEDs
back to the badge's animation. `app_open(app)` opens an application from a service (a notification).
`app_show_still(render)` leaves the application and shows the page drawn by `render` like the screensaver: the
screen's OTP waveform (no ghost), kept without power; any button goes back to the menu. It can be called from
`start()` (name tag) or from `task()` (end of an image reception).

To add an application:
1. write `foo.c` with `const app_t app_foo = {...}`;
2. add `APP_FOO` to `app_id_t` in [apps.h](../../src/menu/apps.h), and `[APP_FOO] = &app_foo` to `APPS[]`
   ([apps.c](../../src/menu/apps.c));
3. put `M_APP(APP_FOO)` in a theme of `SUBMENUS` (`main.c`);
4. add `foo.c` to [src/menu/CMakeLists.txt](../../src/menu/CMakeLists.txt).

A **service** (radio reception in the background) subscribes to its packet type in an `*_init()` function called by
`main()`, and reports an event through a function polled in the loop (`vote_new()`, `duel_invited()`...):
`notify()` beeps, writes the text at the bottom of the screen and opens the application when in the menus or the
screensaver.

**Achievements**: a new feature can give an achievement ([achievements.h](../../src/menu/achievements.h)):
1. add an `ACHV_*` to `achv_id_t` and its row in `ACHV[]` of `achievements.c`: name, how to get it (shown by the
   page), XP. Platine (`ACHV_ALL`) requires all the achievements placed **before** it;
2. call `achv_unlock(ACHV_FOO)` when it is reached: only once, saved, announced at the bottom of the screen
   ("Succès : ..." or "Niveau n : ... !") with a chime; or `achv_add(ACHV_CNT_FOO, n)` for a counter
   (`achv_counter_t`, 16 at most, `store_t.achv_counters`: trades, kills, group games, wins, book endings,
   ringtones). `achv_add()` only counts: no achievement has a threshold on a counter yet, test it after the call
   (`if (achv_add(...) >= 10) achv_unlock(...)`);
3. for an achievement that comes from the state of the badge (number of encounters...), check it in `achv_task()`,
   called every second by the loop.

The bits of `store_t.achievements` are the index in `achv_id_t` (64 at most): once the badges are handed out, never
reorder nor remove an achievement. An achievement inserted before `ACHV_ALL` takes the bit of Platine (a badge that
had it would then have the new achievement): do it before the badges are handed out, or add the achievement after
`ACHV_ALL` (Platine does not require it).

### Drawing helpers and text editor (`ui.h`)

[ui.h](../../src/menu/ui.h) gives the pages their common look: `ui_title()` (black band), `ui_footer()`,
`ui_list()` (scrolling list), `ui_lines()` / `ui_text()` (centered or left-aligned lines), `ui_fit()`
(text truncated with "..."), `ui_wrapped()` (centered text, cut at the spaces to fit the width, on `max_lines` lines
at most; used by the announcements), `ui_box()`, `ui_gauge()`.

**Check of the texts** (`U` key of the serial port, `ui_check`): `ui_title()`, `ui_footer()`, `ui_fit()`,
`ui_lines()`, `ui_text()`, `ui_wrapped()` and the pages of `main.c` log the texts cut (`uicheck: cut "..."`), too
wide (`uicheck: title too wide (...)`, `footer`, `wrapped text cut`) or drawn under the footer
(`uicheck: under the footer (y ...)`). `tools/badge_screens.py` turns it on while going through all the pages.

The 4-button text editor (`ui_edit_t`, at most 56 characters, `UI_EDIT_MAX`) is used by the business cards, the crypto challenges
and the cure of the virus:
- `ui_edit_start(e, text, length, charset)`: `UI_CHARSET_TEXT` (letters, digits, punctuation for names, e-mails,
  URLs), `UI_CHARSET_PHONE` (digits, +, space), `UI_CHARSET_UPPER` (A-Z, 0-9, space), `UI_CHARSET_LONG` (letters
  with the French accents é è ê à â ç ô î ù û ë ï É È À Ç, digits, more punctuation: text and QR code content of the
  announcements). In the editor, an accented letter takes one cell (one byte, 0x80 + its rank in `ACCENTS`);
  `ui_edit_start()` reads it as UTF-8 and `ui_edit_result()` gives it back as UTF-8 (2 bytes): the destination
  must have the room;
- flanks: `ui_edit_change(e, -1 / +1)` (repeating while held is up to the caller);
- right wing (short press): `ui_edit_move(e, 1)`; left wing: `ui_edit_move(e, -1)`, which returns `false`
  from the first position (cancel);
- right wing (long press): done, `ui_edit_result()` returns the text without the trailing spaces;
- `ui_edit_render()` draws the prompt, the text around the cursor and the instructions;
- the PC keyboard (keyboard mode of `badge_remote.py`: byte 0x02 followed by the character on the USB) **inserts**
  the character at the cursor of the open editor (the rest moves right, the last character is lost when the text is
  full), through `ui_edit_apply_typed()` (Enter: done; Escape: cancel; Backspace: erase). The buttons, on the other
  hand, change the character under the cursor. Only ASCII goes through (0x20 to 0x7E): no accented letter from the
  keyboard.

### Adding a menu entry

For an entry handled directly by `main.c` (the older features):
1. Add a value to `menu_item_t` and its label in `item_label()`.
2. Put it in a `SUBMENUS` theme (at most 24 entries per theme, `items[24]` of `submenu_t`; the number of entries
   `n` is written by hand: Médias 7, Jeux 19, Social 12, Radio & IR 9, Badge 8, Réglages 6, Admin 13).
   The last theme, Admin, is only shown in admin mode (`store_t.admin == STORE_ADMIN_ON`).
3. In `validate()`, run the action or switch to a new `app` state (`app_state_t`).
4. If the state has its own page:
   - handle its buttons in the loop;
   - draw it inside `if (redraw)`;
   - handle going back in `cancel()`.
5. Call `draw_title()` (or `ui_trace()`) so that the page is logged as `ui: <title>` over USB,
   which the automated tests rely on.

An application (above) saves steps 3 to 5.

**Admin mode**: in the main menu, the sequence `ADMIN_SEQUENCE` (`"LLRRLRLR"`, L = left flank, R = right flank)
typed within 8 s (`ADMIN_SEQUENCE_MS`) sets `store_t.admin` to `STORE_ADMIN_ON` and selects the Admin theme;
"Quitter le mode admin" sets it back to 0. On the serial port, the byte 0x01 followed by `A` (on) or `a` (off) does
the same (the "Mode admin" check box of `badge_remote.py`). All three go through `set_admin()` (`main.c`), which
logs `admin: on` / `admin: off` and redraws the menu (the Admin theme disappears if it was open).

### Adding a game

Games ([games.h](../../src/menu/games.h)) depend only on `gfx`. A game provides 5 functions:
- `*_buttons()`;
- `*_task()`;
- `*_render()`;
- a start function;
- an "idle" state for `games_calm()`.

It hooks them into the `switch` statements of the API. High scores live in `store_t.game_records` (0xFFFF = none).
Then add tests in [tests/host/test_games.c](../../src/tests/host/test_games.c).

The puzzles ([puzzles.h](../../src/menu/puzzles.h)) follow the same principle, with the *hooks* of `games.h`:
short presses to move, long presses to act (long left wing: quit), a help page at the start.
Each one becomes an application through the `PUZZLE_APP` macro of [puzzles_app.c](../../src/menu/puzzles_app.c);
their records are in `store_t.puzzle_records`, their tests in
[tests/host/test_puzzles.c](../../src/tests/host/test_puzzles.c).


## 6. Mechanisms

### 6.1 e-Paper display (SSD1681)

- **Waveforms**:
  - `screen_ws_1681_bw`: full black-and-white refresh, about 2 s;
  - `screen_ws_1681_4grays`: 4 gray levels, using both RAMs;
  - `screen_ws_10fps`, `20fps`, `30fps`: fast *multiframe* mode, only the differences are redrawn.
- **Orientation**: data entry decrements X and Y; RAM byte (x, y) is byte (24 − x, 199 − y) of the image.
  `screen_set_image_position()` handles windows, including partially covered bytes.
- **Cleaning**: `screen_clean()` clears the screen with the screen's own full waveform (OTP, command 0x22 0xF7,
  temperature compensated, about 3 s). The custom waveforms are too short to erase the ghosts of the fast refreshes:
  the ghost comes back a while after the image.
- **Screensaver**: cleaning in black then white, then the image **dithered to black and white** (2×2 ordered) drawn
  with the OTP waveform (`screen_show_image_bw_otp()`). Tried on the badge: with the project's 4 grays waveform,
  a big ghost comes back a few seconds after the image (also with EOPT 0x22); with the OTP one, the image holds.
  The image viewer keeps the 4 grays.
- **Deep sleep**: after 20 s without an update, `display.c` puts the display to sleep (as recommended by the manufacturer).
- **Screen copy**: `screen_shot()` returns a copy of what was sent (shadow of both RAMs) and its kind
  (BW, 4G, white, black). This is what the USB protocol sends (`@FB`).
- `display.c`:
  - chains fast refreshes ("new image" RAM + "displayed image" RAM);
  - does a full refresh every 15 updates to clear ghosting;
  - suspends that full refresh while a game is being played (`display_set_periodic_full(false)`).

### 6.2 Shared SPI0: display and SD card

The display and the SD card sit on the same SPI0. A 74HC139 decoder selects the display according to A0/A1;
the SD card CS is driven as a plain GPIO. Only one user at a time:
- the browser and the players wait for `display_is_idle()` before reading the card;
- they call `display_invalidate()` afterwards to force a full refresh.

### 6.3 SD card and FatFs

- FatFs R0.15, read-only, with:
  - long file names, **in UTF-8** (`FF_LFN_UNICODE 2`);
  - exFAT and GPT partitions (`FF_LBA64`);
  - `f_findfirst`;
  - code page 437.
- `sd_mount()` mounts on demand; `sd_list_files()` and `sd_list_dirs()` return sorted lists
  (names shorter than 64 bytes).

### 6.4 Sound

- **Output**:
  - PWM on GPIO28 at about 61 kHz, with the sample value as the duty cycle;
  - samples are pushed by DMA, paced by a DMA timer at the sample rate;
  - an 8192-sample (8-bit) ring buffer is filled by `audio_write()`.
- **Volume**: 0 to 8, 6 by default.
- **Silence**: silent time is accounted for so that the playback position stays accurate.
- `wav.c` reads 8, 16, 24 and 32-bit PCM, 32-bit float (format 3) and `WAVE_FORMAT_EXTENSIBLE` WAV files, mono or
  stereo, from 4 to 192 kHz; above 48 kHz, one sample out of 2 (96 kHz), 3 or 4 is played (decimation).
  Video uses the sound as its clock.
- The output can also drive the radio (`AUDIO_OUT_RADIO`): that is the modulation of the pirate radio (§ 6.27).
- `audio2wav.py` and `video2epaper.py` compress the dynamic range (250 Hz high-pass, 5 kHz low-pass, compressor,
  normalization): the buzzer is inefficient, so the sound has to be loud and midrange-heavy.

### 6.5 Video

- `video_start(path)` opens an `.EPV` file.
- Frames are displayed in fast *multiframe* mode.
- Sound is read from the end of the file, through a second file handle.
- The clock is the audio position: when the display falls behind, frames are skipped.

### 6.6 CC1101 radio

- `radio.c` provides:
  - `radio_reset`;
  - register access;
  - `radio_tx_packet`;
  - output power (`PATABLE`: 0xC0 = +10 dBm, 0x0E = −20 dBm);
  - crystal adjustment (`radio_set_xosc`).
- **Message**: Flipper Zero's 9.99 kbit/s GFSK preset, sync word 0x464C, compatible with
  the *SubGHz chat* application.
- **Cicada network** (`social.c`, see [docs/idees_reseau_extensions_ctf.md](../idees_reseau_extensions_ctf.md)),
  on top of the network layer (§ 6.12):
  - one `NET_BEACON` every 2 s ± 0.5 s at +10 dBm (measured between two badges about 1 m apart: +10 dBm arrives
    around −70 to −83 dBm, −10 dBm around −97 dBm, at the edge of the sensitivity, −20 dBm does not arrive);
  - 22-byte packet: `0xC1`, type 1, identifier (FNV hash of the unique ID), sequence number, score, name
    (8 characters), then the skills (4-byte mask, § 6.24) and the level (1 to 10); a badge with an older firmware
    sends 17 bytes (`BEACON_LEN`), read without skills nor level;
  - an encounter = RSSI ≥ −80 dBm (`SOCIAL_RSSI_CLOSE`, provisional: to calibrate on site with the radar)
    on 3 beacons within 10 s;
  - +10 points for a new badge, +1 for a known badge, at most once per hour;
  - the neighbours heard (`social_neighbours()`: name, RSSI, already met, skills, level) are used by the radar, the
    messages, the game invitations, the skills, the gauge of the assassin and the smuggling game;
  - a close neighbour (same threshold) that shares a skill is announced once per visit
    (`social_event()`: "X aime aussi : ...", X likes it too).

### 6.7 Infrared

- **Reception**: edge interrupts, mark / space durations.
- **Transmission**: 38 kHz carrier via PWM, modulated by an alarm.
- **Decoding**: `ir_decode_nec()` decodes NEC (standard and extended), with ±25 % tolerance.
- Four slots are kept in flash.

### 6.8 Flash storage

`store.c` keeps a `store_t` in the last 4 KB sector of the flash:
- name, score and encounters;
- CTF flags;
- IR signals;
- settings (reading speed and position, screen saver);
- game high scores;
- fields added later: `lamp_percent` (brightness of the lamp), `badge_type` (`STORE_TYPE_PARTICIPANT`, `SPEAKER`,
  `STAFF`), `admin` (`STORE_ADMIN_ON` = 0xA5: admin mode), `remote_off` (1: remote commands ignored),
  `muted` (1: mute mode), `infection` (state of the virus), `crypto_solved` (bit n: challenge n solved),
  `puzzle_records` (records of the puzzles), `radio_tuned` (`STORE_RADIO_TUNED` = 0xA5: radio tuned),
  `radio_noise_dbm` (noise measured, dBm), `radio_freq_offset` (frequency correction, FSCTRL0) (§ 6.21);
- the **second generation** of fields, valid when `v2_magic` is `STORE_V2_MAGIC` (0x5A): `skills` (bit n: skill n
  ticked), `achievements` (64 bits, bit n: achievement n), `achv_counters[16]` (counters of the achievements),
  `book_hash` and `book_section` (gamebook being read and its section), `cargo[32]` (the hold of the smuggling game)
  and `cargo_seeded`. When `v2_magic` differs (sector of an earlier version: 0xFF), `store_init()` sets these fields
  to 0 and writes `v2_magic`: otherwise an erased flash (0xFF) would mean "all the achievements".

How it works:
- the write happens 5 s after the last change (`store_changed()`), using `flash_safe_execute` (about 50 ms,
  interrupts disabled);
- `store_save_now()` writes at once: for a change that must not be lost if the badge is switched off within the 5 s
  (the hold after a smuggling trade: otherwise a good could be duplicated);
- new fields are appended **at the end** of the structure: in a sector written by an earlier version,
  they read as 0xFF, which their users check for (or, for a group of fields, a magic byte like `v2_magic`);
- a `_Static_assert` guarantees that the structure fits in the sector.

The business cards do not fit in that sector: they live in a second store, `store_ext_t`, of 8 KB (two sectors)
right before the first one (`STORE_OFFSET - 8192`), with its own header (magic `"CONT"`, version):
the mask of the fields sent, my card and the 12 cards received (`STORE_CONTACTS`), 512 bytes each with fixed fields
(`CONTACT_BYTES`). It is reset when the header does not match: the move to 512-byte cards (`STORE_EXT_VERSION` 2)
therefore erases, on update, the cards received **and** my card (the mask goes back to first name + name).
`store_ext_changed()` writes it the same way, 5 s later.
The admin announcements (§ 6.23) were added later at the end of `store_ext_t`, without changing the version:
`announce_magic` then 6 `store_announce_t` (`STORE_ANNOUNCES`: time 6 bytes, text 112, type of the QR code,
content 64), valid when `announce_magic` is `STORE_ANNOUNCE_MAGIC` (`"ANNO"`); otherwise `announce_list()` puts the
default announcements there. Then `contact_skills[12]`: the skills of each card received (§ 6.20), moved along with
the cards.

The third store, the **factory settings** (`store_factory_t`, magic `"FACT"`), has its own sector just before
`store_ext_t`: never erased by the Remise à zéro, a version change or a new firmware; it keeps the calibration of the
battery (§ 6.9) and is written at once (`store_factory_save()`).

### 6.9 Battery

- The raw reading (ADC3, 16 samples, filtered) is only converted to a voltage with a **two-point calibration**
  (linear between the points, which must be at least `BATTERY_CAL_MIN_RAW` = 150 ADC steps apart).
- Without calibration nothing is displayed: the badge must never show a wrong value.
- The points are set on the badge: Admin > Batterie (calibration) (battcal.c, see the user guide § 5).
  `battery_set_point(mv, raw)` sets the first point, the second one, or replaces the nearest one.
- They are **factory settings**: `store_factory_t` (magic `"FACT"`), in its own flash sector just before the
  second store. It is written at once (`store_factory_save()`) and never erased by the Remise à zéro, a new
  store version or a new firmware (picotool only writes the sectors of the program).
- Fallback for a badge with no factory points: `BATTERY_CAL_RAW1/MV1/RAW2/MV2` at build time
  ([battery.h](../../src/menu/battery.h)).
- The `!` diagnostic prints the measure and `battery: factory points <raw> = <mV>, <raw> = <mV>`.

### 6.10 Text and fonts

- `gfx_text()` draws UTF-8 text.
- Fonts generated by `gen_fonts.py` (Aileron):
  - ASCII and French accented letters;
  - advance widths in 1/16 pixel, for even spacing.
- An accented letter missing from the font is drawn without its accent; any other unknown character becomes "?".
- `gfx_set_size()` allows drawing for the 128×64 OLED.


### 6.11 Signed scores (QR code)

- At the end of a game, `games.c` builds the text `HIP26:<game>:<score>:<badge id>:<name>:<signature>`:
  - games: `MORPION`, `P4` (wins-losses-draws of the session), `SIMON`, `REFLEX` (average in ms), `SNAKE`;
  - from the menu (long press on a game, `games_show_record()`), it is the record: best score, best average,
    or total wins against the cicada (`12V`) for Morpion and Puissance 4;
  - the id is the one of the network of the cicadas (hash of the RP2040 unique id).
- The signature is a 64-bit SipHash-2-4 of the text before it, with a 128-bit key
  ([score_code.c](../../src/menu/score_code.c)). The key is not stored in clear: it is masked by a xorshift stream,
  rebuilt on the stack for the computation then wiped. Someone reading the firmware can recover it: this protects
  against hand-made cheating and is a challenge for the curious, not a strong cryptographic secret.
- The QR code comes from Project Nayuki's [qrcodegen](../../src/qrcode/qrcodegen.h) library (MIT license, no allocation),
  error correction level M, version 6 at most, 4 pixels per module when it fits.
- `tools/score_check.py` checks the scanned texts and ranks them (best score of each badge, a QR code scanned twice
  counts once). The key is given with `--key` or the `BADGE_SCORE_KEY` environment variable;
  `--make-key` generates a new key and the masked table to paste into `score_code.c`.


### 6.12 The badge radio network (`net.c`)

All the features that talk to other badges share the CC1101 through [net.h](../../src/menu/net.h):
- **Profile**: GFSK 9.99 kbit/s (the Flipper preset), 433.92 MHz, sync word 0xC16A, CC1101 CRC.
- **Packet**: `[0xC1][type][sender id, 4 bytes little endian][data, at most 55 bytes]`,
  61 bytes at most (the RX FIFO minus the length byte and the two status bytes).
  The id is `net_id()`, an FNV-1a hash of the unique id of the flash.
- **Reception**: the badge listens all the time. `net_subscribe(type, handler)` (one handler per type) gets a
  `net_packet_t`: type, sender, data, length, RSSI in dBm, reception time (end of the packet).
  The badge's own packets are ignored.
- **Transmission**: `net_send(type, data, length, flags)` queues the packet (queue of 8); it is sent when the channel
  is free (no reception going on, GDO0 low). `false` when the queue is full or the data too long.
- **Flags**:

  | Flag | Effect |
  |---|---|
  | `NET_QUIET` | −20 dBm: ~−107 dBm at 1 m, below the sensitivity (unused) |
  | `NET_MEDIUM` | −10 dBm: ~−97 dBm at 1 m, at the edge of the sensitivity (unused) |
  | `NET_LOUD` | +10 dBm: ~−70 to −83 dBm at 1 m; used by every feature (proximity is judged on the RSSI) |
  | `NET_JITTER` | random delay (up to 300 ms) before sending: when many badges answer the same packet |

- **Sharing the radio**: the other features (message to the Flipper, carrier, OOK receiver) take the radio;
  the network pauses (`net_pause()`, `radio_tools_idle()`) and reconfigures the radio afterwards.
- **Chat mode**: `net_set_chat(handler)` switches the radio to the profile of the Flipper Zero's "SubGHz chat"
  (`radio_tools_profile_chat()`: same GFSK 9.99 kbps modem, sync word 0x464C); `net_set_chat(NULL)` goes back to the
  network. The packets are plain text, without header (at most 61 bytes): a Flipper (`subghz chat`) or any CC1101
  reads and writes them. `net_send_text()` sends them at +10 dBm; the handler gets each text with its RSSI.
  In this mode the network hears nothing, `net_send()` refuses the packets and the queue is emptied when the mode
  changes; `net_chat()` tells the mode, `net_queue_free()` the room left in the queue.
  Used by the card exchange (§ 6.20).

| Type | Name | Module | Data |
|---|---|---|---|
| 0x01 | `NET_BEACON` | `social.c` | sequence, score (2 bytes), name (8), skills (4), level |
| 0x02 | `NET_COMMAND` | `remote.c` | command, nonce (2) |
| 0x03 | `NET_MESSAGE` | `messages.c` | uid (2), TTL, origin (4), recipient (4, 0 = everybody), name (8), message number |
| 0x04 | `NET_VOTE_QUESTION` | `vote.c` | session (2), question, open (0 / 1); every 3 s while the question is open |
| 0x05 | `NET_VOTE_ANSWER` | `vote.c` | session (2), question, answer; sent 3 times |
| 0x06 | `NET_GAME` | `duel.c`, `battle.c` | session (2), kind, recipient (4), round or turn, then depending on the kind (§ 6.16) |
| 0x07 | `NET_TRADE` | `smuggler.c` | trade id (4), kind, recipient (4), data (§ 6.26); formerly `NET_CONTACT`, unused since the cards go in chat mode (§ 6.20) |
| 0x08 | `NET_HOTCOLD` | `hotcold.c` | hot - cold beacon, one per second |
| 0x09 | `NET_INFECTION` | `infection.c` | generation (0 = patient zero); a "cough" every 4 to 5 s, at +10 dBm; contagion at RSSI ≥ −80 dBm (provisional) |
| 0x0A | `NET_IMAGE` | `image_radio.c` | transfer (2), block, 48 bytes (§ 6.17) |
| 0x0B | `NET_SONG` | `chorus.c` | song, kind, session (2), ms (4), voices (§ 6.15) |
| 0x0C | `NET_LEDS` | `ledcast.c` | nonce (2), mode, R, G, B, time 1 (2), time 2 (2); sent 5 times (§ 6.22) |
| 0x0D | `NET_ANNOUNCE` | `announce.c` | nonce (2), part, number of parts, at most 48 bytes; the whole 3 times (§ 6.23) |
| 0x0E | `NET_PARTY` | `party.c`, then `tug.c`, `assassin.c`, `werewolf.c` | game, session (2), kind, recipient (4, 0 = everybody), at most 47 bytes (§ 6.25) |
| 0x0F | `NET_PING` | `net.c` | number (`P` key) |

Messages are relayed by flooding: each badge sends a message it has not seen yet (origin + uid) once more,
with the TTL decremented, with `NET_MEDIUM | NET_JITTER`, down to TTL 0 (3 at the start).

**Testing with a single badge**: the `L` key turns on the *loopback* mode: every packet sent comes back as if sent by
a "twin" whose id is `net_id() ^ NET_TWIN` (0x00FF00FF). The badge can then vote on its own question, receive its own
message or infect itself. The cicada network ignores this twin (no encounter).
`V` logs every packet sent and received (in chat mode, the text of the packets received), `P` sends a ping, `!` shows
the counters (sent, received, dropped) and the state of the network (`net state: ...`: paused or not, owner of the
radio, sending, chat mode, queue, number of repairs, MARCSTATE and main registers, GDO0).

**Robustness of the radio**: a badge could become deaf and mute (GDO0 stuck high: the network waited for the end of a
packet that never came).
- **PKTLEN = 61**: `configure()` of [radio_tools.c](../../src/menu/radio_tools.c) limits the length of the packets to
  61 bytes (the RX FIFO minus the length byte and the two status bytes). With the default value (255), a sync word
  found in the noise followed by a big "length" left the radio waiting for the end of a packet that did not exist.
- **Watchdog**: every second (`CHECK_MS`, not while sending), `check_radio()` checks that the radio is in the
  configuration of the network: PKTLEN 61, PKTCTRL0 0x05, MDMCFG2 0x12, IOCFG0 0x06, the sync word of the mode
  (0xC16A, or 0x464C in chat mode) and MARCSTATE between IDLE and RX (neither a FIFO overflow nor sending). GDO0 high
  for more than 300 ms (`GDO0_STUCK_US`; a packet lasts 60 ms at most) is stuck: the pin becomes an input again.
  In all these cases, the radio goes to IDLE, the FIFOs are flushed, the network configures it again on the next
  call and logs `net: radio repaired (...)`; the `repairs` counter is in the `net state:` line of `!`.
- **Full reconfiguration**: when the network takes the radio back (after the OOK receiver, the OOK transmitter, the
  crystal measurement...), it first calls `radio_tools_reconfigure()` (all the registers of the profile), then its
  profile: another user may have left other registers (a radio left in OOK sends nothing usable).
- **Frequency offset**: for each packet received from another badge, the network adds up the FREQEST of the CC1101
  (measured offset, steps of fXOSC / 2^14); `net_freq_offsets(&sum, reset)` gives it to the tuning of the radio
  (§ 6.21), and `net_reconfigure()` makes the network configure the radio again.


### 6.13 Remote commands and mute mode (`remote.c`)

A command arrives in two ways:
- from an **admin badge**: a `NET_COMMAND` packet `[command][nonce 2]`, at +10 dBm, sent 5 times over 2 s
  (`remote_send()`); the same sender + nonce pair is only executed once (for 10 s);
- from a **433 MHz remote control**, a Flipper Zero for instance: a 24-bit Princeton code `0xC16A00 | command`,
  decoded by the OOK receiver (§ 6.14); the same code repeated (button held) is only executed once (1.5 s).

The admin badge first sends the command like a remote: 12 Princeton frames (`OOK_FRAMES`, te = 400 µs, ~0.6 s,
[ook_tx.c](../../src/menu/ook_tx.c): CC1101 in asynchronous serial mode, edges on GDO0 timed by a hardware alarm),
then the network packets. The talk badge, which only listens in OOK, thus gets the admin commands.
A command received both ways is only executed once (same command within 4 s).

| Command | Effect | Handled by |
|---|---|---|
| 0x01 | the cicada sings for 6 s | `remote.c` |
| 0x02 / 0x03 | mute mode / end of the mute mode | `remote.c` |
| 0x10 to 0x14 | lights of the talk badge: off, green, orange, red, angry red | `talk.c` (page open) |
| 0x30 + n | starts song n of the choir | `chorus.c` |

A module handles a group of commands (the high nibble) with `remote_subscribe(group, handler)`;
the handler gets the low nibble. `remote_execute()` executes a local command.
The badge obeys unless Réglages > Télécommande is set to "non" (`store_t.remote_off = 1`).

**Listening to the remote controls**: the CC1101 cannot listen in GFSK and OOK at the same time. There is no blind
listening window any more: the network listens all the time, and `remote_task()` reads the RSSI every 20 ms
(`RSSI_POLL_MS`) when the radio and the network are free. A transmitter above the threshold `remote_trigger_dbm()`
(the noise measured by the tuning of the radio + 15 dB, `OOK_TRIGGER_ABOVE_NOISE`, kept between −95 and −70 dBm;
−90 dBm, `OOK_TRIGGER_DBM`, without tuning: the noise is around −105 dBm) measured twice in a row
(`OOK_TRIGGER_POLLS`) without a network packet on the air (no sync
word recognized, `net_transmitting()`) may be a remote control: an OOK window of 150 ms opens (`OOK_WINDOW_MS`).
It is extended in steps of 100 ms, up to 1.5 s (`OOK_WINDOW_MAX_MS`), while a remote control is sending (at least
30 edges in 100 ms, `OOK_ACTIVE_PULSES`: a Princeton frame gives about 95, the noise and the GFSK packets of the
badges far fewer). After a window, the next one waits at least 800 ms (`OOK_PERIOD_MS`, against a transmitter that
never stops), a time doubled after each window without a code decoded (an interferer), up to 3.2 s
(`OOK_PERIOD_MAX_MS`); a strong signal, −75 dBm or more (`OOK_STRONG_DBM`: a remote control right next to the badge),
opens a window even during this wait. A window also opens every 10 s (`OOK_FORCED_MS`), for a remote control weaker
than the threshold.
Measured between two badges: 97 to 98 % of the pings received (82 to 85 % with 80 ms windows every 800 ms, 73 % with
the former 220 ms windows).
During a window, the network hears nothing. The features that need all the packets (choir, image, card exchange)
suspend the windows with `remote_pause_windows()` (calls are counted), and there are none in chat mode.
The talk badge listens to the remote control all the time: it only receives the admin commands in OOK.
If the radio stays busy, the admin badge gives up the Princeton frames after 2 s (`OOK_GIVE_UP_MS`,
`remote: princeton not sent (radio busy)`) and sends the network packets anyway. `!` logs the state of the listening
(`remote state: ...`: window, pauses, OOK receiver and transmitter, frames pending, sends left, wait between two
windows).

**Buttons of the Flipper's remote**: the Flipper's Sub-GHz application, on a saved Princeton file, sends the code of
the file with OK, and with the arrows the same code with another button in the low nibble: up = 2, down = 4,
left = 8, right = F. `flipper_buttons()` puts on these buttons the commands missing from a group, so that a single
file drives the whole group:

| Code received | Command executed |
|---|---|
| 0x04 | 0x03: end of the mute mode |
| 0x18 | 0x10: talk off |
| 0x1F | 0x13: talk red (done) |

The files `tools/flipper/SecSea_general.sub` (`C16A01`: OK cicada, up mute, down end of the mute mode) and
`tools/flipper/SecSea_talk.sub` (`C16A11`: OK green, up orange, right red, down angry red, left off) are 24-bit
Princeton keys, te = 400 µs, preset `FuriHalSubGhzPresetOok650Async`.

**Mute mode** (`store_t.muted`): `audio_set_mute()` keeps playing the samples at level 0 (the players keep their
clock), `set_leds()` and the LEDs of the games stay off, `app_tone()` and `app_leds()` respect it.
An `owns_leds` application (the talk badge) drives its LEDs and its buzzer even in mute mode. In the "STOP !" state
(angry red), the talk badge makes the cicada sing (`noise_gen_set_enabled(true)` after `audio_close()`: the noise
generator drives the buzzer at full swing) instead of a beep.

**From a Flipper Zero**: the `subghz tx` command of the Flipper's command line does not send the key as is (it
replaces the low nibble with 6);
use a `.sub` file (§ 6.19) or Sub-GHz > Add Manually > Princeton_433, then edit the `Key:` line.


### 6.14 OOK receiver and 433 MHz decoders

- [ook_rx.c](../../src/menu/ook_rx.c) puts the CC1101 in asynchronous OOK reception (registers of the Flipper's
  "AM650" preset, 650 kHz bandwidth): the demodulated signal comes out on GDO0. An interrupt
  (`gpio_add_raw_irq_handler()`) measures the durations between edges into a ring of 1024; the main loop splits the
  transmissions (50 ms of silence) and decodes them with `ookdec_decode()`.
- **Decoding in parts**: when the ring fills the decoder without a silence (long transmission, noise), it is decoded
  in parts, with an overlap of 120 durations (`OVERLAP`, more than two Princeton frames): a frame across the cut is
  not lost. A part that decodes nothing (200 durations at most, `RETAIN_MAX`) is kept and decoded again with the next
  one, if it comes within 400 ms (`RETAIN_US`): the Flipper sometimes leaves more than 50 ms between two frames, and a
  single frame is not enough (the code must be seen twice). Checked: 10 codes out of 10 sent by the Flipper decoded,
  against 2 out of 8 before. The `O` key logs the decoding attempts.
- **Radio lent**: when another feature used the radio during a listening (message, carrier), `radio_tools.c` calls
  `ook_rx_resume()`, which writes the OOK registers again and restarts the reception.
- Several users can listen at the same time (`ook_rx_start()` / `ook_rx_stop()`, counted): the remote control windows,
  the decoder and weather station pages, the talk badge. The network is paused while listening.
  Princeton codes are passed to `remote_princeton()`, the pages read the last frame with `ook_rx_get()`.
- [ookdec.c](../../src/radio/ookdec.c) does not depend on any hardware (tested on the PC): Princeton remote controls
  (PT2262, EV1527...), CAME 12/24 bits, Nice FLO 12/24 bits; Nexus-TH, inFactory-TH, ThermoPRO-TX4, GT-WT02,
  LaCrosse TX141TH-Bv2, Acurite 592TXR thermometers. The protocols without checksum must be received twice identically,
  like the Flipper does; glitches shorter than 80 µs are merged. Known ambiguity: a ThermoPRO frame has a valid
  GT-WT02 checksum about once in 300.
- [radio433.c](../../src/menu/radio433.c): the 433 MHz decoder page (last 8 frames) and the weather station page
  (last measure of up to 4 sensors). Receive only.
- **Power of the frames**: while the carrier is present (GDO0 high), `ook_rx_task()` reads the RSSI and keeps the
  maximum; this peak goes with the next decoded frame, `ook_rx_last_rssi()` (−128: unknown). The peak of a signal
  that decodes nothing (noise) is forgotten.
- **433 MHz hunt** (`app_hunt433`, [hotcold.c](../../src/menu/hotcold.c)): the OOK receiver listens all the time
  (`ook_rx_start()`); the page lists the codes heard (8 at most, `TARGETS`: protocol, code, dBm of the last frame,
  number of frames), then follows the one chosen with the view, the LEDs and the beeps of the hot - cold between
  badges, on the power of each of its frames. It is lost after 15 s without a frame (`TARGET_LOST_MS`; 5 s for the
  beacon of a badge). Log: `hunt433: <frame> at <n> dBm`.
- **Back to GFSK**: the OOK receiver changes FREND0, FREND1 and MDMCFG0, which the GFSK preset leaves at their reset
  values. With FREND0 = 0x11, the packets would be sent with `PATABLE[1]`, without power: the GFSK configuration of
  `radio_tools.c` writes these registers back.


### 6.15 Choir of the cicadas

The protocol is detailed in [docs/chorus_sync.md](../chorus_sync.md) (in French). In short:
- the leader broadcasts "start in 2000 ms" (`NET_SONG`, kind 1) three times, computing the remaining delay again:
  each badge schedules the start at "reception + delay" (RBS principle: everybody receives the same packet at the same time);
- during the song, the leader broadcasts its position every 2 s (kind 2): a badge that arrives late joins the choir
  at that position; at the end, a stop (kind 3);
- the leader sings voice 1, the others share the other voices according to their id;
- the songs are in the firmware (public domain); the notes are synthesized as square waves at 16 kHz,
  just in time (100 ms ahead);
- two competing starts: the earliest wins, then the smallest leader id.


### 6.16 Two-badge games: no cheating

Duel ([duel.c](../../src/menu/duel.c)) and battleship ([battle.c](../../src/menu/battle.c)) share the `NET_GAME` type:
`[session 2][kind][recipient 4][round or turn][...]`, kinds 1 to 5 for the duel, 10 and more for the battleship
(`duel.c` passes them to `battle_handle()`). A neighbour is invited (list of the beacons heard);
the current message is sent again every 500 ms until the other badge moves on,
and the game is lost after 20 s (duel) or 60 s (battleship) without news.

- **Duel: commit then reveal**. Each round, a badge first sends the commitment `[hash 4]`: a 32-bit FNV-1a of the
  choice, the round, the session, a 32-bit random number and its id. It only reveals `[choice][nonce 4]` after it
  received the other's commitment, and the other checks the reveal. Nobody can wait for the other's choice.
- **Battleship: fleet hash**. At the start, each badge sends the hash of its fleet (36 bits, one per cell) with a random
  number and its id. During the game, it answers "hit" or "miss" to each shot. At the end, it reveals its fleet and
  the random number (`[fleet 5][nonce 4]`, repeated for 10 s); the other one checks the hash, the 7 ship cells and each
  answer it received: "Flotte OK" (fleet OK) or "TRICHE !" (cheat!).

A 32-bit hash protects against improvised cheating; it is not a cryptographic proof.


### 6.17 Image over the radio

[image_radio.c](../../src/menu/image_radio.c) sends an image of the gallery (`.EPI` in the `IMAGES` folder)
or the built-in SecSea image:
- the image is dithered to black and white, 1 bit per pixel: 5000 bytes (1 = white);
- cut into 105 blocks of 48 bytes (the last one padded), plus 14 parity blocks: the XOR of each group of 8 blocks.
  A block lost in a group is rebuilt from the 7 others and the parity, without asking again;
- `NET_IMAGE` packet `[transfer 2][block][48 bytes]`, blocks 0 to 104 for the image, 105 to 118 for the parities;
  one block every 70 ms at +10 dBm (a block lasts about 53 ms on the air: the queue keeps room for the other
  features), and the whole sequence **twice** (a block missed the first time arrives the second), about 17 s;
- the receiver only shows the progress (number of blocks and gauge, drawn again every 25 blocks: each fast refresh
  leaves a bit of ghost); the complete image is shown by `app_show_still()`, like the screensaver (OTP waveform,
  no ghost), with the number of blocks corrected. The remote control windows are suspended while sending and
  receiving.


### 6.18 Crypto challenges

[crypto_ctf.c](../../src/menu/crypto_ctf.c) holds 13 challenges, from the easiest to the hardest (texts in French).
Their texts are generated by `tools/crypto_ctf_make.py`, **the solution file** (answers in clear: do not give it to
the players), which computes the ciphertexts, checks that everything fits on the screen (7 lines of text, 3 of hint)
and replaces the table between the `GENERATED` markers (`--update`; `--answers` prints the answers).
The firmware only keeps the SipHash-2-4 of the normalized answers (upper case, spaces) and the pieces of the final
flag, masked by a SipHash stream. The progress is in `store_t.crypto_solved`.
The "Ultrason" challenge (`ultrasound` in the table) plays its Morse at 19 kHz with `audio_pwm_tone()`: a square wave
straight from the PWM (the 16 kHz audio player cannot go above 8 kHz), without the LEDs.


### 6.19 Flipper Zero

- **Test remote controls and sensors**: `tools/ook_sub.py` writes Sub-GHz RAW files (`.sub`, preset
  `FuriHalSubGhzPresetOok650Async`) with the same timings as `ookdec.c`:

  ```bash
  python tools/ook_sub.py princeton 0xC16A02 -o mute.sub      # command 0x02 (mute mode)
  python tools/ook_sub.py came 0x5A1 --bits 24
  python tools/ook_sub.py weather --temp 21.5 --hum 45 --channel 2 -o weather   # one file per protocol
  python tools/ook_sub.py check file.sub                      # checks the format
  ```

- **Packets of the cicada network**: `tools/flipper_net_sub.py` writes RAW files with a custom GFSK preset
  (`FuriHalSubGhzPresetCustom` and its `Custom_preset_data` line). The script computes the bits as the CC1101 sends
  them: 4 bytes of 0xAA preamble, the sync word 0xC1 0x6A, the length, the packet, then the CC1101 CRC-16;
  the Flipper sends them as asynchronous GFSK.

  ```bash
  python tools/flipper_net_sub.py command 0x02 -o mute_gfsk.sub   # remote command (NET_COMMAND)
  python tools/flipper_net_sub.py ping -o ping.sub
  python tools/flipper_net_sub.py raw 0x0F 01 -o packet.sub       # type, then the data bytes in hex
  ```

- **Recording the packets of a badge with the Flipper**: the Flipper does not know the GFSK profile of the badges.
  `python tools/flipper_net_sub.py preset` prints the "SecSea" preset (the same `Custom_preset_data` string) to add to
  the `subghz/assets/setting_user` file of the Flipper's SD card; Read RAW with this preset then records the packets
  (an image transfer for instance), which can be replayed.
- **Sending a file from the PC** through the Flipper's command line (USB serial port):
  `storage write_chunk /ext/subghz/<file>.sub <size>` then the bytes of the file, and
  `subghz tx_from_file /ext/subghz/<file>.sub 1 0` (1 repeat, internal radio).
  Do not use `subghz tx` for the Princeton codes: the key is not sent as is (low nibble replaced with 6).
- **Ready-made remote controls**: `tools/flipper/SecSea_general.sub` and `SecSea_talk.sub` (§ 6.13).
- **Business cards**: `subghz chat 433920000 0` reads the cards sent by the badges in exchange mode, and can send
  one by typing its lines (§ 6.20).


### 6.20 Business cards (vCard in chat mode)

[contacts.c](../../src/menu/contacts.c) exchanges the cards in clear, on the profile of the Flipper's chat (chat mode,
§ 6.12); [vcard.c](../../src/menu/vcard.c) formats and reads them, without depending on the hardware (tested on the PC):
- **Format**: a vCard 3.0 (RFC 6350), one line per text packet, ended by `\r\n`, at most 60 bytes
  (`VCARD_PACKET_MAX`); a longer line is folded: the continuation packets start with a space.
  Properties: `N`, `FN`, `ADR`, `TEL`, `EMAIL`, `ORG`, `TITLE`, `URL;TYPE=linkedin`, `URL;TYPE=git`, `URL`,
  `X-MASTODON`, `NOTE`; only the ticked fields are sent. Then `CATEGORIES`: the ticked skills (`skills_to_text()`:
  "Électronique,Flipper Zero"), as soon as there is one; on reception, `skills_from_text()` finds them again
  (ignoring the case, the accents, the spaces and "/ - .", unknown names are ignored) and they are kept in
  `store_ext_t.contact_skills`; `contacts_export()` (`k` key) gives them back as `CATEGORIES`.
- **Check**: before `END:VCARD`, a line `X-SECSEA-CHECK:<lines>-<crc>` (an extension allowed by the standard): the
  number of lines from `BEGIN:VCARD` and the CRC-16/CCITT-FALSE (hex) of these lines joined by `\n`. A card with a
  line lost or mixed with those of another card is rejected: it will arrive with the next sending. A card without
  this line (typed on a Flipper, `name: LINE`) is accepted as it is; the name prefix and the color codes of the
  Flipper's chat are removed.
- **Sending**: the whole card is sent again every 2.5 s plus a random delay of up to one second (two badges do not
  stay in step), one line every 70 ms. After each chat packet heard, the next sending of the card waits 2 s: a radio
  hears nothing while it sends (half duplex). The same card received again (same CRC) is offered only once.
- **Sizes**: 512 bytes per card (`CONTACT_BYTES`); e-mail 47 characters, LinkedIn, Git and web site 55, Mastodon 47
  (sizes of `FIELDS`, final 0 included); the text editor goes up to 56 characters (`UI_EDIT_MAX`).
- **Privacy**: during the exchange, any Flipper in range in chat mode reads the card; only the ticked fields are
  sent. The former `NET_CONTACT` type (0x07) is now used by the smuggling game (`NET_TRADE`).


### 6.21 Automatic tuning of the radio

[radio_tune.c](../../src/menu/radio_tune.c) (Réglages > Réglage radio) opens by itself at boot as long as
`store_t.radio_tuned` is not `STORE_RADIO_TUNED` (0xA5): first start, or an update that brings it (the field then
reads 0xFF). A tuning that was stopped therefore starts again at the next boot. The remote control windows are
suspended during the measures (the network listens all the time). Three steps:
1. **Crystal**: the measure of `radio_tools.c` (`radio_tools_measure_xosc()`, 26 or 27 MHz).
2. **Noise**: 150 readings of the RSSI over 3 s (`NOISE_MS`, one every 20 ms) while no packet is being received; the
   median is the noise (`radio_noise_dbm`). The listening threshold of the remote controls becomes
   `remote_trigger_dbm()` = noise + 15 dB, kept between −95 and −70 dBm (§ 6.13): more sensitive in a quiet place,
   not fooled in a noisy one.
3. **Frequency**: for 10 s (`FREQ_MS`), the FREQEST of the packets of the other badges (`net_freq_offsets()`).
   With at least 2 packets (`FREQ_MIN_PACKETS`), **half** of the mean offset is added to the correction (FSCTRL0,
   steps of fXOSC / 2^14, about 1.6 kHz; for sending and receiving): two badges tuned at the same time meet instead
   of crossing. A mean offset of more than 60 steps (`FREQ_MAX_STEPS`, about 95 kHz) is a wrong measure: ignored;
   the correction stays between −60 and +60.

At the end, `radio_tuned`, `radio_noise_dbm` and `radio_freq_offset` are saved, `radio_tools_set_freq_offset()`
applies the correction and `net_reconfigure()` makes the radio configured again; `radio_tools_init()` reads it back
at boot. Log: `tune: crystal ... Hz, noise ... dBm (remotes above ... dBm), frequency offset <before> -> <after>
(<n> packets, mean <m>)`. The page shows the steps with a gauge (footer "G : arrêter"), then the crystal, the noise,
the threshold of the remote controls, the correction and the number of packets heard ("D : régler  G : retour").


### 6.22 LEDs of the cicadas (admin badge)

[ledcast.c](../../src/menu/ledcast.c) (Admin > LEDs des cigales): a colour (list of 9 colours, or R, G, B from 0 to
255) and a mode: `Fixe` (steady), `Clignotant` (blinking, times on / off) or `Fondu` (fading, to the colour / to
black), times from 50 ms to 5 s in steps of 50 ms.
- **Packet** `NET_LEDS`: `[nonce 2][mode][r][g][b][time 1, ms, u16 LE][time 2, ms, u16 LE]`, at +10 dBm, sent 5 times
  over 2 s (every 450 ms). Mode 0: "Rétablir" (restore). The same sender + nonce is only applied once; the times
  received are kept between 50 ms and 5 s. Log: `leds: <mode>, color r g b, times t1 / t2 ms (from <id>)`.
- **Reception**: `ledcast_show()`, called by `set_leds()` of `main.c`, replaces the animation of the badge
  (`leds_anim_fixed()`, `leds_anim_blink()`, `leds_anim_fade()`) until "Rétablir" or a restart (nothing is kept in
  flash). The mute mode still turns the LEDs off (checked first); the main loop does not apply the order received
  during a game or in an `owns_leds` application (talk badge): it applies when they are left.
- **Sending**: the admin badge applies the order to itself. The page is `owns_leds`: it shows the colour and the mode
  chosen on its own LEDs while setting.
- **`leds` library**: two animations added, `leds_anim_blink(color, on_us, off_us)` and
  `leds_anim_fade(color, in_us, out_us)` (from black to the colour then back to black, again and again);
  `leds_anim_t` has a second time, `period2`.


### 6.23 Announcements

[announce.c](../../src/menu/announce.c): an admin badge sends an announcement, the cicadas **build the screen** from
what they receive (no image is sent).
- **Content** (`store_announce_t`): the time (`"10:30"`), the text (at most 56 characters, UTF-8), the type of the QR
  code (`ANNOUNCE_QR_NONE`, `URL`, `TEXT`, `TEL`, `SMS`, `EMAIL`, `WIFI`, `GEO`) and its content (64 bytes).
  `announce_qr_text()` puts it in the standard form that phones read: URL (`https://` added when there is no `://`),
  text as is, `tel:`, `SMSTO:`, `mailto:`, `WIFI:T:WPA;S:<network>;P:<password>;;` from `network;password`
  (`WIFI:T:nopass;S:<network>;;` without ";"), `geo:`. An empty content: no QR code.
- **Screen** (`announce_draw()`): the time in large in a 38-pixel black band ("Annonce" without a time), the text in
  the medium font cut at the spaces (`ui_wrapped()`, 3 lines with a QR code, 6 without), then the QR code
  (`score_code_draw()`, 2 to 4 pixels per module depending on the room; "(QR code trop grand)" below 2).
- **Radio**: `NET_ANNOUNCE` `[nonce 2][part][number of parts][at most 48 bytes]`. The announcement is serialized
  (`time\0text\0<type>content\0`), cut into parts of 48 bytes (5 at most), one every 70 ms, and the whole is sent
  3 times (600 ms between two rounds), at +10 dBm. The receiver puts the parts of the same sender + nonce together;
  a complete announcement is only taken once (`announce: received ...`).
- **Reception**: the last 5 announcements are kept in memory (Social > Annonces, not in flash). `announce_new()` gives
  the main loop the text of the notification ("Annonce : ..."); `announce_open_newest()` makes the next opening of the
  page show the newest one with `app_show_still()`, like the screensaver. `notify()` opens the page right away when
  the badge is on the menus or the screensaver; otherwise, it shows at the next opening of Social > Annonces.
- **Admin** (Admin > Annonces (admin), `app_announce_admin`): 6 editable announcements (`STORE_ANNOUNCES`), kept in
  `store_ext_t` (§ 6.8), examples at first (`DEFAULTS`). Rows: Heure (editor `UI_CHARSET_TEXT`, 5 characters), Texte
  and Contenu (`UI_CHARSET_LONG`, 56 characters), type of the QR code (wings), Aperçu (`announce_draw()` with the fast
  refresh), Envoyer (`announce_send()`, in the background).


### 6.24 Skills and achievements

- **Skills** ([skills.c](../../src/menu/skills.c)): 20 names (`SKILLS`, `SKILLS_COUNT` ≤ 32), a 32-bit mask in
  `store_t.skills`. The 16 × 16 pixel pictograms are drawn in ASCII art in
  [tools/skills_icons.py](../../tools/skills_icons.py), which generates `skills_icons.h` (one `uint16_t` per row, the
  high bit is the leftmost pixel; `--preview icons.png`: a contact sheet); same order as `SKILLS`.
  `skills_draw_icon()` / `skills_draw_row()` draw them (name tag: 10 at most; page: 9; "Qui les partage ?" list: 4).
  The mask goes in the beacon (§ 6.6) and in the vCard (`CATEGORIES`, § 6.20). Log: `skills: <name> on/off`.
- **Achievements** ([achievements.c](../../src/menu/achievements.c)): 32 achievements (`ACHV[]`: name, how, XP), XP =
  sum of the achievements obtained + 2 per cicada met (`XP_PER_MEETING`, `store_t.n_met`); 10 levels (`LEVEL_XP`:
  0, 20, 50, 100, 170, 260, 380, 530, 720, 1000; `LEVEL_NAMES`: Oeuf ... Cigale d'or). `achievements_init()` gives
  "Premiers pas"; `achv_task()` (every second) gives the achievements of the encounters and "Expert"; `achv_event()`
  returns the text of the announcement once, which the loop writes at the bottom of the screen with a chime (unless a
  sound is playing). Platine is given as soon as all the achievements before `ACHV_ALL` are obtained. The level goes
  in the beacon. Log: `achievement: <name> (+<xp> XP, level <n>)`. To add an achievement, see § 5.


### 6.25 Group games: the lobby (`party.c`) and the games

[party.h](../../src/menu/party.h): a **host** opens a party, the cicadas around join it, the host sees the list of the
players and starts the game; the game then exchanges its own messages with `party_send()`. A single lobby at a time
on a badge (a game page shows "Une partie de ... est en cours" when another group game runs).

- **Packet** `NET_PARTY`: `[game][session 2][kind][recipient 4][data ≤ 47]`, recipient 0 = everybody, at +10 dBm.
  Games: `PARTY_GAME_TUG` (1), `PARTY_GAME_ASSASSIN` (2), `PARTY_GAME_WEREWOLF` (3).

  | Kind | From → to | Data |
  |---|---|---|
  | OPEN (1) | host → all, every second while the lobby is open | players, maximum, options (`flags` of the game), name of the host (8) |
  | JOIN (2) | player → host, every second until it is in the list | random key (4), name (8) |
  | ROSTER (3) | host → all | page, pages, players, then 3 players at most (id 4, name 8); 4 pages per second during the lobby and the 10 s after the start, then one per second |
  | START (4) | host → all, 5 times (300 ms apart), then every second during the game | players, delay (2, ms: the game starts this long after the packet), seed (4) |
  | LEAVE (5) | anybody | the player leaves; the host: party cancelled before the start; after the start, the game decides (`PARTY_KIND_LEAVE` given to the handler) |
  | ≥ 16 (`PARTY_KIND_GAME`) | depends on the game | messages of the game, given to the handler of `party_set_handler()` once the party started |

- **Synchronized start**: each badge computes `party_start_time()` = reception of the START + delay (same principle
  as the choir); `party_seed()` is the same seed on all the badges. It travels in clear: never for a secret.
- **Secret**: the key sent by each player in its JOIN is only known by the host (the others get 0);
  `party_mask(key, salt)` (FNV-1a) masks a secret meant for this player (role, target). The key goes in clear in the
  JOIN: someone who recorded the lobby can find it (an accepted limit, it is a game).
- **States** (`party_state()`): IDLE, HOSTING, SCANNING (`party_found()`: the parties heard, the closest first),
  JOINING, JOINED, STARTED, CANCELLED. 40 players at most (`PARTY_MAX`). Log: `party: ...`.
- Each game has a **service** called by the loop (`tug_service()`, `assassin_service()`, `werewolf_service()`): the
  game goes on while the player looks at another page; its news go through `assassin_event()` / `werewolf_event()`
  and `notify()`.

**Tug of war** ([tug.c](../../src/menu/tug.c), hardware-free rules in [tug_logic.c](../../src/menu/tug_logic.c)):
2 players at least. `tug_teams()` draws the Cigales / Fourmis teams with the common seed (Fisher-Yates, xorshift:
the same on every badge), the referee is the extra player. Phases computed from the common start time: teams 5 s,
countdown 3 s, pulling 20 s (`PULL_MS`), final count, result. A pull = left wing then right wing (`tug_pull()`).
Each badge sends its total (`K_COUNT`: pulls 2 bytes, "stopped" flag) every 300 ms + 40 ms per player; each badge
adds up the totals of each team: the screens agree even far from the host. A team ahead by `tug_margin()` (20 pulls
per player of the team) stops the game at once (the flag stops the others). After the result, the badge leaves the
party (5 s). Log: `tug: ...`.

**Assassin** ([assassin.c](../../src/menu/assassin.c), [assassin_logic.c](../../src/menu/assassin_logic.c)): 3 players
at least (2 when the host is in admin mode, for the tests). The host draws a **secret ring** with its own random
generator (not the common seed) and a secret code per player, and sends each one **its** target only (`K_TARGET`,
masked with `party_mask()` of its key) until the acknowledgement (`K_TARGET_ACK`).
- **Kill**: the right wing sends `K_KILL` to the target (every 400 ms for 4 s) with a proof (hash of the code of the
  target and of the killer's id). The target accepts it with a right proof and an RSSI ≥ `ASSASSIN_KILL_RSSI`
  (**−50 dBm**, badges almost touching, **to calibrate**: the victim logs `assassin: KILL from <name>, rssi <n>` at
  each attempt; `-DASSASSIN_KILL_RSSI=-90` for a test). It then sends `K_DEAD` back to the killer: its own target,
  sealed with its code (only the killer can open it), until `K_DEAD_ACK`.
- **STATUS**: each badge sends now and then (20 s ± 5 s, in bursts after an event) the dead, the leavers and the
  winner; the badges merge them: a lost packet does not lose the game. A player who gives up puts his sealed target in
  his STATUS: his hunter opens it and goes on.
- **Known weaknesses** (listed at the top of `assassin.c`): the key in clear in the JOIN, a refused proof (too far)
  replayable from close by, a stronger transmitter kills from afar, fake STATUS. Log: `assassin: ...`.
- The hot - cold gauge follows the beacons of the target (§ 6.6) or its packets of the game.

**Werewolf** ([werewolf.c](../../src/menu/werewolf.c), rules in [werewolf_logic.c](../../src/menu/werewolf_logic.c)):
a narrator (who does not play: `party_host(..., false, ...)`) and 5 to 20 players. The narrator's badge deals the
roles and runs the phases; its messages (STATE, NAMES, PRIV masked with the key of each player, ACT, ABORT) are
described at the top of `werewolf.c`. At night, every living player chooses in a list and receives the same kind of
packet: nobody can guess the roles. Test mode: narrator in admin mode, robots complete the party.
Rules and phases: [loup_garou.md](../fr/loup_garou.md) (in French).


### 6.26 Smuggling

[smuggler.c](../../src/menu/smuggler.c): 26 goods ([smuggler_goods.c](../../src/menu/smuggler_goods.c), 32 × 32 icons
generated by `tools/smuggler_icons.py`), the hold in `store_t.cargo[32]`, finds at the encounters, trades and gifts
between two badges "à portée de main" (within reach): RSSI of the beacons ≥ `SMUGGLER_TRADE_RSSI` (**−55 dBm**, badges
touching, **to calibrate** on site). The trade protocol ([smuggler_trade.c](../../src/menu/smuggler_trade.c),
hardware-free, tested on the PC) uses `NET_TRADE` (0x07): `[trade id 4][kind][recipient 4][data]`, a two-phase
commit where the inviter decides; the hold is written at once after a trade (`store_save_now()`), so that a good is
never duplicated. Everything is described in [smuggler.md](smuggler.md).


### 6.27 Pirate radio

[pirate_radio.c](../../src/menu/pirate_radio.c) (Admin > Radio pirate) sends sound in narrow band FM: the CC1101 in
2-FSK, asynchronous serial mode, at 500 kBaud, and the audio output (`AUDIO_OUT_RADIO`) that drives GDO0 with a
~122 kHz PWM whose duty cycle follows the sound; an NFM receiver only sees its average, the instantaneous frequency
follows the sound. The listening (Radio & IR > Écouter la radio pirate) puts the CC1101 of another badge in 2-FSK
asynchronous receive (406 kHz channel): GDO2 follows the PWM of the transmitter, a PWM slice of the RP2040 counts
the time high and a DMA at 16 kHz copies it; the loop turns it into sound, measures the tone (Goertzel) and corrects
the frequency offset (AFC). The network is paused while sending and listening; the transmission stops after
10 minutes.

**Test between two badges**: the 1 kHz test tone is recovered by the receiving badge with the transmitter at
**+10 dBm** (purity 84 %); at **0 dBm** it arrives weak: the wide channel (406 kHz) of the badge receiver makes it
not very sensitive. Details, settings and listening with a Portapack or an SDR: [pirate_radio.md](pirate_radio.md).


### 6.28 Ringtones and gamebooks

- **Ringtones** ([rtttl.c](../../src/menu/rtttl.c), hardware-free parser [rtttl_parse.c](../../src/menu/rtttl_parse.c)):
  12 built-in ringtones (`RTTTL_BUILTIN`), then the `.txt`, `.rtttl` and `.rtx` files of the `SONNERIES` folder
  (24 files, 128 ringtones). The sound is synthesized (square wave, envelope) about 150 ms ahead in the audio buffer;
  the note shown and the LEDs follow `audio_played()`. Log: `rtttl: ...`. Format: [ringtones.md](ringtones.md).
- **Gamebooks** ([gamebook.c](../../src/menu/gamebook.c), hardware-free parser
  [gamebook_parse.c](../../src/menu/gamebook_parse.c)): the built-in book
  ([gamebook_builtin.c](../../src/menu/gamebook_builtin.c), generated by
  `python tools/gamebook_check.py docs/sd/LIVRES/tresor_cigalon.txt --c src/menu/gamebook_builtin.c`) and the `.txt`
  files of the `LIVRES` folder (16 at most). The badge only reads the section shown; the progress is in
  `store_t.book_hash` and `book_section`. Log: `gamebook: ...`. Format and limits: [gamebooks.md](gamebooks.md).


### 6.29 Demo mode

Admin > Mode démo ([demo.c](../../src/menu/demo.c)) explains, then calls `demo_start()` of `main.c`: the loop goes
through the steps of `DEMO_STEPS` (name, duration): name tag 8 s, achievements 6 s, images 18 s (`image_next()`
every 6 s), video 20 s, skills 5 s, music 10 s, screen demo 15 s, program 6 s, radar 6 s, gamebooks 5 s, credits
6 s, info 5 s, then again. The media play the first file of the browser (or of its first folder); without an SD card
or a file, the step is skipped (`demo: <step> skipped`). `demo_back()` goes back to the menu step by step (a video
stops in several loops). Any button stops the demo (the press is not given to the page); the LEDs (rainbow during the
demo) go back to their mode, the screensaver is held off during the demo. Logs: `demo: on`, `demo: <step>`,
`demo: off`.


## 7. File formats

| Format | Content |
|---|---|
| `.EPI` (image) | 16-byte header: `EPIMAGE1`, width, height, bits per pixel (1 or 2), 0 (little-endian uint16); then the "lsb" plane, then the "msb" plane (if 2 bits). Each plane is 5000 bytes, bit 7 = leftmost pixel, row by row. Gray = msb×2 + lsb (0 = black, 3 = white). |
| `.EPV` (video) | 512-byte header: `EPVIDEO2`, width, height, fps, bits per pixel, frame count, audio sample rate, sample count; then the 5000-byte frames (1 = white); then the sound, 8-bit unsigned, synchronized with the first frame. `EPVIDEO1` = no sound. |
| `.WAV` | 8, 16, 24 or 32-bit PCM, 32-bit float, `WAVE_FORMAT_EXTENSIBLE`; mono or stereo, 4 to 192 kHz (decimated above 48 kHz); 8-bit 16 kHz mono recommended (`audio2wav.py`). |
| `.TXT` | UTF-8 (with or without BOM) or Windows-1252, detected automatically. |
| Ringtones (`.txt`, `.rtttl`, `.rtx`) | one RTTTL ringtone per line, `name:d=4,o=5,b=120:notes`; empty lines and `#` ignored; 2048 characters per line at most ([ringtones.md](ringtones.md)). |
| Gamebook (`.txt`) | title, author, then sections `== n [FIN [gagné\|perdu]]`, choices `-> n [item\|!item\|dé a-b] : text`, items `+[x]` / `-[x]`; 400 sections, 8 choices, 8 items at most ([gamebooks.md](gamebooks.md)). |
| `.sub` (Flipper Zero) | Sub-GHz RAW file: header lines (`Frequency`, `Preset`, possibly `Custom_preset_data`), then `RAW_Data:` lines of at most 512 durations in µs, alternately positive (carrier) and negative (silence). Written by `ook_sub.py` and `flipper_net_sub.py` (§ 6.19). |
| `.vcf` (vCard 3.0) | business cards exported by the `k` key and `tools/contacts_export.py`. |

Images embedded in the firmware (display demo, logos) are converted to C arrays at build time by `image2epaper.py`
(CMake function `badge_image2epaper`).


## 8. The USB serial protocol

The badge shows up as a USB serial port (115200 baud, irrelevant over USB). One key per character:

| Key | Action |
|---|---|
| `a` `b` `x` `y` | short press: left wing, right wing, right flank, left flank |
| `A` `B` `X` `Y` | long press (quit a puzzle, save the name, fast-reading forward / back...) |
| `[` / `]` | send the screen on every change: on / off |
| `s` | send the screen once |
| `!` | diagnostic: OLED, IR, cicada network and neighbours, CTF, network counters (sent, received, dropped, loopback), state of the network and of the radio (`net state:`, § 6.12), state of the listening to the remote controls (`remote state:`, § 6.13), remote commands (enabled, muted, admin mode), firmware version (`version:`), radio (version, crystal used and measured), battery |
| `?` | sound diagnostic |
| `i` | IR test: decode an NEC frame then transmit it (about 68 ms) |
| `o` | state of the OOK receiver: active, pulses, frames, RSSI, MARCSTATE, GDO0 |
| `p` | durations of the last signal given to the OOK decoder |
| `O` | log the decoding attempts of the OOK receiver: on / off |
| `k` | export of the business cards received as vCards (read by `tools/contacts_export.py`) |
| `V` | log every network packet sent and received (with the measured frequency offset, FREQEST; in chat mode, the text received): on / off |
| `r` | registers of the CC1101 and PATABLE |
| `M` | sends the "Radio : message" (chat profile of the Flipper: `subghz chat 433920000 0` receives it) |
| `P` | ping at +10 dBm: the badges that hear it print `net: ping #n from <id>, rssi ...` |
| `L` | *loopback* mode (§ 6.12): on / off |
| `R` | reboot (watchdog) |
| `U` | check of the texts (§ 5, `ui.h`): logs `uicheck: ...` for the texts cut, too wide or under the footer: on / off |
| 0x01 + `A` / `a` | admin mode: on / off ("Mode admin" check box of `badge_remote.py`); the badge answers `admin: on` / `admin: off` |
| 0x02 + character | inserts the character (ASCII) into the open text editor (keyboard mode of `badge_remote.py`); `\r`: done, Escape: cancel, `\b`: erase |

The badge sends lines of text:
- `ui: <title>` on every page change (the name of the application when it opens);
- `browser:`, `image:`, `saver: on/off`, `radio:`, `social:`, `game:`, `ctf: code right/wrong`, `credits:`, `name:`...
- `notify:` (notification), `net:`, `remote:`, `admin:`, `ook:`, `talk:`, `vote:`, `message:`, `program:`,
  `infection:`, `hotcold:`, `hunt433:`, `contacts:`, `chorus:`, `duel:`, `battle:`, `crypto:`, `store:`, `tune:`,
  `leds:`, `announce:`, `reset:`, `uicheck:`, `skills:`, `achievement:`, `party:`, `tug:`, `assassin:`, `werewolf:`,
  `smuggler:`, `pirate:`, `rtttl:`, `gamebook:`, `demo:`, `wav:`...
- `version: <number> (<commit> <date>)` at boot;
- `music: end at <n>s of <duration>s` at the end of a music (or on a read error: the position is then before the
  end), used by `tools/badge_media_test.py`.

The screen is sent as follows:

```
@FB <BW|4G|WHITE|BLACK> [<lsb plane in base64> [<msb plane in base64>]]
```

Messages sent before the PC opens the port are lost: this is a limitation of USB CDC.


## 9. Tests

### 9.1 Host tests (no badge needed)

```bash
python src/tests/host/run_tests.py          # all tests
python src/tests/host/run_tests.py games -v # a single test, with its traces
```

The C modules are compiled with the host compiler (gcc or clang, `CC` variable).
Pico SDK stand-ins are provided in `tests/host/stubs`: GPIO, recorded SPI, simulated time.

| Test | Checks |
|---|---|
| `gfx` | frame buffer format, clipping, UTF-8 text, accents, sizes |
| `ir` | exact NEC decoding, ±20 % tolerance, extended addresses, invalid frames |
| `games` | tic-tac-toe never loses (every game), Connect 4 wins / blocks, Simon, Reflexes and Snake rules, high scores |
| `puzzles` | Démineur, 2048, Taquin, Sokoban, Mastermind, Pendu: rules, end of game, records, texts that fit on the screen |
| `score` | SipHash (reference vectors), score text and signature, QR code drawing |
| `crypto` | texts that fit on the screen, each answer only for its challenge, normalization, wrong answers, pieces and final flag |
| `vcard` | vCard packets (at most 60 bytes, folding, ticked fields), check line: lost line (or folded continuation) and corrupted value rejected; card typed on a Flipper accepted, other chat texts ignored, longest values (55 characters) |
| `ookdec` | remote controls (Princeton, CAME, Nice FLO) and sensors (Nexus-TH, ThermoPRO-TX4, GT-WT02, inFactory, LaCrosse, Acurite) with jitter, glitches, ±20 % clock, repeat required without checksum; noise decodes to nothing |
| `werewolf` | rules of the werewolf (roles, votes, deaths, lovers, winners), then whole games between simulated badges (`werewolf.c` compiled once per badge) over a radio that loses packets, random buttons, texts checked |
| `rsvp` | word splitting, French typography, BOM, Windows-1252, durations, long words, forward / back |
| `gamebook` | format of the books (CRLF, BOM, comments, items, die, endings), missing sections, long lines and texts, typography, line wrapping; the built-in book: same as `docs/sd/LIVRES`, valid links, random games all reach an ending |
| `rtttl` | defaults, durations, dots, sharps, octaves, pauses, spaces, case, errors and their position, garbage (no crash), ringtones of the firmware and of `docs/sd/SONNERIES` |
| `screen` | RAM windows, screen copy, recovery after `screen_clear` |
| `party_games` | tug of war (same teams on every badge, referee, pulls) and assassin (ring, seal, games) |
| `smuggler` | the hold, the draws, `smuggler_goods.c` up to date with the icons; the trade protocol over a radio that loses, repeats and delays packets: no good created or lost |
| `image2epi`, `video2epaper`, `audio2wav` | output files (headers, sizes, non-empty sound). The last two are skipped without ffmpeg. |

### 9.2 Badge test (over USB)

```bash
python tools/badge_selftest.py              # badge_menu flashed, serial port free
python tools/badge_selftest.py --only games,radio -v
python tools/badge_selftest.py --ctf         # also tests the Konami code (marks the flag as found)
```

Sequence:
1. The script reboots the badge (`R`).
2. It simulates button presses and checks the traces: menus, games, screen saver, credits, radio, beacons, IR, SD card images,
   applications, admin mode, OOK receiver, social features.
3. It writes a PASS / FAIL / SKIP table and PNG screenshots (`selftest_<date>` folder).

A test is SKIPped when the hardware is missing (no SD card, uncalibrated battery).
No badge setting is changed, except with `--ctf` (and the game records, since a game can end with 0 points).
The `admin` and `net` groups turn on the admin mode, the mute mode or the virus for the test, then turn them off.
The `admin` group checks the secret sequence of the flanks; the others open the Admin theme with the serial command
0x01 `A` (`open_admin()`), like the check box of `badge_remote.py`. The `SOCIAL` and `ADMIN` lists of the script
follow the order of the menus: to update along with `SUBMENUS`.

The groups (`--only`):

| Group | Checks |
|---|---|
| `diag` | `!` diagnostic: firmware version, CC1101 version, crystal, battery |
| `menus` | each theme opens, back to the themes |
| `games` | mini games, score QR code, record (long press) |
| `puzzles` | each puzzle: help page, start, a move, quitting with a long press on the left wing |
| `ctf` | Konami code (only with `--ctf`) |
| `settings` | screensaver preview and wake-up, credits |
| `radio` | radio message, test mode, beacons of the cicada network |
| `ir` | decoding and sending an NEC frame |
| `images` | images of the SD card |
| `apps` | name tag and lamp |
| `admin` | secret sequence, mute / unmute commands, leaving the admin mode |
| `radio433` | 433 MHz decoder and weather station pages, then the network beacons come back |
| `social` | pages of the Social theme (contacts, radar, hot - cold...) and states of the talk badge |
| `net` | with a single badge, in *loopback*: vote, virus, message |

### 9.3 Two-badge tests

Two badges plugged into USB, `badge_menu` flashed, serial ports free. The buttons are simulated and the logs of both
badges are checked; screenshots in a dated folder. The badges are restarted first (except with `--no-reboot`).

```bash
python tools/test_party_games.py --ports COM9 COM11 [--only tug,assassin]   # tug of war, assassin
python tools/test_werewolf.py --ports COM9 COM11 [--advanced]               # werewolf (narrator + robots)
python tools/test_smuggler.py --ports COM9 COM11                            # smuggling
```

- `test_party_games.py`: badge 1 creates the party, badge 2 joins it; in the tug of war, only badge 2 pulls and its
  team must win, with the same result on both; in the assassin (badge 1 is put in admin mode to play with 2), badge 1
  eliminates badge 2: badges side by side, the RSSI measured by the victim is printed (to calibrate
  `ASSASSIN_KILL_RSSI`).
- `test_werewolf.py`: badge 1 is the narrator, put in admin mode for the test (robots complete the party up to the
  minimum), badge 2 the only real player; checks the role received, the acknowledgement of each choice, the same
  phases and the same end on both badges, and that no text is cut.
- `test_smuggler.py`: badges touching; a trade, a gift, a refusal, a cancellation after the guest sealed its good.
  The hold of the badges changes: use test badges.

### 9.4 Per-module test applications

`src/tests/*.c`: one application per module (display, radio, LEDs, sound...), to flash in order to explore a peripheral.


## 10. Tools

| Script | Usage |
|---|---|
| `tools/badge_remote.py` | window showing the badge screen enlarged (zoom 2–4), arrows / Enter = buttons, Shift = long press ("appui long" buttons on screen too), PNG capture; "Badge :" list to choose among several badges (or `--port`); "Mode clavier" check box: the text typed goes to the badge's editor (0x02 + character; Enter: done, Escape: cancel, Backspace: erase); "Mode admin" check box (0x01 + `A` / `a`), which follows the state of the badge (`admin: on/off` lines, and `!` sent on connection); the reading thread never dies (errors in the status line and the log); `--snapshot file.png` for a single capture |
| `tools/badge_selftest.py` | automated badge test (§ 9.2) |
| `tools/badge_screens.py` | goes through every theme, every entry (Admin included) and the pages of the applications, saves a PNG image of each screen in `docs/screens/` and writes [docs/fr/ecrans.md](../fr/ecrans.md) and [docs/en/screens.md](screens.md); with the check of the texts (`U`), lists the texts cut, too wide or under the footer (`docs/screens/checks.txt` and end of the pages); `--port`, `--only Jeux,Social`. The badge is restarted; the toggles of the menus are not pressed |
| `tools/badge_media_test.py` | plays every video and music of the SD card (subfolders included) and checks that each one plays to its end (read error, stop before the end, frames per second of the videos); `--videos`, `--music` (both by default), `--max N` (N seconds of each file at most, 0 = whole file), `--port`; report printed and written to `media_report.txt` |
| `tools/score_check.py` | checks the score QR codes and ranks them (§ 6.11) |
| `tools/contacts_export.py` | business cards received by the badge → `.vcf` (`--port`, `-o`) |
| `tools/crypto_ctf_make.py` | generates the table of the crypto challenges (`--update`, `--answers`): solution file (§ 6.18) |
| `tools/ook_sub.py` | Flipper `.sub` files: Princeton, CAME, Nice FLO, weather sensors; `check`, `selftest` (§ 6.19) |
| `tools/flipper_net_sub.py` | Flipper `.sub` files for the cicada network (`command`, `ping`, `raw`) and "SecSea" preset (`preset`) (§ 6.19) |
| `tools/skills_icons.py` | 16 × 16 pictograms of the skills in ASCII art → `src/menu/skills_icons.h`; `--preview icons.png` (§ 6.24) |
| `tools/smuggler_icons.py` | 32 × 32 icons of the goods → `src/menu/smuggler_goods.c`; `--png sheet.png`, `--check` (§ 6.26) |
| `tools/gamebook_check.py` | checks a gamebook (errors, warnings), `--pages`, `--play` (play it in the terminal), `--c` (writes the built-in book) (§ 6.28) |
| `tools/test_party_games.py`, `tools/test_werewolf.py`, `tools/test_smuggler.py` | two-badge tests (§ 9.3) |
| `src/images/image2epi.py` | images → `.EPI` (options `--fit`, `--bw`, `--contrast`, `--equalize`, `--preview`) |
| `src/video/video2epaper.py` | video → `.EPV` (`--fps`, `--fit`, `--dither bayer\|fs\|threshold`, `--start`, `--duration`, `--no-audio`, `--preview`) |
| `src/audio/audio2wav.py` | sounds → 8-bit 16 kHz WAV (wildcards, folders, `--no-filter`, `--start`, `--duration`) |
| `src/image2epaper.py` | images → C arrays (used by CMake) |
| `src/gfx/gen_fonts.py` | generates `gfx_fonts.c` from a TrueType font |


## 11. Development troubleshooting

| Symptom | What to check |
|---|---|
| The badge does not show up over USB | switch set to ON; data cable; BOOTLOADER mode for flashing |
| `picotool` cannot find the badge | is the serial port held open by another application (badge_remote, terminal)? |
| No radio packets received | crystal: compare "crystal used" and "measured" in the `radio:` line of `!`; same preset on both sides; Réglages > Réglage radio close to other badges (frequency offset, § 6.21) |
| `net: radio repaired (...)` in the logs | the watchdog of the network put the radio back in its configuration (§ 6.12); if it happens again and again, look for the feature that leaves the radio in another state (registers of the `net state:` line of `!`) |
| Frozen or uniform display | call made while `screen_busy()`; after `screen_clear()`, the RAM bypass is restored on the next draw |
| Inaudible sound | volume; file converted with `audio2wav.py` (without `--no-filter`) |
| Two modules fight over a GPIO interrupt | use `gpio_add_raw_irq_handler()` |
| New `store_t` field reads 0xFF | expected on a badge that has already been used: check for it and fall back to a default value |
| Testing a badge-to-badge feature with a single badge | `L` key (*loopback*, § 6.12), and `V` to see the packets |
| A packet does not arrive | `V` on both badges; "dropped" counter of `!` (queue full, data too long, chat mode); an OOK window (non-badge transmitter heard, or every 10 s) makes the network deaf for a moment: repeat the important packets, or suspend the windows with `remote_pause_windows()` (§ 6.13) |
| A Flipper code is not decoded | `O` key: decoding attempts; the code must be seen twice (§ 6.14) |
| No badge hears this badge any more after OOK listening | FREND0 left at 0x11: the GFSK configuration must write FREND0, FREND1, MDMCFG0 back (§ 6.14) |
| The Flipper sends another Princeton key | `subghz tx` on the command line: use a `.sub` file and `subghz tx_from_file` (§ 6.19) |
| A kill of the assassin or a cicada "à portée de main" of the smuggling game does not work | provisional RSSI thresholds (`ASSASSIN_KILL_RSSI` −50 dBm, `SMUGGLER_TRADE_RSSI` −55 dBm): read the RSSI logged (`assassin: KILL from ..., rssi`, `smuggler: at hand ...`) and adjust at build time (`-D...`) |
| The Flipper records nothing from the badges | the badges send in GFSK (frequency modulation): Read RAW in AM650 / AM270 (the default) does not see them. Use the "SecSea" preset added to `subghz/assets/setting_user` (§ 6.19), or else FM476. Check the transmission: `subghz chat 433920000 0` on the Flipper and `M` on the badge |
