# Camera: the ESP32-CAM module

*Version française : [appareil_photo.md](../fr/appareil_photo.md).*

An **ESP32-CAM** (AI-Thinker: ESP32, OV2640 2 Mpx camera, 4 MB PSRAM, flash LED) plugged on an extension port (SAO) turns
the cicada into a camera: **Badge > Appareil photo** (Camera). The e-Paper is the viewfinder, the photos and the videos go
to the SD card of the badge.

- **The firmware of the ESP32**: [esp32cam/](../../esp32cam) (PlatformIO, Arduino).
- **The link**: [src/shared/camlink/](../../src/shared/camlink) (shared by both firmwares, tested on a PC).
- **The page of the badge**: [src/menu/camera.c](../../src/menu/camera.c).

Contents: [1. How it works](#1-how-it-works) · [2. The hardware](#2-the-hardware) (programming the module,
[plugging it on the cicada](#plugging-the-esp32-cam-on-the-cicada-sao-ports-of-the-v12-board)) ·
[3. On the badge](#3-on-the-badge-badge--appareil-photo) · [4. The web remote control](#4-the-web-remote-control-badge--télécommande-remote-control) ·
[5. The link](#5-the-link-srcsharedcamlink) · [6. On the ESP32](#6-on-the-esp32-esp32cam) · [7. Tests](#7-tests) ·
[8. Troubleshooting](#8-troubleshooting) · [9. To be completed](#9-to-be-completed)


## 1. How it works

The badge cannot read a camera: the ESP32 of the module does all the picture work. The badge gives it the orders,
shows what it receives and writes it on its SD card. Both talk through a **serial link** (2 wires, 921,600 bauds):

```
  ┌──────────────────── ESP32-CAM ────────────────────┐                ┌──────────────── badge (RP2040) ───────────────┐
  │ OV2640 camera ──JPEG──► ESP32 (240 MHz, 4 MB PSRAM)│   serial       │ camport.c (the service of the link)           │
  │                         · decodes, shrinks to 200² │   link         │   ├─ camera.c: the Camera page                │
  │ flash LED (IO4) ◄──     · dithers to 1 bit / 4 gray│ ◄────────────► │   │   ├─► e-Paper screen (the viewfinder)     │
  │                         · keeps the JPEG of a photo│  U0T → RX      │   │   └─► SD card: PHOTOS, IMAGES, VIDEOS     │
  │ Wi-Fi ◄──► phone        · web server (remote)      │  U0R ← TX      │   └─ webremote.c: the Remote control          │
  └────────────────────────────────────────────────────┘ + 3.3 V, ground└───────────────────────────────────────────────┘
```

- **The search**: when the page opens, the badge tries each possible serial link of its extension ports, one per
  second, saying hello (HELLO); the module answers with its name and version, the badge keeps that link. Nothing to
  set: the module may be plugged on either port (§ 2).
- **The viewfinder**: the badge asks for the viewfinder (PREVIEW); the module takes 320 × 240 JPEG pictures, decodes
  them, keeps the central square shrunk to 200 × 200, stretches the contrast and dithers it to black and white (5,000
  bytes), sent in 5 packets. The badge shows each picture with a fast refresh of the e-Paper: **2 to 3 pictures per
  second**.
- **The photo** (B): the module switches to the size of the settings, lights the flash if asked, drops 3 pictures (the
  time for the exposure to settle), sends the **JPEG** (the badge writes it in `PHOTOS/`), then a 200 × 200 version in
  **4 grays** (`.epi`, in `IMAGES/PHOTOS/`) to view it on the badge. About **2 s** from the button to the file.
- **The video** (long B): the module sends one dithered picture per period (Bayer: stable from one picture to the
  next, less ghosting); the badge adds them to the `.epv` file (`VIDEOS/PHOTOS/`) while showing them. **~3.5 frames
  per second**, no sound.
- **Nothing is lost**: each data packet waits for the acknowledgement of the other side (sent again after 300 ms); the
  badge may be busy with its screen or its SD card meanwhile.
- **The remote control** (§ 4) uses the same link: the badge sends its screen to the module, the module sends back the
  buttons pressed on the web page.


## 2. The hardware

### The pins of the ESP32-CAM

Two rows of 8 pins; the names are printed on the back of the board (check yours):

| Row 1 | Row 2 |
|---|---|
| 5V | 3V3 |
| GND | IO16 (PSRAM: do not use) |
| IO12 | IO0 (to ground at boot: programming mode) |
| IO13 | GND |
| IO15 | VCC (3.3 V or 5 V output depending on a jumper: do not power it there) |
| IO14 | U0R (IO3: receive of the UART0) |
| IO2 | U0T (IO1: transmit of the UART0) |
| IO4 (flash LED) | GND |

The UART0 (U0R, U0T) is used both to **program** the ESP32 and to **talk to the badge**.

### Programming the ESP32-CAM with the ESP Flasher (Rev6)

The ESP32-CAM has no USB port: it is programmed through its UART0. The **ESP Flasher Rev6** (KSEC / April Brother) is a
USB-C → serial adapter (**CH340**) at **3.3 V** (MIC5219 regulator, ~400 mA), with a RESET button, a FLASH button
(GPIO0) and an automatic boot circuit on its RST and GPIO0 pins.

**With the row of pins** (the names are printed next to each pin):

| ESP Flasher | ESP32-CAM | |
|---|---|---|
| VCC | **3V3** | the flasher gives 3.3 V: never on the 5V pin of the ESP32-CAM |
| GND | GND | |
| TXD | U0R (IO3) | the transmit of one on the receive of the other |
| RXD | U0T (IO1) | |
| GPIO0 | IO0 | the programming mode, driven by the flasher (or its FLASH button) |
| RST | — | not connected: the ESP32-CAM has no reset pin, only its RST button |
| CH_EN | — | not connected |

**With the 2 × 4 block**: it is the socket of an ESP-01 module (ESP8266). The usual pinout of the ESP-01, seen from the
top, the antenna up: first row GND, GPIO2, GPIO0, RXD; second row TXD, CH_PD, RST, VCC (RXD and TXD of the module).
Wire VCC → 3V3, GND → GND, RXD (the receive of the module) → U0R, TXD (the transmit of the module) → U0T, GPIO0 → IO0;
GPIO2, CH_PD and RST not connected. The maker does not publish the pinout of the block: **check it with a multimeter**
(continuity between each pin of the block and the pin of the same name on the row) before wiring; when in doubt, use
the row.

**Programming**:

1. Plug the flasher in USB: it shows as a serial port (CH340: driver included in Windows 10/11, macOS, Linux).
2. Find its port (`pio device list`: "USB-SERIAL CH340") and put it in `upload_port` and `monitor_port` of
   `esp32cam/platformio.ini`. Without it, PlatformIO takes the first port it sees, often a badge.
3. `cd esp32cam`, then `pio run -t upload` (PlatformIO: `pip install platformio`; if `pio` is not found, it is in
   `%USERPROFILE%\.platformio\penv\Scripts` on Windows, `~/.platformio/penv/bin` elsewhere).
4. When PlatformIO shows "Connecting...": hold the **FLASH** button of the flasher (IO0 to ground) and press the
   **RST** button of the ESP32-CAM, then release. The flasher cannot restart the ESP32-CAM by itself: its RST pin is
   not connected.
5. At the end, press **RST** on the ESP32-CAM: the firmware starts.

The 3.3 V of the flasher (~400 mA) is enough to program and try the camera on the desk, **without the flash** (the flash
LED draws too much).

### Plugging the ESP32-CAM on the cicada (SAO ports of the V1.2 board)

The V1.2 board has two **SAO** extension ports (2 × 3 pins at 2.54 mm pitch), from its schematic
([hardware/badge_v1.2.pdf](../../hardware/badge_v1.2.pdf)):

| Pin | SAO0 | SAO1 |
|---|---|---|
| 1 | **ground** | **ground** |
| 2 | **+3.3 V** | **+3.3 V** |
| 3 | I2C0 SCL (GPIO17) | I2C1 SCL (GPIO3) |
| 4 | I2C0 SDA (GPIO16) | I2C1 SDA (GPIO2) |
| 5 | GPIO19 | UART0 RX (GPIO13): the receive of the badge |
| 6 | GPIO20 | UART0 TX (GPIO12): the transmit of the badge |

```
   SAO port (numbers of the schematic)   pin 1 is marked on the board (square pad or mark)
   ┌──────────┬─────────┐
   │ 1 ground │ 2 3.3 V │
   ├──────────┼─────────┤
   │ 3    SCL │ 4   SDA │
   ├──────────┼─────────┤
   │ 5        │ 6       │
   └──────────┴─────────┘
```

> **Mind the power**: on this board, pin 1 is **ground** and pin 2 is **+3.3 V** (the opposite of the usual SAO
> standard). Before plugging, check with a multimeter: continuity between pin 1 and the ground of the badge (the shield
> of the USB-C socket, for example), 3.3 V between pins 2 and 1 with the badge on. A reversal destroys the ESP32-CAM.

**Two ways to plug the module**, either one; the badge finds by itself the one that is wired (4 wires in both cases):

| ESP32-CAM | On SAO1 (advised) | On SAO0 |
|---|---|---|
| **3V3** (never 5V: the badge has no 5 V) | pin 2 | pin 2 |
| **GND** | pin 1 | pin 1 |
| **U0R** (IO3, the receive of the module) | pin 6 (UART0 TX, GPIO12) | pin 4 (SDA, GPIO16) |
| **U0T** (IO1, the transmit of the module) | pin 5 (UART0 RX, GPIO13) | pin 3 (SCL, GPIO17) |
| Log of the badge | `on UART0 GPIO12/13` | `on UART0 GPIO16/17` |

```
   SAO1 of the badge                 ESP32-CAM
   1 ground ●────────────────────── GND
   2 3.3 V  ●────────────────────── 3V3
   5 RX     ●────────────────────── U0T (IO1)    the transmit of one on the receive of the other
   6 TX     ●────────────────────── U0R (IO3)
   3, 4     (free)                  IO0: not connected
```

- **SAO1**: its pins 5 and 6 are the UART0 of the RP2040, made for a serial link; SAO0 stays free for the infrared
  board (GPIO17, GPIO20). Pins 3 and 4 of SAO1 (I2C1) are for the OLED screen: no OLED on that port at the same time.
- **SAO0**: the badge turns the I2C0 pins into a serial link (GPIO16 transmits, GPIO17 receives); it is the wiring
  tried first. No infrared board on that port at the same time.
- **IO0 not connected** (it has a pull-up on the board): the ESP32 starts its firmware.
- **Current**: the camera and the PSRAM draw ~150-250 mA, the Wi-Fi of the remote control peaks of ~300 mA; the 3.3 V
  regulator of the badge (TPS63000) gives ~1 A. The **flash LED** draws a lot: setting "Flash" at "non" (off) by
  default.
- At boot, the ESP32 writes its boot text on the UART0 (115,200 bauds): the badge ignores it (COBS frames).
- The log of the badge (USB serial port) says where the module was found: `camera: module esp32cam 0.2, protocol 2, on
  UART0 GPIO12/13`.
- **V1.0 / V1.1 boards** (2 × 6 headers J2, J3: [electronics.md § 10](electronics.md#v10-v11-boards-j2-j3)): U0R on
  J2 pin 9 (UART0 TX, GPIO12), U0T on J2 pin 7 (UART0 RX, GPIO13), 3V3 on J2 pin 1, GND on J2 pin 11.

### Sharpness (focus)

The OV2640 of the ESP32-CAM has a **fixed focus**: no autofocus, nothing to set by the program. The focus is set
**by hand**, by screwing or unscrewing the lens:

1. Remove the clear protective film on the lens, if it is still there (a frequent cause of blurry pictures).
2. The lens is often locked by a dab of glue: hold it by its knurled ring with fine pliers and turn gently to break
   the glue (without pulling on the ribbon cable of the sensor).
3. Turn in small steps while looking at the viewfinder of the badge: counter-clockwise, the focus comes closer
   (portraits, objects at 20-50 cm); clockwise, further away.
4. Once sharp, a drop of nail polish or glue locks it, if needed.

The viewfinder is small (200 × 200, 1 bit): check on a JPEG photo (`PHOTOS` folder, on the computer). In low light,
the exposure is long and the photo blurred by motion: add light, or the "Flash" setting.


## 3. On the badge: Badge > Appareil photo

- **Looking for the module**: the badge says hello every second; the module answers (its name and version).
- **The viewfinder**: the picture of the camera on the whole screen, redrawn as soon as it comes (fast refresh of the
  e-Paper, ~2-3 pictures per second). A band at the bottom: what the buttons do, or the last message.
- **B**: a **photo**. The module sends the JPEG at the size chosen, then a 200 × 200 version in 4 gray levels:
  - `PHOTOS/IMG_0001.JPG`: the photo (for a computer);
  - `IMAGES/PHOTOS/IMG_0001.EPI`: the same, to view on the badge (**Médias > Images**, folder PHOTOS).
    A long press on B, in the list or on the photo shown: rename it or delete it (with its JPEG).
- **Long B**: a **video** (long B again: stop). 4 frames per second, black and white, no sound:
  `VIDEOS/PHOTOS/VID_0001.EPV`, to view in **Médias > Vidéos**.
- **X**: the **settings** (kept); Y / X choose, B change the value, A back to the viewfinder: photo size (320x240 to
  1600x1200, 800x600 by default), JPEG quality, brightness, contrast, saturation (-2 to +2), effect (none, negative,
  gray, red, green, blue, sepia), mirror, flip, flash (off, for the photo, always: a lamp), dither of the viewfinder
  (Atkinson by default, Floyd-Steinberg, Bayer, threshold).
- **A**: leave (stops the video being recorded).


## 4. The web remote control: Badge > Télécommande (Remote control)

The ESP32-CAM module serves a web page that drives the badge from a phone (or a computer): the **screen of the
badge** live (the 4 grays included), its **4 buttons Y A B X** (long press: hold the button) and a **text field** for
its editors (name, messages...). The page only sends the buttons and the text: no SD card, no reboot, no admin mode,
unlike the USB port.

**Start**: Badge > Télécommande > "Remote control: yes". It stays on in the background (A goes back to the menus,
you browse everything from the phone) until "Remote control: no" or a restart of the badge.

**The access point (default)**: the module creates its own Wi-Fi "Cigale-<name>", protected by a password drawn at
random at each start. "Connect" shows a **Wi-Fi QR code**: the phone scans it, joins the network and the page opens
by itself (captive portal); otherwise: `http://192.168.4.1`. That password is enough: whoever reads it on the screen
of the badge already holds the badge.

**A network**: on the page (from the access point), "Wi-Fi du module": the name (SSID) and the password of a
network, and an optional admin key; the module keeps them. Then on the badge: "Wi-Fi: network". "Connect" shows the
address of the module on that network (and its QR code) and a **6-digit code** (new at each start) that the page asks
before driving the badge: the other people of the network cannot drive it. The name `http://cigale-<name>.local`
works too on most computers. When the network does not answer (15 s), the module goes back to its access point
("Network not found"), to fix the settings.

**The admin page** (`/admin` on any module of the network): the screen of **every cicada of the network** side by
side, read only. The modules tell each other they are there (UDP, port 4210); the page asks the admin key, the same
on every module (set in "Wi-Fi du module").

**Updating the module** from the SD card of the badge: copy `esp32cam/.pio/build/esp32cam/firmware.bin` (or the file
of the distribution package) as **`ESP32/ESP32CAM.BIN`** on the card, then Badge > Télécommande > "Update the module"
> B. The badge sends it through the link (~1 MB, about a minute), the module checks it then restarts on it; its new
version shows up. The module must already run a firmware with the remote control (protocol 2): the first time,
flash it with the ESP Flasher (§ 2).

- **Current**: the Wi-Fi draws peaks of ~300 mA; the battery of the badge empties faster, stop the remote control
  when it is not used.
- The badge sends its screen only while a page looks at it.


## 5. The link (src/shared/camlink)

- UART at **921,600 bauds**, 8N1.
- Packet: `[type][seq][length on 2 bytes][data][CRC-16]`, **COBS** encoded, ended by a 0 byte.
- The data of the module (viewfinder, files, video) waits for the **ACK** of the badge before the next packet (sent
  again after 300 ms): the badge may be busy with its screen or its SD card without losing anything.
- A frame of the viewfinder: 200 × 200 at 1 bit = 5,000 bytes, in 5 packets of 1,000.
- The types: HELLO, ACK, SETTINGS, PREVIEW, FRAME, SHOT, FILE_OPEN / FILE_DATA / FILE_CLOSE, VIDEO, VFRAME, STATUS,
  and for the remote control WEB, WEB_STATE, SCREEN, KEY, OTA_BEGIN / OTA_DATA / OTA_END (details in
  [camlink.h](../../src/shared/camlink/camlink.h)).
- On the badge, the link is a service shared by the camera and the remote control (`src/menu/camport.c`); without
  the camera page its receiver is small (96 bytes: the keys, the state of the Wi-Fi), for the RAM.
Host test: `python src/tests/host/run_tests.py camlink` (COBS round trip, errors, recovery after the boot text,
dithers).


## 6. On the ESP32 (esp32cam/)

- The camera in **320 × 240 JPEG** for the viewfinder: decoded to grays, the central square shrunk to 200 × 200,
  contrast stretched, dithered to 1 bit (the dither chosen), sent.
- The **photo**: the size switches to the one of the settings, the flash lights if asked, 3 pictures dropped (the
  exposure settles), the JPEG is sent; then the viewfinder goes back to 320 × 240 and its picture in 4 grays
  (Floyd-Steinberg) goes as an `.epi`.
- The **video**: one picture per period (4 per second), Bayer dithered (stable from one picture to the next: less
  ghosting on the e-Paper).
- The **remote control**: the Wi-Fi (access point with a captive portal, or a network saved in the flash:
  `Preferences` "webremote"), a web server in its own task (`esp_http_server`): `/` the page, `/screen`, `/key`,
  `/type`, `/info`, `/config`, `/admin`, `/peers.json`; the pages in `esp32cam/src/web_pages.h`.
- The **update**: the `Update` library, in the other OTA partition (`min_spiffs.csv`: two applications of 1.9 MB).
- Nothing else on the UART0 (`CORE_DEBUG_LEVEL=0`).


## 7. Tests

1. Program the ESP32-CAM (§ 2), plug it on an SAO port (4 wires, § 2).
2. Badge > Appareil photo: "Module found", then the viewfinder.
3. A photo (B): the message "Photo IMG_0001"; Médias > Images > PHOTOS.
4. A video (long B, a few seconds, long B): Médias > Vidéos > PHOTOS.
5. The log of the badge (USB serial port): `camera: module ...`, `camera: PHOTOS/IMG_0001.JPG open`,
   `camera: photo 1 saved`, `camera: video saved, N frames`.


## 8. Troubleshooting

| What you see | Why, and what to do |
|---|---|
| "Looking for the module" forever | No answer on any link: U0R and U0T swapped (cross them: the transmit of one on the receive of the other), a wire on the wrong pin, the module without firmware (program it, § 2), IO0 tied to ground (programming mode). The log of the badge lists the links tried. |
| The module found, then lost; the module restarts | Power too weak: long or thin wires, low battery, the flash. Plug the badge in USB to try, shorten the wires, "Flash: off". |
| "Camera missing" | The ribbon of the camera badly inserted in its socket (the black flap must be closed), or a faulty camera. |
| Blurry picture | The protective film on the lens, or the focus (§ 2, "Sharpness"). |
| Very dark or very bright picture | Little light (long exposure); the Brightness, Contrast settings; the flash. |
| Slow viewfinder | Normal: 2 to 3 pictures per second, the limit of the fast refresh of the e-Paper. |
| Photo not saved | No SD card, or a full or locked card: Médias must be able to read it. |
| The remote control does not open | See § 4: the module needs the firmware of protocol 2 (`esp32cam 0.2` in the log). |


## 9. To be completed

- The current during a photo with the flash.
- The wiring on SAO1 (UART0 GPIO12/13) has not been tried on a V1.2 board yet.

**Tried on a V1.2 cicada (2026-10-19)**: the module found on **UART0 GPIO16/17** (pins 3 and 4 of SAO0); the
viewfinder; an 800 × 600 photo in **2.1 s** from the button to the file (JPEG 17 KB and its .epi); a video of **~3.5
frames per second** (32 frames in 9 s).
