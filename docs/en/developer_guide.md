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
  since the measurement itself varies by about 500 ppm.
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
├── score_check.py      checks the score QR codes, leaderboard
├── contacts_export.py  business cards received by the badge → vCard file
├── crypto_ctf_make.py  generates the crypto challenges (solution file: spoilers)
├── ook_sub.py          Flipper Zero .sub files: test remote controls and weather sensors
├── flipper_net_sub.py  Flipper Zero .sub files: packets of the cicada network, "SecSea" preset
└── flipper/            Flipper remote controls: SecSea_general.sub, SecSea_talk.sub (§ 6.13)
docs/           documentation, ideas, PVSR, synchronization of the choir (chorus_sync.md, in French)
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
    social_task(now); battery_task(now); store_task(now); ...
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
| `radio_tools.c` | radio message and carrier, GFSK profile, crystal measurement |
| `messages.c`, `contacts.c`, `vcard.c`, `program.c`, `vote.c` | relayed messages, business cards and their vCard format (§ 6.20), program, votes |
| `hotcold.c`, `infection.c`, `chorus.c` | hot - cold hunt and radar, cicada virus, choir (§ 6.15) |
| `duel.c`, `battle.c` | rock-paper-scissors and battleship between two badges (§ 6.16) |
| `image_radio.c` | sending and receiving an image over the radio (§ 6.17) |
| `crypto_ctf.c`, `crypto_app.c` | cryptography challenges and their pages (§ 6.18) |
| `lamp.c`, `nametag.c`, `talk.c`, `admin.c` | lamp, name tag, talk badge, radio commands and badge type (admin) |
| `credits.c` | credits pages |
| `score_code.c` | signed scores shown as a QR code (§ 6.11) |
| `battery.c` | battery level (calibration) |
| `store.c` | settings, scores and business cards in flash |
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

### Drawing helpers and text editor (`ui.h`)

[ui.h](../../src/menu/ui.h) gives the pages their common look: `ui_title()` (black band), `ui_footer()`,
`ui_list()` (scrolling list), `ui_lines()` / `ui_text()` (centered or left-aligned lines), `ui_fit()`
(text truncated with "..."), `ui_box()`, `ui_gauge()`.

The 4-button text editor (`ui_edit_t`, at most 56 characters, `UI_EDIT_MAX`) is used by the business cards, the crypto challenges
and the cure of the virus:
- `ui_edit_start(e, text, length, charset)`: `UI_CHARSET_TEXT` (letters, digits, punctuation for names, e-mails,
  URLs), `UI_CHARSET_PHONE` (digits, +, space), `UI_CHARSET_UPPER` (A-Z, 0-9, space);
- flanks: `ui_edit_change(e, -1 / +1)` (repeating while held is up to the caller);
- right wing (short press): `ui_edit_move(e, 1)`; left wing: `ui_edit_move(e, -1)`, which returns `false`
  from the first position (cancel);
- right wing (long press): done, `ui_edit_result()` returns the text without the trailing spaces;
- `ui_edit_render()` draws the prompt, the text around the cursor and the instructions;
- the PC keyboard (keyboard mode of `badge_remote.py`: byte 0x02 followed by the character on the USB) types into the
  open editor, through `ui_edit_apply_typed()` (Enter: done; Escape: cancel; Backspace: erase).

### Adding a menu entry

For an entry handled directly by `main.c` (the older features):
1. Add a value to `menu_item_t` and its label in `item_label()`.
2. Put it in a `SUBMENUS` theme (at most 16 entries per theme, `items[16]` of `submenu_t`).
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
"Quitter le mode admin" sets it back to 0.

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
- `wav.c` reads PCM WAV files; video uses the sound as its clock.
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
  - 17-byte packet: `0xC1`, type 1, identifier (FNV hash of the unique ID), sequence number, score, name (8 characters);
  - an encounter = RSSI ≥ −80 dBm (`SOCIAL_RSSI_CLOSE`, provisional: to calibrate on site with the radar)
    on 3 beacons within 10 s;
  - +10 points for a new badge, +1 for a known badge, at most once per hour;
  - the neighbours heard (`social_neighbours()`) are used by the radar, the messages and the game invitations.

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
  `puzzle_records` (records of the puzzles).

How it works:
- the write happens 5 s after the last change, using `flash_safe_execute` (about 50 ms, interrupts disabled);
- new fields are appended **at the end** of the structure: in a sector written by an earlier version,
  they read as 0xFF, which their users check for;
- a `_Static_assert` guarantees that the structure fits in the sector.

The business cards do not fit in that sector: they live in a second store, `store_ext_t`, of 8 KB (two sectors)
right before the first one (`STORE_OFFSET - 8192`), with its own header (magic `"CONT"`, version):
the mask of the fields sent, my card and the 12 cards received (`STORE_CONTACTS`), 512 bytes each with fixed fields
(`CONTACT_BYTES`). It is reset when the header does not match: the move to 512-byte cards (`STORE_EXT_VERSION` 2)
therefore erases, on update, the cards received **and** my card (the mask goes back to first name + name).
`store_ext_changed()` writes it the same way, 5 s later.

