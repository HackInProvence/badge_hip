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
├── radio/      CC1101 driver
├── ir/         infrared: reception, 38 kHz transmission, NEC decoding
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
└── badge_selftest.py   automated badge test over USB
docs/           documentation, ideas, PVSR
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
    ... button handling depending on the page shown (app) ...
    radio_tools_task(now); social_task(now); battery_task(now); store_task(now); ...
    switch (app) { ... video_task(now) / rsvp_task(now) / games_task(now) / display_task(now) ... }
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
| `main.c` | themed menus (`SUBMENUS`), SD browser, players, settings, screen saver, cicada name, CTF, USB protocol |
| `display.c` | non-blocking display of a frame buffer (fast or full refresh, display sleep) |
| `games.c` | the mini games, hardware-independent (sound, LEDs, randomness supplied through *hooks*) |
| `rsvp.c` | PVSR fast reading ([docs/pvsr.md](../pvsr.md)) |
| `social.c` | cicada network (radio beacons, encounters, score) |
| `radio_tools.c` | radio message and carrier, crystal measurement |
| `credits.c` | credits pages |
| `score_code.c` | signed scores shown as a QR code (§ 6.11) |
| `battery.c` | battery level (calibration) |
| `store.c` | settings and scores in flash |
| `ctf.c`, `oled_demo.c`, `screen_demo.c` | CTF, OLED demos, display demo |

### Adding a menu entry

1. Add a value to `menu_item_t` and its label in `item_label()`.
2. Put it in a `SUBMENUS` theme (at most 8 entries per theme).
3. In `validate()`, run the action or switch to a new `app` state (`app_state_t`).
4. If the state has its own page:
   - handle its buttons in the loop;
   - draw it inside `if (redraw)`;
   - handle going back in `cancel()`.
5. Call `draw_title()` (or `ui_trace()`) so that the page is logged as `ui: <title>` over USB,
   which the automated tests rely on.

### Adding a game

Games ([games.h](../../src/menu/games.h)) depend only on `gfx`. A game provides 5 functions:
- `*_buttons()`;
- `*_task()`;
- `*_render()`;
- a start function;
- an "idle" state for `games_calm()`.

It hooks them into the `switch` statements of the API. High scores live in `store_t.game_records` (0xFFFF = none).
Then add tests in [tests/host/test_games.c](../../src/tests/host/test_games.c).


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
  the ghost comes back a while after the image. The screensaver cleans in black then white before its image.
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
- **Cicada network** (`social.c`, see [docs/idees_reseau_extensions_ctf.md](../idees_reseau_extensions_ctf.md)):
  - one beacon every 2 s ± 0.5 s at −20 dBm, sync word 0xC16A;
  - 17-byte packet: `0xC1`, type 1, identifier (FNV hash of the unique ID), sequence number, score, name (8 characters);
  - an encounter = RSSI ≥ −50 dBm on 3 beacons within 10 s;
  - +10 points for a new badge, +1 for a known badge, at most once per hour.

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
- game high scores.

How it works:
- the write happens 5 s after the last change, using `flash_safe_execute` (about 50 ms, interrupts disabled);
- new fields are appended **at the end** of the structure: in a sector written by an earlier version,
  they read as 0xFF, which their users check for;
- a `_Static_assert` guarantees that the structure fits in the sector.

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


## 7. File formats

| Format | Content |
|---|---|
| `.EPI` (image) | 16-byte header: `EPIMAGE1`, width, height, bits per pixel (1 or 2), 0 (little-endian uint16); then the "lsb" plane, then the "msb" plane (if 2 bits). Each plane is 5000 bytes, bit 7 = leftmost pixel, row by row. Gray = msb×2 + lsb (0 = black, 3 = white). |
| `.EPV` (video) | 512-byte header: `EPVIDEO2`, width, height, fps, bits per pixel, frame count, audio sample rate, sample count; then the 5000-byte frames (1 = white); then the sound, 8-bit unsigned, synchronized with the first frame. `EPVIDEO1` = no sound. |
| `.WAV` | 8- or 16-bit PCM, mono or stereo; 8-bit 16 kHz mono recommended (`audio2wav.py`). |
| `.TXT` | UTF-8 (with or without BOM) or Windows-1252, detected automatically. |

Images embedded in the firmware (display demo, logos) are converted to C arrays at build time by `image2epaper.py`
(CMake function `badge_image2epaper`).


## 8. The USB serial protocol

The badge shows up as a USB serial port (115200 baud, irrelevant over USB). One key per character:

| Key | Action |
|---|---|
| `a` `b` `x` `y` | short press: left wing, right wing, right flank, left flank |
| `B` `X` `Y` | long press (save the name, fast-reading forward / back) |
| `[` / `]` | send the screen on every change: on / off |
| `s` | send the screen once |
| `!` | diagnostic: OLED, IR, cicada network, CTF, radio (version, crystal), battery |
| `?` | sound diagnostic |
| `i` | IR test: decode an NEC frame then transmit it (about 68 ms) |
| `R` | reboot (watchdog) |

The badge sends lines of text:
- `ui: <title>` on every page change;
- `browser:`, `image:`, `saver: on/off`, `radio:`, `social:`, `game:`, `ctf: code right/wrong`, `credits:`, `name:`...

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
| `score` | SipHash (reference vectors), score text and signature, QR code drawing |
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
2. It simulates button presses and checks the traces: menus, games, screen saver, credits, radio, beacons, IR, SD card images.
3. It writes a PASS / FAIL / SKIP table and PNG screenshots (`selftest_<date>` folder).

A test is SKIPped when the hardware is missing (no SD card, uncalibrated battery).
No badge setting is changed, except with `--ctf`.

### 9.3 Per-module test applications

`src/tests/*.c`: one application per module (display, radio, LEDs, sound...), to flash in order to explore a peripheral.


## 10. Tools

| Script | Usage |
|---|---|
| `tools/badge_remote.py` | window showing the badge screen enlarged (zoom 2–4), arrows / Enter / Esc = buttons, PNG capture; `--snapshot file.png` for a single capture |
| `tools/badge_selftest.py` | automated badge test (§ 9.2) |
| `tools/score_check.py` | checks the score QR codes and ranks them (§ 6.11) |
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
