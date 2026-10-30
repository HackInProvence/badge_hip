# Camera: the ESP32-CAM module

*Version française : [appareil_photo.md](../fr/appareil_photo.md).*

An **ESP32-CAM** (AI-Thinker: ESP32, OV2640 2 Mpx camera, 4 MB PSRAM, flash LED) plugged on an extension port (SAO) turns
the cicada into a camera: **Badge > Appareil photo** (Camera). The e-Paper is the viewfinder, the photos and the videos go
to the SD card of the badge.

- **The firmware of the ESP32**: [esp32cam/](../../esp32cam) (PlatformIO, Arduino).
- **The link**: [src/shared/camlink/](../../src/shared/camlink) (shared by both firmwares, tested on a PC).
- **The page of the badge**: [src/menu/camera.c](../../src/menu/camera.c).


## 1. The hardware

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

### Plugging the ESP32-CAM on the cicada (SAO extension port)

The extension ports of the cicada are **SAO** ports (2 × 3 pins, keyed): 1 3.3V, 2 GND, 3 SDA, 4 SCL, 5 GPIO1,
6 GPIO2.

The badge does not need to know where the ESP32-CAM is plugged: while looking for it, it tries each possible serial
link of the ports (one per second, shown on the screen) and keeps the one that answers: UART0 on GPIO16/17 (the SDA and
SCL of the I2C0: pins 3 and 4), UART1 on GPIO20/21 (pins 5 and 6 if these are the GPIOs), UART0 on GPIO12/13 (the old
2 × 6 port of the V1.1 board).

| SAO port | ESP32-CAM |
|---|---|
| 1 (3.3 V) | **3V3** (not 5V: the badge has no 5 V) |
| 2 (GND) | GND |
| 3 (SDA) or 5 (GPIO1): the transmit of the badge | U0R (IO3) |
| 4 (SCL) or 6 (GPIO2): the receive of the badge | U0T (IO1) |

- **IO0 not connected** (it has a pull-up on the board): the ESP32 starts its firmware.
- **Current**: the camera and the PSRAM draw ~150-250 mA; the 3.3 V regulator of the badge (TPS63000) gives ~1 A.
  The Wi-Fi is not used. The **flash LED** draws a lot: setting "Flash" at "non" (off) by default.
- The infrared board and the OLED screen must not be plugged on the same port.
- At boot, the ESP32 writes its boot text on the UART0 (115,200 bauds): the badge ignores it (COBS frames).
- The log of the badge (USB serial port) says where the module was found: `camera: module esp32cam 0.1, protocol 1,
  on UART0 GPIO16/17`.

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


## 2. On the badge: Badge > Appareil photo

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


## 3. The link (src/shared/camlink)

- UART at **921,600 bauds**, 8N1.
- Packet: `[type][seq][length on 2 bytes][data][CRC-16]`, **COBS** encoded, ended by a 0 byte.
- The data of the module (viewfinder, files, video) waits for the **ACK** of the badge before the next packet (sent
  again after 300 ms): the badge may be busy with its screen or its SD card without losing anything.
- A frame of the viewfinder: 200 × 200 at 1 bit = 5,000 bytes, in 5 packets of 1,000.
- The types: HELLO, ACK, SETTINGS, PREVIEW, FRAME, SHOT, FILE_OPEN / FILE_DATA / FILE_CLOSE, VIDEO, VFRAME, STATUS
  (details in [camlink.h](../../src/shared/camlink/camlink.h)).

Host test: `python src/tests/host/run_tests.py camlink`.


## 4. To be completed

- The GPIOs really on each SAO port of the V1.2 board (its schematic is not in the repository): the automatic search
  covers the possible cases; to confirm for the documentation.
- The current during a photo with the flash.

**Tried on a V1.2 cicada (2026-10-19)**: the module found on **UART0 GPIO16/17** (pins 3 and 4 of the SAO port); the
viewfinder; an 800 × 600 photo in **2.1 s** from the button to the file (JPEG 17 KB and its .epi); a video of **~3.5
frames per second** (32 frames in 9 s).