### 6.9 Battery

- The raw reading (ADC3, 16 samples, filtered) is only converted to a voltage with a **two-point calibration**
  (`BATTERY_CAL_RAW1/MV1/RAW2/MV2` in [battery.h](../../src/menu/battery.h)).
- Without calibration nothing is displayed: the badge must never show a wrong value.
- To calibrate:
  1. read the `battery:` line of the `!` diagnostic (the `ADC raw` value);
  2. measure the battery voltage with a multimeter at the same time;
  3. do this once while charging and once on battery;
  4. fill in the four values.

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
| 0x01 | `NET_BEACON` | `social.c` | sequence, score (2 bytes), name (8) |
| 0x02 | `NET_COMMAND` | `remote.c` | command, nonce (2) |
| 0x03 | `NET_MESSAGE` | `messages.c` | uid (2), TTL, origin (4), recipient (4, 0 = everybody), name (8), message number |
| 0x04 | `NET_VOTE_QUESTION` | `vote.c` | session (2), question, open (0 / 1); every 3 s while the question is open |
| 0x05 | `NET_VOTE_ANSWER` | `vote.c` | session (2), question, answer; sent 3 times |
| 0x06 | `NET_GAME` | `duel.c`, `battle.c` | session (2), kind, recipient (4), round or turn, then depending on the kind (§ 6.16) |
| 0x07 | `NET_CONTACT` | — | unused, reserved (the cards go in chat mode, § 6.20) |
| 0x08 | `NET_HOTCOLD` | `hotcold.c` | hot - cold beacon, one per second |
| 0x09 | `NET_INFECTION` | `infection.c` | generation (0 = patient zero); a "cough" every 4 to 5 s, at +10 dBm; contagion at RSSI ≥ −80 dBm (provisional) |
| 0x0A | `NET_IMAGE` | `image_radio.c` | transfer (2), block, 48 bytes (§ 6.17) |
| 0x0B | `NET_SONG` | `chorus.c` | song, kind, session (2), ms (4), voices (§ 6.15) |
| 0x0F | `NET_PING` | `net.c` | number (`P` key) |

Messages are relayed by flooding: each badge sends a message it has not seen yet (origin + uid) once more,
with the TTL decremented, with `NET_MEDIUM | NET_JITTER`, down to TTL 0 (3 at the start).

**Testing with a single badge**: the `L` key turns on the *loopback* mode: every packet sent comes back as if sent by
a "twin" whose id is `net_id() ^ NET_TWIN` (0x00FF00FF). The badge can then vote on its own question, receive its own
message or infect itself. The cicada network ignores this twin (no encounter).
`V` logs every packet sent and received (in chat mode, the text of the packets received), `P` sends a ping, `!` shows
the counters (sent, received, dropped).


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
| 0x20 + n | shows talk n of the program | `program.c` |
| 0x30 + n | starts song n of the choir | `chorus.c` |

A module handles a group of commands (the high nibble) with `remote_subscribe(group, handler)`;
the handler gets the low nibble. `remote_execute()` executes a local command.
The badge obeys unless Réglages > Télécommande is set to "non" (`store_t.remote_off = 1`).

**Listening to the remote controls**: the CC1101 cannot listen in GFSK and OOK at the same time. There is no blind
listening window any more: the network listens all the time, and `remote_task()` reads the RSSI every 20 ms
(`RSSI_POLL_MS`) when the radio and the network are free. A transmitter at −90 dBm or more (`OOK_TRIGGER_DBM`; the
noise is around −105 dBm) measured twice in a row (`OOK_TRIGGER_POLLS`) without a network packet on the air (no sync
word recognized, `net_transmitting()`) may be a remote control: an OOK window of 150 ms opens (`OOK_WINDOW_MS`).
It is extended in steps of 100 ms, up to 1.5 s (`OOK_WINDOW_MAX_MS`), while a remote control is sending (at least
30 edges in 100 ms, `OOK_ACTIVE_PULSES`: a Princeton frame gives about 95, the noise and the GFSK packets of the
badges far fewer). After a window, the next one waits at least 800 ms (`OOK_PERIOD_MS`, against a transmitter that
never stops); a window also opens every 10 s (`OOK_FORCED_MS`), for a remote control weaker than the threshold.
Measured between two badges: 97 to 98 % of the pings received (82 to 85 % with 80 ms windows every 800 ms, 73 % with
the former 220 ms windows).
During a window, the network hears nothing. The features that need all the packets (choir, image, card exchange)
suspend the windows with `remote_pause_windows()` (calls are counted), and there are none in chat mode.
The talk badge listens to the remote control all the time: it only receives the admin commands in OOK.

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
An `owns_leds` application (the talk badge) drives its LEDs and its buzzer even in mute mode.

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
  `X-MASTODON`, `NOTE`; only the ticked fields are sent.
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
  sent. The `NET_CONTACT` type is no longer used.


## 7. File formats

