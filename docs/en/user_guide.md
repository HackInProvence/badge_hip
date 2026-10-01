# SecSea badge — user guide

This guide explains how to use the Hack In Provence SecSea badge: turning it on, finding your way around the menus,
enjoying the media, games, social features, radio and extensions, and preparing a micro-SD card.
To modify the badge software, see the [developer guide](developer_guide.md).
All the pages of the badge in pictures, with their texts: [the screens of the badge](screens.md).

*Version française : [guide utilisateur](../fr/guide_utilisateur.md).*


## 1. The badge at a glance

- A 200 × 200 pixel e-Paper (electronic ink) screen, in black and white or 4 shades of grey.
  The image stays on screen even without power.
- 4 buttons, 2 colour LEDs, a buzzer.
- A 433 MHz radio (CC1101) compatible with the Flipper Zero. The badges use it to talk to each other:
  messages, votes, two-player games, business card exchange, choir...
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

Some pages also use the **long press** (about 0.8 s): in the puzzles, a long press on the left wing quits the game;
when typing text, a long press on the right wing confirms.

**Typing text with 4 buttons** (business cards, answers to the challenges, cure of the virus):
- flanks: previous / next character (hold to scroll through the characters);
- right wing (short press): next position; left wing: previous position;
- left wing on the first position: cancel;
- right wing (long press): confirm.


## 4. The menus

The main menu groups the features by theme:

| Theme | Contents |
|---|---|
| **Médias** (Media) | "Images", "Vidéos" (Videos), "Musique" (Music), "Lecture rapide" (Speed reading), "Volume" |
| **Jeux** (Games) | Morpion (tic-tac-toe), Puissance 4 (Connect Four), Simon, Réflexes (Reflexes), Snake, Démineur (Minesweeper), 2048, Taquin (15-puzzle), Sokoban, Mastermind, Pendu (Hangman), Blind test, CTF, Défis crypto (Crypto challenges), Duel, Bataille navale (Battleship) |
| **Social** | Cicada network, Messages, Contacts, Programme (Program), Vote, Radar des cigales (Cicada radar), Chaud - froid (Hot - cold), Virus des cigales (Cicada virus), Choeur (Choir), Annonces (Announcements) |
| **Radio & IR** | Radio message, radio carrier, Décodeur 433 MHz (433 MHz decoder), Station météo (Weather station), Envoyer une image (Send an image), Recevoir une image (Receive an image), Infrared, Chasse 433 MHz (433 MHz hunt) |
| **Badge** | Badge nominatif (Name tag), Lampe (Lamp), Badge de talk (Talk badge), cicada song, LED animations, screen demo, OLED screen |
| **Réglages** (Settings) | "Veille de l'écran" (Screen sleep), "Télécommande" (Remote control), "Mode muet" (Mute mode), "Infos" (Info), "Crédits" (Credits), "Réglage radio" (Radio tuning) |

