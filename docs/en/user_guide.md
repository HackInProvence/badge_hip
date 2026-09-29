# SecSea badge — user guide

This guide explains how to use the Hack In Provence SecSea badge: turning it on, finding your way around the menus,
enjoying the media, games, radio and extensions, and preparing a micro-SD card.
To modify the badge software, see the [developer guide](developer_guide.md).

*Version française : [guide utilisateur](../fr/guide_utilisateur.md).*


## 1. The badge at a glance

- A 200 × 200 pixel e-Paper (electronic ink) screen, in black and white or 4 shades of grey.
  The image stays on screen even without power.
- 4 buttons, 2 colour LEDs, a buzzer.
- A 433 MHz radio (CC1101) compatible with the Flipper Zero.
- A micro-SD card reader for videos, music, texts and images.
- Two extension ports (infrared, OLED screen...).
- A battery you can recharge over USB-C.


## 2. Getting started

1. Set the switch to **ON**. On **OFF**, the badge does not start, even when plugged into USB
   (only charging works: red LED while charging, green when the battery is full).
2. The main menu appears after a few seconds.

The e-Paper screen is slow (about 0.3 s to change the image) and sometimes flashes black and white:
this is normal, it regularly "cleans" itself to avoid ghosts of previous images.


## 3. The buttons

Hold the badge facing you, with the cicada's head at the top.

| Button | In the menus | Usual role |
|---|---|---|
| **Right wing** (R) | select | OK, pause, start |
| **Left wing** (L) | back | cancel, quit |
| **Left flank** | up | previous, less |
| **Right flank** | down | next, more |

The screens are in French. The bottom of each screen reminds you of the useful buttons, for example
"Flancs : choix  D : OK  G : retour" (Flanks: choose, R: OK, L: back). On screen, "D" (*droite*) is the right wing
and "G" (*gauche*) the left wing.
In the main menu, the left wing also mutes the cicada's song and turns off the LEDs.


## 4. The menus

The main menu groups the features by theme:

| Theme | Contents |
|---|---|
| **Médias** (Media) | "Images", "Vidéos" (Videos), "Musique" (Music), "Lecture rapide" (Speed reading), "Volume" |
| **Jeux** (Games) | Morpion (tic-tac-toe), Puissance 4 (Connect Four), Simon, Réflexes (Reflexes), Snake, Blind test, CTF |
| **Badge** | Cicada song, LED animations, screen demo, OLED screen |
| **Radio & IR** | Cicada network, radio message, radio carrier, infrared |
| **Réglages** (Settings) | "Veille de l'écran" (Screen sleep), "Infos" (Info), "Crédits" (Credits) |


### 4.1 Médias (Media)