| Format | Content |
|---|---|
| `.EPI` (image) | 16-byte header: `EPIMAGE1`, width, height, bits per pixel (1 or 2), 0 (little-endian uint16); then the "lsb" plane, then the "msb" plane (if 2 bits). Each plane is 5000 bytes, bit 7 = leftmost pixel, row by row. Gray = msb×2 + lsb (0 = black, 3 = white). |
| `.EPV` (video) | 512-byte header: `EPVIDEO2`, width, height, fps, bits per pixel, frame count, audio sample rate, sample count; then the 5000-byte frames (1 = white); then the sound, 8-bit unsigned, synchronized with the first frame. `EPVIDEO1` = no sound. |
| `.WAV` | 8- or 16-bit PCM, mono or stereo; 8-bit 16 kHz mono recommended (`audio2wav.py`). |
| `.TXT` | UTF-8 (with or without BOM) or Windows-1252, detected automatically. |
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
| `!` | diagnostic: OLED, IR, cicada network and neighbours, CTF, network counters (sent, received, dropped, loopback), remote commands (enabled, muted, admin mode), radio (version, crystal), battery |
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
| 0x02 + character | types the character into the open text editor (keyboard mode of `badge_remote.py`); `\r`: done, Escape: cancel, `\b`: erase |

The badge sends lines of text:
- `ui: <title>` on every page change (the name of the application when it opens);
- `browser:`, `image:`, `saver: on/off`, `radio:`, `social:`, `game:`, `ctf: code right/wrong`, `credits:`, `name:`...
- `notify:` (notification), `net:`, `remote:`, `admin:`, `ook:`, `talk:`, `vote:`, `message:`, `program:`,
  `infection:`, `hotcold:`, `contacts:`, `chorus:`, `duel:`, `battle:`, `crypto:`, `store:`...

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
| `rsvp` | word splitting, French typography, BOM, Windows-1252, durations, long words, forward / back |
| `screen` | RAM windows, screen copy, recovery after `screen_clear` |
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

The groups (`--only`):

| Group | Checks |
|---|---|
| `diag` | `!` diagnostic: CC1101 version, crystal, battery |
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
| `net` | with a single badge, in *loopback*: vote, talk announcement, virus, message |

### 9.3 Per-module test applications

`src/tests/*.c`: one application per module (display, radio, LEDs, sound...), to flash in order to explore a peripheral.


## 10. Tools

| Script | Usage |
|---|---|
| `tools/badge_remote.py` | window showing the badge screen enlarged (zoom 2–4), arrows / Enter = buttons, Shift = long press ("appui long" buttons on screen too), PNG capture; "Badge :" list to choose among several badges (or `--port`); "Mode clavier" check box: the text typed goes to the badge's editor (0x02 + character; Enter: done, Escape: cancel, Backspace: erase); the reading thread never dies (errors in the status line and the log); `--snapshot file.png` for a single capture |
| `tools/badge_selftest.py` | automated badge test (§ 9.2) |
| `tools/score_check.py` | checks the score QR codes and ranks them (§ 6.11) |
| `tools/contacts_export.py` | business cards received by the badge → `.vcf` (`--port`, `-o`) |
| `tools/crypto_ctf_make.py` | generates the table of the crypto challenges (`--update`, `--answers`): solution file (§ 6.18) |
| `tools/ook_sub.py` | Flipper `.sub` files: Princeton, CAME, Nice FLO, weather sensors; `check`, `selftest` (§ 6.19) |
| `tools/flipper_net_sub.py` | Flipper `.sub` files for the cicada network (`command`, `ping`, `raw`) and "SecSea" preset (`preset`) (§ 6.19) |
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
| No radio packets received | crystal: compare "Quartz mesuré" and "utilisé" in Infos; same preset on both sides |
| Frozen or uniform display | call made while `screen_busy()`; after `screen_clear()`, the RAM bypass is restored on the next draw |
| Inaudible sound | volume; file converted with `audio2wav.py` (without `--no-filter`) |
| Two modules fight over a GPIO interrupt | use `gpio_add_raw_irq_handler()` |
| New `store_t` field reads 0xFF | expected on a badge that has already been used: check for it and fall back to a default value |
| Testing a badge-to-badge feature with a single badge | `L` key (*loopback*, § 6.12), and `V` to see the packets |
| A packet does not arrive | `V` on both badges; "dropped" counter of `!` (queue full, data too long, chat mode); an OOK window (non-badge transmitter heard, or every 10 s) makes the network deaf for a moment: repeat the important packets, or suspend the windows with `remote_pause_windows()` (§ 6.13) |
| A Flipper code is not decoded | `O` key: decoding attempts; the code must be seen twice (§ 6.14) |
| No badge hears this badge any more after OOK listening | FREND0 left at 0x11: the GFSK configuration must write FREND0, FREND1, MDMCFG0 back (§ 6.14) |
| The Flipper sends another Princeton key | `subghz tx` on the command line: use a `.sub` file and `subghz tx_from_file` (§ 6.19) |
| The Flipper records nothing from the badges | the badges send in GFSK (frequency modulation): Read RAW in AM650 / AM270 (the default) does not see them. Use the "SecSea" preset added to `subghz/assets/setting_user` (§ 6.19), or else FM476. Check the transmission: `subghz chat 433920000 0` on the Flipper and `M` on the badge |