A seventh theme, **Admin**, is hidden: it is for the organizers (see [§ 4.8](#48-admin-mode-organizers)).

**Notifications**: when a message, a vote question, an invitation to play or an announcement of the organizers
arrives, the badge beeps and writes it at the bottom of the screen. From the menus or the screensaver, the relevant
page opens directly (vote, program, virus, invitation to a duel or a battleship game, full-screen announcement).


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

In the games of the table below (except Simon), **the left wing quits the game**.
In the puzzles, it is a **long press on the left wing**.

| Game | How to play |
|---|---|
| **Morpion** (tic-tac-toe) | Flanks: choose the square; right wing: play. You are the crosses, the cicada the noughts. You take turns starting. The cicada makes a mistake now and then... |
| **Puissance 4** (Connect Four) | Flanks: choose the column; right wing: drop the counter. Line up 4 solid counters before the cicada's rings. |
| **Simon** | Each button is an area of the screen, placed like the button on the badge (flanks at the top, wings at the bottom), with its own colour and note. Repeat the sequence, which gets longer each round. Here the left wing is part of the game: to quit, wait until the game is over, then press the left wing. |
| **Réflexes** (Reflexes) | When the LEDs light up green (with a beep), quickly press the right wing or a flank. 5 tries, the average counts. Too early: the try starts over. |
| **Snake** | Right wing: start / pause. Left flank: turn left; right flank: turn right (relative to the snake's direction). |
| **Blind test** | Pick a music folder: tracks play in random order, without repeats. Right wing: show the title; right flank: next track; left flank: pause. |
| **CTF** | A challenge: enter a famous video game code with the 4 buttons to get a "flag". |

Records are kept even after the badge is switched off: best score of Simon and Snake, best average of Réflexes,
number of wins against the cicada in Morpion and Puissance 4.

**Score as a QR code**: at the end of a game, press a flank to show a QR code with the game, the score, the name
of your cicada and a signature: the organizers can scan it for a leaderboard, and a score edited by hand is rejected.
Any button closes the QR code.
In the Jeux menu, a **long press on the right wing** on the name of a game shows the QR code of its record the same way.

At the bottom of the screen, the buttons are listed in the order they are on the badge: G (left wing) on the left,
D (right wing) on the right.

**Puzzles**: Démineur, 2048, Taquin, Sokoban, Mastermind and Pendu are turn-based, at the pace of the screen.
They share the same controls:
- short presses: move. Left flank: left; right flank: right; left wing: up; right wing: down
  (seen from the front, the badge has its flanks at the top and its wings at the bottom);
- long presses: act. Right wing: main action (open, confirm...); left flank: secondary action (flag, undo);
  right flank: help page; left wing: quit.

Each game starts on its help page (rules, controls, record).

| Game | How to play |
|---|---|
| **Démineur** (Minesweeper) | 9 × 9 grid with 12 mines; the first cell opened is never a mine. Long right wing: open; long left flank: flag. The number of a cell gives the number of mines around it. |
| **2048** | All the tiles slide towards the button pressed; two equal tiles that touch merge. Reach the 2048 tile! |
| **Taquin** (15-puzzle) | Put the tiles 1 to 15 back in order, the hole at the end: the tile next to the hole slides towards the button pressed. |
| **Sokoban** | Push the boxes onto the targets, one at a time. 6 levels; long left flank: undo the last move; long right wing: restart the level. |
| **Mastermind** | Find the secret code of 4 symbols out of 6 (disc, ring, filled square, empty square, triangle, cross) in 10 tries. Flanks: position; wings: symbol; long right wing: confirm. A dot: a symbol in the right place; a ring: a symbol present but in the wrong place. |
| **Pendu** (Hangman) | Find the (French) word, letter by letter, before being hanged (8 mistakes). Flanks and wings: choose the letter; long right wing: try it. |

Records kept: best time at Démineur, best score at 2048, fewest moves at Taquin, number of levels solved at Sokoban,
fewest tries at Mastermind, most words found in a row at Pendu.

**Défis crypto** (Crypto challenges): 13 classical cryptography puzzles, from the easiest to the hardest
(acrostic, Caesar, ROT13, Morse, binary, hexadecimal, base64, Atbash, scytale, XOR, Vigenère, truncated hash,
ultrasound).
The texts are in French.
- The list ticks the solved challenges; right wing: open a challenge.
- On a challenge: right wing (short press): answer; right wing (long press): a hint;
  right flank: listen to the Morse (buzzer and LEDs), for the challenges that have some. The "Ultrason"
  challenge plays its Morse in a loop at 19 kHz, without the LEDs: hardly audible, it shows on the spectrogram
  of a phone (Spectroid, Spek...); right flank again: stop.
- The answer is typed with the 4 buttons (capital letters, digits, space); long right wing: confirm.
- Each solved challenge gives a piece of the flag; "> Le flag final" (the final flag) shows it whole once all are
  solved. Progress is kept even after the badge is switched off.

**Duel** (rock-paper-scissors) and **Bataille navale** (Battleship) are played by two badges, over the radio:
- the page lists the cicadas in range; flanks: choose, right wing: challenge;
- the other badge receives the invitation (a beep, and the page opens from the menus): left wing: decline;
  right wing: accept.
- **Duel**: each round, flanks: rock, paper or scissors ("Pierre", "Feuille", "Ciseaux"); right wing: play.
  The first to win 3 rounds wins. No badge can wait for the other's choice to cheat: each one commits to its choice
  before revealing it.
- **Bataille navale**: a 6 × 6 sea and 3 ships (3, 2 and 2 cells) placed at random. The one who invites shoots first,
  then each in turn. Flanks: aim (cell by cell); right wing: shoot. The first to sink the whole enemy fleet wins.
  At the end, each badge reveals its fleet and the other one checks that it answered honestly:
  "Flotte OK" (fleet OK) or "TRICHE !" (cheat!).
- At the end of a game: right wing: play again; left wing: quit.


### 4.3 Social

**Réseau des cigales** (Cicada network)
- Badges that are switched on quietly send each other radio signals.
- Hold your badge very close to another one for a few seconds: you earn
  **10 points** for a new encounter, **1 point** for a cicada you have already met (at most once per hour).
- Right wing: choose your **cicada's name** (8 characters).
  - Flanks: change the letter.
  - Right wing (short press): next letter; left wing: previous letter.
  - Right wing (long press): save.
  - Left wing on the first letter: cancel.
- Right flank: turn the signals on / off.

**Messages**: short messages between cicadas, chosen from a list of 16 ("Salut !" (Hi!), "Café ?" (Coffee?),
"Qui fait le CTF ?" (Who's doing the CTF?)...), no keyboard needed.
- The inbox keeps the last 10 messages; "(privé)" (private) marks the ones that are only for you.
- "> Ecrire un message" (write a message), then the recipient: "Tout le monde" (everybody), a neighbouring cicada or
  the author of a message received. A cicada is shown by its name and the end of its identifier, for example "Tristan#33EC".
- Right wing on a message received: reply to it.
- The other cicadas relay the message (up to 3 times) so that it reaches its recipient.

**Contacts**: a business card, exchanged over the radio with a badge close by.
- "Ma carte" (my card): 13 fields (first name, name, phone, e-mail, company, job title, address, city, LinkedIn, Git,
  web site, Mastodon, comment). Right wing: edit the field (typed with the 4 buttons);
  right wing (long press): tick or untick "[x]" to send it or not. Only the ticked fields are sent.
- "Echanger (badges proches)" (exchange with the badges close by): **both** badges must be on this page, the exchange
  only happens with your consent. Your card is sent again about every 3 seconds; the card received is shown:
  right wing: keep it; left wing: ignore it. Left wing on the exchange page: stop.
- The card goes out **in clear**, as a vCard, on the channel of the Flipper Zero's "SubGHz chat": any Flipper
  in range running `subghz chat 433920000 0` reads it during the exchange. Only the ticked fields are sent.
- A Flipper can also **send** a card to a badge in exchange mode: in `subghz chat 433920000 0`, type the lines
  one by one, for example `BEGIN:VCARD`, `N:Name;First name;;;`, `TEL:0612345678`, `END:VCARD`.
- "Contacts reçus" (contacts received): the last 12 cards kept (beyond that, the oldest one is forgotten).
  Right wing: view; right wing (long press): delete.
- On the computer, `tools/contacts_export.py` exports the cards received to a vCard file (`.vcf`),
  to import into a phone or an address book.

**Programme** (Program): the conference program, no SD card needed. Right wing: the details of a talk, with the
QR code of its link; flanks: previous / next talk. When the organizers announce the next talk, its page opens
by itself ("Prochain : ...", next: ...).
The SecSea 2026 program is not published yet: the talks shown are placeholders.

**Vote**: when the organizers ask a question ("Ce talk vous a plu ?", did you like this talk?...), the page opens.
Flanks: choose the answer; right wing: vote. You can change your mind while the vote is open:
only the last vote of each badge counts.

**Radar des cigales** (Cicada radar): the cicadas heard, with the strength of their signal (in dBm, "*" for a cicada
already met). Right wing: follow a cicada in "hot - cold" mode; left wing: back to the list.

**Chaud - froid** (Hot - cold): the organizers hide a beacon badge; find it by the strength of its signal.
The badge shows "Glacial" (freezing), "Froid" (cold), "Tiède" (warm), "Chaud" (hot) or "BRÛLANT !" (burning!) with a
gauge, the LEDs go from blue to red and the beeps speed up as you get closer.
Right wing: look for another beacon.

**Virus des cigales** (Cicada virus): a (harmless) virus spreads from cicada to cicada.
- Stay too long close to an infected cicada and you may catch it; your badge then "coughs" in turn and can infect
  the cicadas close by.
- The cure: a riddle (in French). Right wing: get cured, then type the answer with the 4 buttons.
  A cured cicada is immune.

**Choeur** (Choir): when a choir leader (an organizer or a remote control) starts a song, the cicadas around sing it
together, each one its own voice, with a start synchronized over the radio. Two songs: "Frère Jacques" as a 4-voice
canon and the "Ode to Joy" in 3 voices.
Your badge takes part by default; right wing: take part / stop singing. In mute mode, the choir is silent.

**Annonces** (Announcements): the announcements of the organizers (coffee break, next talk...). An announcement is a
screen built by the badge: the time in a black band, the text, and often a QR code (a link, a phone number, a Wi-Fi
network...) to scan with a phone.
- When the badge is on the menus or the screensaver, the announcement shows up by itself, like the screensaver
  image (sharp, it stays on screen); any button goes back to the menu.
- Otherwise, the badge beeps and writes it at the bottom of the screen: the next time Social > Annonces is opened,
  it shows the new announcement right away.
- The page keeps the last 5 announcements received (time and text). Flanks: choose; right wing: show it;
  left wing: back.


### 4.4 Radio & IR

**Radio : message** (Radio: message) sends "SecSea <name of your cicada> coucou #<number>", which you can read with
the "SubGHz chat" app on a Flipper Zero (433.92 MHz).
A **long press on the right wing** starts the **test mode**: one message every 5 seconds.
- Flanks: shorter / longer pause (1 second minimum, no maximum). Hold a flank to scroll the values faster and faster.
- Right wing: pause / resume; left wing: quit.

**Radio : porteuse** (Radio: carrier) transmits a continuous signal for 30 s (visible with a Flipper Zero's frequency analyzer).

**Décodeur 433 MHz** (433 MHz decoder, receive only): the last frames heard, with their age: Princeton, CAME and
Nice FLO remote controls, weather sensors. A frame received several times is followed by "x2", "x3"...
Right wing: clear. While listening, the badge no longer hears the other cicadas.

**Station météo** (Weather station, receive only): the last measure of each 433 MHz sensor heard (4 at most):
temperature, humidity, protocol, channel, and "pile !" when the sensor's battery is low.
Supported sensors: Nexus-TH, inFactory, ThermoPRO TX-4, GT-WT02, LaCrosse TX141TH-Bv2, Acurite 592TXR.
The sensors transmit every 30 to 60 s: be patient. Right wing: clear.

**Envoyer une image** (Send an image): choose the built-in "SecSea" image or an image from the `IMAGES` folder;
it is converted to black and white and sent twice over the radio to the badges waiting for it (about 17 seconds).
Left wing: stop.

**Recevoir une image** (Receive an image): wait for another badge to send an image. While it is received, the page
shows the number of blocks received, with a gauge; parity blocks allow rebuilding the ones lost on the way.
Left wing: stop; right wing: start again (wait for another image).
Once complete, the image is shown like the screensaver (sharp, no ghost), with "Reçue" (received) and the number
of blocks corrected; any button goes back to the menu.

**Infrarouge** (Infrared, module on the right port)
- "Enregistrer un signal" (Record a signal): point a remote control at the receiver and press a key.
- The 4 slots replay the recorded signals, like a remote control.

**Chasse 433 MHz** (433 MHz hunt, receive only): find a hidden 433 MHz transmitter (remote control, sensor, jammer
that repeats its code) by "hot - cold".
- The page lists the codes heard (8 at most): protocol and code, strength of the last frame (dBm) and number of
  frames received ("x3"...). Flanks: choose; right wing: hunt it; left wing: back.
- The badge then follows this code like Social > Chaud - froid: from "Glacial" to "BRÛLANT !", gauge, LEDs from blue
  to red and faster and faster beeps. The strength is measured on each frame received: the transmitter must be
  sending. Without a frame for 15 s, it is "hors de portée" (out of range). Left wing: back to the list.
- During the hunt, the badge no longer hears the other cicadas.


### 4.5 Badge

- **Badge nominatif** (Name tag): "SecSea 2026", your cicada's name in large letters and your type (PARTICIPANT,
  ORATEUR (speaker) or STAFF) in a black band. The screensaver does not replace it: it stays on screen, even with the
  badge switched off. The type is chosen by the organizers. Left wing: back.
- **Lampe** (Lamp): the 2 LEDs in white. Flanks: dimmer / brighter, in steps of 10 % (hold: faster);
  right wing: off / on. The brightness is remembered and shown in the menu ("Lampe : 50 %").
  In mute mode, the LEDs stay off.
- **Badge de talk** (Talk badge): for the speakers, the LEDs show the speaking time:

  | State | LEDs |
  |---|---|
  | Éteint (off) | off |
  | OK | steady green: all is well |
  | 5 min | orange, softly breathing: 5 minutes left |
  | FINI (over) | blinking red: time is up |
  | STOP ! | fast blinking red, and the cicada sings at full volume (even in mute mode): time to conclude! |

  The state is changed by the organizers' remote control (see [§ 4.7](#47-remote-control-and-mute-mode)) or by hand:
  left flank: previous state; right flank or right wing: next state. This page works even in mute mode,
  and listens to the remote control all the time while it is open.
- **Cigale** (Cicada): turns the cicada's song on or off.
- **LEDs**: changes the animation (rainbow, breathing, heartbeat, blinking, steady green, off).
- **Démo écran** (Screen demo): shows what the screen can do (black and white, 4 greys, fast animation).
- **Écran OLED** (OLED screen): demos on a small OLED screen plugged into the left port (stars, 3D cube, cicada, text, video).


### 4.6 Réglages (Settings)

- **Veille de l'écran** (Screen sleep):
  - the delay before sleep (1, 2, 5, 10 or 30 minutes, or disabled);
  - the image shown while asleep (the SecSea by default, or an image from the `IMAGES` folder);
  - "Aperçu" (Preview) to try it out.

  The screensaver image is shown in dithered black and white (grays become dot patterns):
  it is the most stable display of the screen, the image stays sharp for hours without power.

  While asleep, any button wakes the badge up.
- **Télécommande : oui / non** (Remote control: yes / no): the badge obeys (or not) the radio commands of the
  organizers and of the Flipper Zero (see [§ 4.7](#47-remote-control-and-mute-mode)). Enabled by default.
- **Mode muet : oui / non** (Mute mode: yes / no): turns off the sound and the LEDs. The organizers can switch it on
  remotely during the talks.
- **Infos** (Info): 6 lines: firmware version (number and git commit, a "+" when the sources differed from the
  commit), build date, radio version, crystal used ("(mesure)" while it is being measured), SD card, battery.
  Right wing: the credits.
- **Crédits** (Credits): the people and associations behind the badge.
- **Réglage radio** (Radio tuning): tunes the radio automatically, in 3 steps (about 15 s):
  1. the crystal of the radio (26 or 27 MHz);
  2. the radio noise of the place (3 s): the badge listens to the remote controls from 15 dB above the noise;
  3. the frequency, from the packets of the other cicadas heard for 10 s: stay close to other badges switched on.

  The tuning runs by itself at the first start of the badge (and after an update that brings it), then on demand:
  right wing: tune again; left wing: stop or go back. The page shows the result: crystal, noise, threshold of the
  remote controls ("Télécommandes : > −90 dBm"...), frequency correction, and the number of packets of other
  cicadas heard (with no other cicada, the frequency is not corrected).


### 4.7 Remote control and mute mode

The organizers can send commands to all the badges in the room, from a badge in admin mode or from a Flipper Zero.
The badge shows the command received at the bottom of the screen.

| Command | Effect |
|---|---|
| 0x01 | the cicada sings for a few seconds |
| 0x02 | **mute mode**: no more sound or LEDs (during the talks) |
| 0x03 | end of the mute mode |
| 0x10 to 0x14 | lights of the talk badge (if its page is open): off, OK, 5 min, FINI, STOP ! |
| 0x20 + n | shows talk n of the program ("Prochain : ...") |
| 0x30 + n | starts song n of the choir |

The mute mode setting is kept after the badge is switched off; it is turned off by command 0x03 or in Réglages > Mode muet.
To stop obeying the commands: Réglages > Télécommande : non.

**From a Flipper Zero**: a command is a 24-bit **Princeton** code `0xC16Axx`, where `xx` is the command
(for example `0xC16A02` for the mute mode).

The simplest: copy the two files of `tools/flipper/` to the `subghz` folder of the Flipper's SD card, then
Sub-GHz > Saved > the file. With each button, the remote of the Flipper sends the same code with another Princeton
button (the last 4 bits), and each button has its command:

| Flipper button | `SecSea_general.sub` | `SecSea_talk.sub` |
|---|---|---|
| OK | the cicada sings | talk: green |
| Up | mute mode | talk: orange (5 min) |
| Down | end of the mute mode | talk: angry red |
| Right | — | talk: red (done) |
| Left | — | talk: off |

Other commands:
- Sub-GHz > Add Manually > Princeton_433, save, then edit the `Key:` line of the saved file;
- or generate a `.sub` file on the computer and copy it to the `subghz` folder of the Flipper's SD card:
  `python tools/ook_sub.py princeton 0xC16A02 -o mute.sub`, then Sub-GHz > Saved > mute > Send.

The badge listens to the network of the cicadas all the time; when it hears a transmitter that is not a badge, it
listens to the remote controls for a moment to decode the code, which must be received twice: keep sending for
a second. The `subghz tx` command of the Flipper's command line does not send the code as is: use a `.sub` file
(it replaces the last 4 bits of the code with 6).

An admin badge sends its commands both ways, like a remote control and then over the network of the cicadas;
a command received twice is executed only once.


### 4.8 Admin mode (organizers)

In the main menu (the list of themes), press the flanks **left, left, right, right, left, right, left, right** within
8 seconds: "Mode admin activé" (admin mode on) is shown and the **Admin** theme appears, already selected, after the
others. The admin mode stays on after the badge is switched off.
More discreet: the "Mode admin" check box of `tools/badge_remote.py`, with the badge plugged into USB (see [§ 6](#6-controlling-the-badge-from-a-computer)).

| Entry | Role |
|---|---|
| **Commandes radio** (Radio commands) | sends a command to all the badges around: mute, end of mute, cicada, lights of the talk badge |
| **LEDs des cigales** (LEDs of the cicadas) | chooses the colour and the animation of the LEDs of all the cicadas around (see below) |
| **Annonces (admin)** (Announcements) | writes and sends the announcements to all the cicadas (see below) |
| **Annoncer un talk** (Announce a talk) | sends a talk of the program to all the cicadas, as an announcement: its time, its title and speaker, the QR code of its link |
| **Vote (admin)** | opens a question, counts the votes (one per badge) and shows the histogram; right wing: close the vote |
| **Choeur : lancer** (Choir: start) | starts a song of the choir; this badge sings the first voice |
| **Balise chaud-froid** (Hot-cold beacon) | this badge sends a beacon every second: hide it, the others look for it with Social > Chaud - froid |
| **Virus : patient zéro** (Virus: patient zero) | infects this badge to start the epidemic; left flank: cure it |
| **Remise à zéro** (Reset) | erases the scores and the progress of this badge (see below) |
| **Batterie (calibration)** | calibrates the battery measure with a multimeter (see § 5) |
| **Type du badge** (Badge type) | Participant, Orateur (speaker) or Staff, shown by the name tag |
| **Quitter le mode admin** (Leave admin mode) | hides the Admin theme again |

**LEDs des cigales** (LEDs of the cicadas): a list of settings; flanks: choose the row.
- "Couleur" (colour): wings: previous / next colour (red, orange, yellow, green, cyan, blue, purple, pink, white);
- "Rouge (R)", "Vert (G)", "Bleu (B)": from 0 to 255; left wing: −, right wing: + (hold: faster and faster);
  the colour becomes "personnalisée" (custom);
- "Mode": Fixe (steady), Clignotant (blinking) or Fondu (fading) (wings: previous / next mode). In Clignotant or Fondu,
  a long press on the right wing opens the times: on / off, or to the colour / to black, from 50 ms to 5 s in steps
  of 50 ms (flanks: choose the time; wings: − / +; long press on a wing: back);
- "> Envoyer aux cigales" (send to the cicadas, right wing): the cicadas around, and this badge, show these LEDs
  instead of their animation, until "> Rétablir leurs LEDs" (restore their LEDs) or their restart. The mute mode still
  turns their LEDs off, and the pages that drive the LEDs themselves (games, talk badge) keep them.

While setting, the LEDs of this badge show the colour and the mode chosen. Long left wing (or left wing on the last
two rows): quit.

**Annonces (admin)** (Announcements): 6 announcements, kept after the badge is switched off; at first, examples
(welcome, breaks, CTF awards...). Flanks: choose; right wing: open it; left wing: back. An announcement has 6 rows
(flanks: choose; left wing: back to the list):
- "Heure" (time, for example 10:30) and "Texte" (text, at most 56 characters, accented letters included): right wing:
  edit, with the 4-button editor (or the keyboard of the computer);
- "QR code": the type of the QR code, with the wings: Aucun (none), Lien (URL), Texte (text), Téléphone (phone), SMS,
  E-mail, Wi-Fi, Position GPS (GPS position);
- "Contenu" (content): what the QR code contains (right wing: edit); the badge puts it in the form that phones
  understand:

  | Type | Content to type | QR code |
  |---|---|---|
  | Lien (URL) | `www.example.com` or `https://...` | `https://` added when missing |
  | Texte | a text | the text |
  | Téléphone | `+33612345678` | `tel:+33612345678` |
  | SMS | `number:message` | `SMSTO:number:message` |
  | E-mail | `address@mail.com` | `mailto:address@mail.com` |
  | Wi-Fi | `network;password` | `WIFI:T:WPA;S:network;P:password;;` (without ";": open network) |
  | Position GPS | `43.17,5.60` | `geo:43.17,5.60` |

- "> Aperçu" (preview): the screen of the announcement, as the cicadas will show it (a wing: back);
- "> Envoyer à toutes les cigales" (send to all the cicadas): the announcement goes over the radio, 3 times in a row
  (for the cicadas that missed it). This badge does not show it to itself: that is what the preview is for.

**Remise à zéro** (Reset): before the event or after tests. Flanks: choose; right wing, then a **long press on the
right wing** to confirm (left wing: no):
- "Scores sociaux" (social scores): the score and the encounters of the cicada network;
- "Records des jeux" (game records): the records of the games and the puzzles;
- "Défis CTF et crypto" (CTF and crypto challenges): the CTF flags and the crypto challenges solved;
- "Contacts reçus" (contacts received): the business cards received (not your card);
- "Virus": the state of the virus (healthy);
- "Tout" (all): all of this at once. The name, the settings and your business card are kept.


## 5. Battery

- The battery charges through the USB-C port, even with the switch on OFF.
  Red LED: charging; green LED: charged.
- The battery level is only shown (icon at the top right of the menu, and in Infos)
  if the badge has been calibrated; otherwise it shows "non calibrée" (not calibrated).
- **Calibrating** (Admin > Batterie (calibration)), once per badge:
  1. with the badge on USB (charging), measure the battery voltage with a multimeter;
  2. on the "Multimètre" line, set this voltage with the wings (- / +, hold: fast);
  3. flank: "Enregistrer le point" (save the point), right wing;
  4. do it again on battery, unplugged for a few minutes (the voltage must have dropped by at least 0.2 V):
     the level is shown from this 2nd point on.
  A new point replaces the nearest one. "Effacer" (right wing twice) forgets the calibration.
  The calibration is a "factory" setting: kept by the Reset (even "Tout") and by the firmware updates.


## 6. Controlling the badge from a computer

When plugged into USB, the badge shows up as a serial port. The `tools/badge_remote.py` application displays the badge's
screen in large on the computer and lets you control it from the keyboard (arrow keys = buttons,
Shift + arrow = long press).

```bash
pip install pyserial
python tools/badge_remote.py
```

- The buttons of the window simulate those of the badge; under each one, "appui long" (long press).
- **Badge :** the list of the badges plugged in, to choose the one to control when there are several.
- **Mode clavier (saisie de texte)** (keyboard mode, text input): when a text editor is open on the badge (business
  card, answer to a challenge...), the characters typed on the computer are inserted at the cursor; Enter: done;
  Escape: cancel; Backspace: erase. The arrow keys are still the buttons. Only the characters without accent go
  through the keyboard: the accented letters are chosen with the flanks.
- **Mode admin**: turns the admin mode of the badge on or off (see [§ 4.8](#48-admin-mode-organizers));
  the box follows the state of the badge.
- A connection problem (badge unplugged, port busy) is shown in the status line and in the log;
  the application finds the badge again when it comes back.

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
| The radio message does not reach the Flipper | The Flipper must be on 433.92 MHz in "SubGHz chat". In Infos, the crystal must be 26 or 27 MHz. |
| No more sound or LEDs | The mute mode may be on (command of the organizers during a talk): Réglages > Mode muet. |
| The badge does not obey the Flipper | Réglages > Télécommande : oui? Princeton code `0xC16Axx`, sent from a `.sub` file and held for a second. |
| The other cicadas are no longer heard | Normal while the 433 MHz decoder, the weather station, the 433 MHz hunt, the talk badge or the contact exchange is open: they use the radio. |
| The remote controls or the other cicadas are poorly received | Réglages > Réglage radio, close to other badges switched on. |
| The screen keeps ghost images | Normal after many fast refreshes: it cleans itself at the next full refresh. |
| The badge stops responding | Press the RESET button, or switch it off and on again. |