Media files are read from the SD card (see [§ 7](#7-preparing-the-sd-card)).
Each player shows the list of files; subfolders are marked with "> ".

**Images** (`IMAGES` folder)
- Flanks: previous / next image.
- Right wing: use this image as the sleep screen image.
- Left wing: back to the list.

**Vidéos** (Videos, `VIDEOS` folder)
- Right wing: pause / resume; flanks: volume; left wing: stop.
- Sound comes out of the buzzer: recognisable, but not hi-fi.

**Musique** (Music, `MUSIQUE` folder)
- Right wing: pause; left wing: stop; left flank: quieter; right flank: louder.

**Lecture rapide** (Speed reading, `TEXTES` folder)
- Words appear one at a time in the same spot, so your eyes no longer move: you read much faster
  (PVSR method, see [pvsr.md](../pvsr.md)).
- Right wing: pause / resume; left wing: quit.
- Flanks (short press): speed −/+ (from 100 to 900 words per minute, 250 at start).
- Flanks (long press): go back / forward about 10 seconds of reading.
- Your position is remembered: a text resumes where you left it.

**Volume**: each press on the right wing raises the volume (0 to 8, then back to 0) and plays a short chime.


### 4.2 Jeux (Games)

In every game (except Simon), **the left wing quits the game**.

| Game | How to play |
|---|---|
| **Morpion** (tic-tac-toe) | Flanks: choose the square; right wing: play. You are the crosses, the cicada the noughts. You take turns starting. The cicada makes a mistake now and then... |
| **Puissance 4** (Connect Four) | Flanks: choose the column; right wing: drop the counter. Line up 4 solid counters before the cicada's rings. |
| **Simon** | Each button is an area of the screen, placed like the button on the badge (flanks at the top, wings at the bottom), with its own colour and note. Repeat the sequence, which gets longer each round. Here the left wing is part of the game: to quit, wait until the game is over, then press the left wing. |
| **Réflexes** (Reflexes) | When the LEDs light up green (with a beep), quickly press the right wing or a flank. 5 tries, the average counts. Too early: the try starts over. |
| **Snake** | Right wing: start / pause. Left flank: turn left; right flank: turn right (relative to the snake's direction). |
| **Blind test** | Pick a music folder: tracks play in random order, without repeats. Right wing: show the title; right flank: next track; left flank: pause. |
| **CTF** | A challenge: enter a famous video game code with the 4 buttons to get a "flag". |

High scores for Simon, Réflexes and Snake are kept even after the badge is switched off.

**Score as a QR code**: at the end of a game, press a flank to show a QR code with the game, the score, the name
of your cicada and a signature: the organizers can scan it for a leaderboard, and a score edited by hand is rejected.
Any button closes the QR code.

At the bottom of the screen, the buttons are listed in the order they are on the badge: G (left wing) on the left,
D (right wing) on the right.


### 4.3 Badge

- **Cigale** (Cicada): turns the cicada's song on or off.
- **LEDs**: changes the animation (rainbow, breathing, heartbeat, blinking, steady green, off).
- **Démo écran** (Screen demo): shows what the screen can do (black and white, 4 greys, fast animation).
- **Écran OLED** (OLED screen): demos on a small OLED screen plugged into the left port (stars, 3D cube, cicada, text, video).


### 4.4 Radio & IR

**Réseau des cigales** (Cicada network)
- Badges that are switched on quietly send each other radio signals.
- Hold your badge very close to another one (a few centimetres) for a few seconds: you earn
  **10 points** for a new encounter, **1 point** for a cicada you have already met (at most once per hour).
- Right wing: choose your **cicada's name** (8 characters).
  - Flanks: change the letter.
  - Right wing (short press): next letter; left wing: previous letter.
  - Right wing (long press): save.
  - Left wing on the first letter: cancel.
- Right flank: turn the signals on / off.

**Radio : message** (Radio: message) sends a message you can read with the "SubGHz chat" app on a Flipper Zero (433.92 MHz).

**Radio : porteuse** (Radio: carrier) transmits a continuous signal for 30 s (visible with a Flipper Zero's frequency analyzer).

**Infrarouge** (Infrared, module on the right port)
- "Enregistrer un signal" (Record a signal): point a remote control at the receiver and press a key.
- The 4 slots replay the recorded signals, like a remote control.


### 4.5 Réglages (Settings)

- **Veille de l'écran** (Screen sleep):
  - the delay before sleep (1, 2, 5, 10 or 30 minutes, or disabled);
  - the image shown while asleep (the SecSea by default, or an image from the `IMAGES` folder);
  - "Aperçu" (Preview) to try it out.

  While asleep, any button wakes the badge up.
- **Infos** (Info): radio version, crystal, SD card, battery. Right wing: the credits.
- **Crédits** (Credits): the people and associations behind the badge.


## 5. Battery

- The battery charges through the USB-C port, even with the switch on OFF.
  Red LED: charging; green LED: charged.
- The battery level is only shown (icon at the top right of the menu, and in Infos)
  if the badge has been calibrated; otherwise it shows "non calibrée" (not calibrated) (see the developer guide).


## 6. Controlling the badge from a computer

When plugged into USB, the badge shows up as a serial port. The `tools/badge_remote.py` application displays the badge's
screen in large on the computer and lets you control it from the keyboard (arrow keys = buttons).

```bash
pip install pyserial
python tools/badge_remote.py
```

See the [developer guide](developer_guide.md#8-the-usb-serial-protocol) for the serial port keys.


## 7. Preparing the SD card

### 7.1 The card

- Any capacity works (micro-SD, SDHC, SDXC).
- Formatted as **FAT32** or **exFAT**, the way a computer or camera does it.
  The badge never formats the card and never writes anything to it.
- File names can be long and contain accented characters.

### 7.2 The folders

```
SD card
├── IMAGES/     .epi images (200×200, 4 greys)
├── VIDEOS/     .epv videos
├── MUSIQUE/    .wav sounds (subfolders allowed, handy for the blind test)
└── TEXTES/     .txt texts for speed reading
```

### 7.3 Converting your files

The scripts are in the project repository. They need Python 3 and, for sound and video,
[ffmpeg](https://ffmpeg.org/) installed and available in the PATH.

```bash
pip install pillow numpy
```

**Images**: any photo (JPEG, PNG...) becomes a 200 × 200 image in 4 greys.

```bash
python src/images/image2epi.py photo1.jpg photo2.png -o IMAGES
python src/images/image2epi.py affiche.jpg --fit -o IMAGES        # whole image, with white bars
python src/images/image2epi.py paysage.jpg --equalize --preview    # more contrast, and a .png preview
```

Useful options:
- `--fit`: the image is not cropped;
- `--contrast 3`: stronger contrast;
- `--equalize`: for dull images;
- `--bw`: pure black and white;
- `--preview`: writes a .png preview.

**Videos**: the video is scaled down to 200 × 200, black and white, 10 frames per second, with sound for the buzzer.

```bash
python src/video/video2epaper.py film.mp4 -o VIDEOS/film.epv
python src/video/video2epaper.py film.mp4 --fit --start 60 --duration 30 -o VIDEOS/extrait.epv
```

Useful options:
- `--fit`: the image is not cropped;
- `--fps 10|20|30`: 10 gives the best contrast;
- `--no-audio`: no sound;
- `--preview apercu.gif`: writes an animated preview.

**Sounds**: MP3, OGG, FLAC... become WAV files suited to the buzzer (8 bits, 16 kHz, compressed sound so it can be heard).

```bash
python src/audio/audio2wav.py "musiques/*.mp3" -o MUSIQUE/Films
python src/audio/audio2wav.py dossier_complet -o MUSIQUE
```

**Texts**: any `.txt` file (UTF-8 or Windows/Latin-1) in `TEXTES`.

> Respect copyright: use works that are free of rights or that you hold the rights to.


## 8. Troubleshooting

| Problem | Solution |
|---|---|
| The badge does not turn on | Is the switch on ON? Is the battery charged (plug it into USB)? |
| The computer does not see the badge | Use a USB data cable (not a charge-only cable); switch on ON. |
| "Carte SD absente" (SD card missing) | Is the card pushed all the way in? Formatted as FAT32 or exFAT? |
| A file does not show up | Right extension (`.epi`, `.epv`, `.wav`, `.txt`) and right folder? Name shorter than 64 characters? |
| No sound | Volume at 0? (Médias > Volume). The buzzer is quiet: put your ear close to it. |
| The radio message does not reach the Flipper | The Flipper must be on 433.92 MHz in "SubGHz chat". In Infos, the crystal used must be 26 or 27 MHz. |
| The screen keeps ghost images | Normal after many fast refreshes: it cleans itself at the next full refresh. |
| The badge stops responding | Press the RESET button, or switch it off and on again. |
