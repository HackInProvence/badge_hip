# Camera: the ESP32-CAM module

*Version française : [appareil_photo.md](../fr/appareil_photo.md).*

An **ESP32-CAM** (AI-Thinker: ESP32, OV2640 2 Mpx camera, 4 MB PSRAM, flash LED) plugged on the port **J2** turns the
cicada into a camera: **Badge > Appareil photo** (Camera). The e-Paper is the viewfinder, the photos and the videos go
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

### Programming the ESP32-CAM

The ESP32-CAM has no USB port: it is programmed through its UART0, with a USB-serial adapter (here the **ESP
Flasher v6.1**). The exact wiring of the ESP Flasher (its row of pins, its 2 × 4 block) is still to be written: see
§ 6.

The principle, with any **3.3 V** USB-serial adapter:

| Adapter | ESP32-CAM |
|---|---|
| 5V | 5V (the ESP32-CAM has its own 3.3 V regulator; the camera and the Wi-Fi draw current) |
| GND | GND |
| TX | U0R |
| RX | U0T |
| — | IO0 tied to GND **while it boots** (a wire or a jumper) |

1. Tie IO0 to GND, plug the adapter (or press the RST button of the ESP32-CAM): programming mode.
2. `cd esp32cam`, then `pio run -t upload --upload-port COMx` (PlatformIO: `pip install platformio`).
3. Remove the IO0-GND wire, press RST: the firmware starts.

### Plugging the ESP32-CAM on the cicada (port J2)

| J2 (cicada) | Signal | ESP32-CAM |
|---|---|---|
| 1 | +3.3 V | **3V3** (not 5V: the badge has no 5 V) |
| 11 or 12 | GND | GND |
| 9 | UART0 TX (GPIO12) | U0R (IO3) |
| 7 | UART0 RX (GPIO13) | U0T (IO1) |

```
   Cicada, port J2 (2 x 6)                ESP32-CAM
   ┌─────────────────────┐
   │  1 +3.3V ●──────────┼──────────────── 3V3
   │  7 RX    ●──────────┼──────────────── U0T (IO1)
   │  9 TX    ●──────────┼──────────────── U0R (IO3)
   │ 11 GND   ●──────────┼──────────────── GND
   └─────────────────────┘
```

- **IO0 not connected** (it has a pull-up on the board): the ESP32 starts its firmware.
- **Current**: the camera and the PSRAM draw ~150-250 mA; the 3.3 V regulator of the badge (TPS63000) gives ~1 A.
  The Wi-Fi is not used. The **flash LED** draws a lot: setting "Flash" at "non" (off) by default.
- **Port J2** also carries the infrared board (GPIO17, GPIO20): both at once need a Y cable or a common extension
  board.
- At boot, the ESP32 writes its boot text on the UART0 (115,200 bauds): the badge ignores it (COBS frames).


## 2. On the badge: Badge > Appareil photo

- **Looking for the module**: the badge says hello every second; the module answers (its name and version).
- **The viewfinder**: the picture of the camera on the whole screen, redrawn as soon as it comes (fast refresh of the
  e-Paper, ~2-3 pictures per second). A band at the bottom: what the buttons do, or the last message.
- **B**: a **photo**. The module sends the JPEG at the size chosen, then a 200 × 200 version in 4 gray levels:
  - `PHOTOS/IMG_0001.JPG`: the photo (for a computer);
  - `IMAGES/PHOTOS/IMG_0001.EPI`: the same, to view on the badge (**Médias > Images**, folder PHOTOS).
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

- The pinout of the **ESP Flasher v6.1** (its row of pins and its 2 × 4 block): exact wiring, and whether it drives IO0
  and the reset by itself.
- The measures: frames per second of the viewfinder, current during a photo with the flash, time to send a photo.
