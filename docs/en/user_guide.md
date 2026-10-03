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
- A micro-SD card reader for videos, music, texts, images, ringtones and gamebooks.
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
| **Médias** (Media) | "Images", "Vidéos" (Videos), "Musique" (Music), "Sonneries" (Ringtones), "Lecture rapide" (Speed reading), "Livres-jeux" (Gamebooks), "Volume" |
| **Jeux solo** (Solo games) | Morpion (tic-tac-toe), Puissance 4 (Connect Four), Simon, Réflexes (Reflexes), Snake, Démineur (Minesweeper), 2048, Taquin (15-puzzle), Sokoban, Mastermind, Pendu (Hangman), Blind test, CTF, Défis crypto (Crypto challenges) |
| **Jeux multi** (Multiplayer games) | Duel, Bataille navale (Battleship), Loup-garou (Werewolf), Assassin, Tir à la corde (Tug of war) |
| **Social** | Cicada network, Messages, Contacts, Compétences (Skills), Programme (Program), Vote, Radar des cigales (Cicada radar), Chaud - froid (Hot - cold), Virus des cigales (Cicada virus), Choeur (Choir), Annonces (Announcements), Contrebande (Smuggling) |
| **Radio & IR** | Radio message, radio carrier, Décodeur 433 MHz (433 MHz decoder), Station météo (Weather station), Envoyer une image (Send an image), Recevoir une image (Receive an image), Infrared, Chasse 433 MHz (433 MHz hunt), Écouter la radio pirate (Listen to the pirate radio) |
| **Badge** | Badge nominatif (Name tag), Lampe (Lamp), Badge de talk (Talk badge), cicada song, LED animations, screen demo, OLED screen, Succès (Achievements) |
| **Réglages** (Settings) | "Veille de l'écran" (Screen sleep), "Langue" (Language), "Télécommande" (Remote control), "Mode muet" (Mute mode), "Infos" (Info), "Crédits" (Credits), "Réglage radio" (Radio tuning) |

A seventh theme, **Admin**, is hidden: it is for the organizers (see [§ 4.8](#48-admin-mode-organizers)).

**Notifications**: when a message, a vote question, an invitation to play or an announcement of the organizers
arrives, the badge beeps and writes it at the bottom of the screen. From the menus or the screensaver, the relevant
page opens directly (vote, program, virus, invitation to a duel, a battleship game or a smuggling deal, news of the
assassin and werewolf games, full-screen announcement).
A new achievement ("Succès : Sociable") or a new level is also shown at the bottom of the screen, with a chime.


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
- WAV files are played as 8, 16, 24 or 32-bit PCM, or 32-bit float, mono or stereo, from 4 to 192 kHz;
  the best is still the 8-bit 16 kHz mono of `audio2wav.py` (see [§ 7.3](#73-converting-your-files)).

**Sonneries** (Ringtones, `SONNERIES` or `RTTTL` folders and their sub-folders, optional): ringtones in the RTTTL format, the one of the old Nokia
phones. 12 public domain tunes are built into the badge (Lettre à Élise (Für Elise), Ode à la joie (Ode to Joy),
Frère Jacques, La Marseillaise, Korobeiniki...), those of the SD card follow.
- Flanks: choose; right wing: play; left wing: back. A ringtone marked `(!)` has an error: the right wing shows the
  line, the column and the reason.
- While playing: the name, the note played, a progress bar; the LEDs light up with one colour per note.
  Left wing: stop; flanks: previous / next ringtone; right wing: start again from the beginning.
- The format and how to add ringtones: [ringtones.md](ringtones.md).

**Lecture rapide** (Speed reading, `TEXTES` folder)
- Words appear one at a time in the same spot, so your eyes no longer move: you read much faster
  (PVSR method, see [pvsr.md](../pvsr.md)).
- Right wing: pause / resume; left wing: quit.
- Flanks (short press): speed −/+ (from 100 to 900 words per minute, 250 at start).
- Flanks (long press): go back / forward about 10 seconds of reading.
- Your position is remembered: a text resumes where you left it.

**Livres-jeux** (Gamebooks, `LIVRES` folder, optional): books in which you are the hero. One book is built into the
badge, "Le Trésor du capitaine Cigalon" (Captain Cigalon's treasure, in French), the `.txt` books of the SD card
follow (16 at most).
- The list: "Continuer : ..." (continue) resumes the last book; flanks: choose; right wing: open; left wing: quit.
- The menu of a book: Commencer (start), Reprendre la lecture (resume), Recommencer (start over), Objets (items),
  Autres livres (other books).
- Reading: right flank: next page, then next choice; left flank: previous choice, then previous page; right wing:
  take the selected choice; left wing: the menu of the book; left wing held: quit.
- Dice, items, endings won or lost; the progress is kept even with the badge switched off (one book at a time).
- Reading, adding and writing books: [gamebooks.md](gamebooks.md).

**Volume**: each press on the right wing raises the volume (0 to 8, then back to 0) and plays a short chime.


### 4.2 Jeux (Games)

Two themes: **Jeux solo** (against the cicada or alone) and **Jeux multi** (between cicadas, by radio: duel, battleship
and the group games).

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

**Group games**: **Tir à la corde** (Tug of war), **Assassin** and **Loup-garou** (Werewolf) are played by several
badges, over the radio, with the same lobby:
- "Créer une partie" (create a game; "Mener une partie", lead a game, at the werewolf): your badge hosts the game and
  lists the players who join it. Right wing: start (when there are enough players); left wing: cancel.
- "Rejoindre une partie" (join a game): the open games around (name of the host, number of players), the closest
  first. Right wing: join; the "Salle d'attente" (waiting room) waits for the start; left wing: leave.
- A single group game at a time on a badge. The game goes on while you use the other pages: long press on the left
  wing to leave the page without leaving the game.

**Tir à la corde** (Tug of war, 2 players at least): two teams, the **Cigales** (cicadas) and the **Fourmis** (ants),
drawn at random; with an odd number of players, one of them is the **arbitre** (referee: watches the rope).
- The teams are shown for 5 s, then a countdown 3, 2, 1 (beeps, LEDs yellow, orange, red), then "Tirez !" (pull!)
  for 20 s: **left wing then right wing** = one pull (both at the same time do not count).
- The knot of the rope moves with the pulls of the two teams, on every badge. A team 20 pulls ahead per player of the
  team reaches its mark and wins at once; otherwise, the strongest at the end of the 20 s wins.
- The result: the winning team, the score "Cigales - Fourmis", your pulls and the best puller; green LEDs for the
  winners, red for the losers. Right wing: back to the choice Create / Join; left wing: back.

**Assassin** (3 players at least; 2 when the host is in admin mode): a game that lasts the whole conference.
- At the start, the host draws a secret ring: each player gets a **target**, and is himself the target of another one.
- The page: "Ta cible :" (your target) and its name, a hot - cold gauge from its signals ("Glacial", "Froid", "Tiède",
  "Chaud", "Brûlant !", or "Pas captée": not heard), the number of survivors.
- **Right wing: eliminate**. You must be very close to the target, the badges almost touching (the badge of the target
  checks the strength of the signal; provisional threshold, to tune on site). "Cible éliminée !" (target eliminated):
  the target of your victim becomes yours. "Trop loin ou absente" (too far or absent): try again closer.
- The victim sees "Éliminé par ..." (eliminated by); its target goes to its killer, then right wing: leave the game.
- Flank: give up (confirm with the right wing): your target goes to your hunter.
- The last cicada standing wins. The news ("Éliminé par ...", "Assassin : nouvelle cible" (new target),
  "Assassin : victoire !" (victory)) arrives even on another page.
- The badge of the host must stay on; a badge that restarts leaves the game.

**Loup-garou** (Werewolf, 8 to 18 players, plus a narrator): the werewolf game after the rules of Les Loups-garous
de Thiercelieux, without cards. 2 werewolves from 8 to 11 players, 3 from 12 to 18. Fewer than 8 players: only if
the organizer unlocks it (Admin > Loup-garou (admin)).
- The narrator, who does not play, chooses "Mener une partie" (lead a game): the roles in play (a check box per
  role: voyante (seer), sorcière (witch), chasseur (hunter), Cupidon (Cupid), petite fille (little girl), capitaine
  (captain), voleur (thief); presets classique and débutant), the length of the debate (2, 3 or 5 minutes), then
  starts the game; the players choose "Rejoindre une partie" (join a game).
- Each one discovers his secret card on his badge (**hide your screen**). The narrator's badge runs the phases (night:
  thief, Cupid, lovers, seer, wolves, witch; day: dawn, election of the captain, debate, vote, verdict) and every badge
  rings at each new phase.
- At night, every living player chooses in a list (those without the role pretend): nobody can guess the roles by
  watching who presses buttons. During the day, each one votes on his badge.
- "Aide : les rôles" (help: the roles): an illustrated card per role, with its camp and its powers.
- Left wing: back to the menu, the game goes on; left wing held: leave the game; right wing held: see your card again.
- The rules, the phases, the votes and the tips for the narrator: [loup_garou.md](../fr/loup_garou.md) (in French).

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
- Your skills (below) go with the card (`CATEGORIES` line of the vCard) as soon as at least one is ticked; those of a
  card received are shown as pictograms at the bottom of the card.

**Compétences** (Skills): 20 skills, each with its pictogram: Électronique (electronics), Flipper Zero, Android, iOS,
Radio / SDR, Web, Réseau (network), Crypto, Reverse, Pentest, Forensic, OSINT, Linux, Windows, Cloud, IA (AI),
Développement (development), CTF, Lockpicking, Défense (defense).
- "Mes compétences" (my skills): the list with check boxes; right wing: tick / untick. The pictograms of the ticked
  skills are shown on the page, on the name tag, and go out in the signals of the cicada network.
- "Qui les partage ?" (who shares them?): the cicadas heard that share at least one of your skills, with the
  pictograms in common (updated every 3 s).
- When a cicada sharing a skill comes close (as close as for an encounter), the badge says so at the bottom of the
  screen, with a chime: "Marius aime aussi : Radio / SDR" (Marius likes it too; once per visit).
- The badges of an older firmware do not send their skills.

**Programme** (Program): the conference program, no SD card needed. Right wing: the details of a talk, with the
QR code of its link; flanks: previous / next talk.
The SecSea 2026 program is not published yet: the talks shown are placeholders.

**Vote**: when the organizers ask a question ("Ce talk vous a plu ?", did you like this talk?...), the page opens.
Flanks: choose the answer; right wing: vote. You can change your mind while the vote is open:
only the last vote of each badge counts.

**Radar des cigales** (Cicada radar): the cicadas heard, with their level ("N3") and the strength of their signal (in dBm, "*" for a cicada
already met). Right wing: follow a cicada in "hot - cold" mode; left wing: back to the list.

**Chaud - froid** (Hot - cold): the organizers hide a beacon badge; find it by the strength of its signal.
The badge shows "Glacial" (freezing), "Froid" (cold), "Tiède" (warm), "Chaud" (hot) or "BRÛLANT !" (burning!) with a
gauge, the LEDs go from blue to red and the beeps speed up as you get closer. The scale is the beacon's: "BRÛLANT !"
at about 1 m by default (from -74 dBm), set by the organizer (see Admin > Balise chaud-froid).
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

**Contrebande** (Smuggling): the smuggler cicada. 26 virtual goods (food, rum, spices, treasures), common, rare or
legendary, to collect and to trade **on the quiet** between two badges held against each other.
- The home page: Cale (hold), Échanger en douce (trade on the quiet), Donner (give), Collection (x / 26), Fortune (in
  doubloons, with a rank).
- The hold gets 4 common goods at the first opening; each new cicada met gives a one-in-two chance of finding another
  one (discreet message at the bottom of the screen, no sound).
- "Échanger en douce": the cicadas **à portée de main** (within reach: badges touching); right wing: offer a deal.
  The other one gets "Psst..." and accepts (right wing) or refuses (left wing); each one chooses its good, both offers
  are shown, right wing: close the deal. "Donner" offers a good with nothing in return.
- The details of the pages, of the goods and of the protocol: [smuggler.md](smuggler.md).


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

**Écouter la radio pirate** (Listen to the pirate radio, receive only): the badge receives the FM sound sent by an
organizer's badge (Admin > Radio pirate) and plays it on its buzzer.
- The page: the frequency, the strength of the signal (dBm and gauge), the frequency correction, the tone detected,
  the level.
- Flanks: previous / next channel (the 5 channels of the pirate radio, 433.920 MHz at first); right wing:
  squelch ("Silencieux") on / off; left wing: back.
- While listening, the badge no longer hears the other cicadas. How it works and the tests: [pirate_radio.md](pirate_radio.md).


### 4.5 Badge

- **Badge nominatif** (Name tag): "SecSea 2026", your cicada's name in large letters and your type (PARTICIPANT,
  ORATEUR (speaker) or STAFF) in a black band, then the pictograms of your skills (Social > Compétences, 10 at most).
  The screensaver does not replace it: it stays on screen, even with the badge switched off.
  The type is chosen by the organizers. Left wing: back.
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
- **Succès** (Achievements): the achievements obtained and the level of your cicada (see below).

**Achievements and level**: like the dolphin of the Flipper Zero, your cicada earns experience points (XP): each
achievement gives XP, and each cicada met **2 XP**. The level goes from 1 to 10:

| Level | Name | XP | Level | Name | XP |
|---|---|---|---|---|---|
| 1 | Oeuf (egg) | 0 | 6 | Cigale (cicada) | 260 |
| 2 | Larve (larva) | 20 | 7 | Chanteuse (singer) | 380 |
| 3 | Nymphe (nymph) | 50 | 8 | Virtuose (virtuoso) | 530 |
| 4 | Mue (moult) | 100 | 9 | Maestro | 720 |
| 5 | Jeune cigale (young cicada) | 170 | 10 | Cigale d'or (golden cicada) | 1000 |

The page shows the level, the XP (out of those of the next level) with a gauge, the number of achievements obtained,
then the list: first the meetings ("Rencontres : 16 x 2 = 32 XP": each cicada met brings 2 XP), then the value of each
achievement ("Contrebandier (+20 XP)", filled box: obtained). Flanks: choose;
right wing: how to get it; left wing: back.
A new achievement is announced at the bottom of the screen ("Succès : Sociable"), or the new level
("Niveau 3 : Nymphe !").

| Achievement | How to get it | XP |
|---|---|---|
| Premiers pas (first steps) | switch your cicada on | 5 |
| Bonjour ! (hello!) | meet a cicada (stay close to it) | 10 |
| Sociable | meet 10 cicadas | 30 |
| Star du réseau (network star) | meet 50 cicadas | 80 |
| Facteur (postman) | send a message (Social > Messages) | 10 |
| Carte de visite (business card) | receive a contact (Social > Contacts) | 15 |
| Citoyen (citizen) | vote (Social > Vote) | 10 |
| Choriste (chorister) | sing in the choir | 15 |
| Patient | catch the cicada virus | 10 |
| Remède (cure) | get cured of the virus | 20 |
| Duelliste (duellist) | win a rock-paper-scissors | 20 |
| Amiral (admiral) | win a battleship game | 30 |
| Pleine lune (full moon) | play werewolf | 20 |
| Survivant (survivor) | win at werewolf | 40 |
| Ombre (shadow) | eliminate your target at the assassin | 20 |
| Dernier debout (last one standing) | win the assassin | 50 |
| Costaud (strong) | win the tug of war | 20 |
| Héros (hero) | finish a gamebook | 30 |
| Mélomane (music lover) | play a ringtone | 5 |
| Contrebandier (smuggler) | trade a good (Social > Contrebande) | 20 |
| Trésor (treasure) | get a legendary good | 50 |
| Collectionneur (collector) | own all the goods | 100 |
| Hacker | find a flag of the CTF | 30 |
| Cryptographe (cryptographer) | solve a crypto challenge | 20 |
| Fin limier (sleuth) | find the hot-cold beacon ("BRÛLANT !") | 30 |
| Chasseur d'ondes (wave hunter) | hear a 433 MHz remote control (Chasse 433 MHz) | 15 |
| Recordman | beat a record in a game (or win against the cicada) | 15 |
| Cinéphile (film buff) | watch a video to the end | 15 |
| Photographe (photographer) | send an image over the radio | 15 |
| Expert | tick your skills (Social > Compétences) | 10 |
| Âmes soeurs (soulmates) | come across a cicada that shares a skill | 20 |
| Platine (platinum) | get all the other achievements | 200 |

### 4.6 Réglages (Settings)

- **Veille de l'écran** (Screen sleep):
  - the delay before sleep, shown in the menu ("Veille : 5 min"): 1, 2, 3, 5, 10, 15 or 30 minutes, 1 hour, or
    disabled (right wing: the next one);
  - the image shown while asleep (the SecSea by default, or an image from the `IMAGES` folder);
  - "Aperçu" (Preview) to try it out.

  The screensaver image is shown in dithered black and white (grays become dot patterns):
  it is the most stable display of the screen, the image stays sharp for hours without power.

  While asleep, any button wakes the badge up.
- **Langue** (Language): French or English ("Langue / Language", each language written in itself; sides then right
  wing). A badge left in a language you can't read: left wing 5 times (back to the main menu), then the sides
  in turn left, right × 4: it switches to English. See [translation](translation.md).
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
| 0x05 | **sleep** (by the network of the badges only, not in Princeton): see Admin > Commandes radio |
| 0x10 to 0x14 | lights of the talk badge (if its page is open): off, OK, 5 min, FINI, STOP ! |
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
| **Commandes radio** (Radio commands) | sends a command to all the badges around: mute, end of mute, cicada, lights of the talk badge, sleep |
| **LEDs des cigales** (LEDs of the cicadas) | chooses the colour and the animation of the LEDs of all the cicadas around (see below) |
| **Annonces (admin)** (Announcements) | writes and sends the announcements to all the cicadas (see below) |
| **Vote (admin)** | opens a question, counts the votes (one per badge) and shows the histogram; right wing: close the vote |
| **Choeur : lancer** (Choir: start) | starts a song of the choir; this badge sings the first voice |
| **Balise chaud-froid** (Hot-cold beacon) | this badge sends a beacon every second: hide it, the others look for it with Social > Chaud - froid. Flanks: the scale ("Brûlant dès -74 dBm" by default, 5 dB steps, saved), sent to the hunters with the beacon: set it on site for the distance wanted |
| **Virus : patient zéro** (Virus: patient zero) | infects this badge to start the epidemic; left flank: cure it |
| **Loup-garou (admin)** (Werewolf, admin) | fewer than 8 players: "8 joueurs minimum" (the rules, the default), "Petites parties (4+)" (small games: 1 werewolf below 8) or "Test : robots" (robots complete up to 8); saved |
| **Contrebande (admin)** (Smuggling, admin) | adds a good of your choice to the cargo of this badge (to unlock a rare or a legendary one, for instance): flanks: choose, right wing: add |
| **Remise à zéro** (Reset) | erases the scores and the progress of this badge (see below) |
| **Batterie (calibration)** | calibrates the battery measure with a multimeter (see § 5) |
| **Radio pirate** (Pirate radio) | sends a melody, a 1 kHz tone or a WAV file of the SD card in FM on 433 MHz, to listen to with a Flipper Zero (Sub-GHz > Read RAW, FM476, sound on), a Portapack, an SDR or another cicada; settings: deviation (47.6 kHz "Flipper" by default, 5 kHz for an NFM receiver), sound gain x1 / x2 / x4 (see [pirate_radio.md](pirate_radio.md)); low power, short tests |
| **Mode démo** (Demo mode) | for a stand: the badge shows its features in a loop (see below) |
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

**Mise en sommeil** (sleep, the last of the Commandes radio): the other badges switch everything off (radio, LEDs,
sound, services) and show "SOMMEIL: le badge a été mis en sommeil par un admin. S'il reste coincé, rapprochez-vous
d'un organisateur." (put to sleep by an admin, see an organizer if it stays stuck); a reboot does not wake them up.
Except a badge on its **talk badge** page, and the admin badge that sends the order. The order goes by the network of
the badges, not in Princeton: the Princeton remote of a Flipper cannot put the conference to sleep (a network
`.sub` file made by `tools/flipper_net_sub.py command 0x05` can: keep it for the organizers). **Unlock** (organizers): 5 times the left
flank, then 5 times the right one (less than 5 s between two presses); the badge restarts normally.

**Remise à zéro** (Reset): before the event or after tests. Flanks: choose; right wing, then a **long press on the
right wing** to confirm (left wing: no):
- "Scores sociaux" (social scores): the score and the encounters of the cicada network;
- "Records des jeux" (game records): the records of the games and the puzzles;
- "Défis CTF et crypto" (CTF and crypto challenges): the CTF flags and the crypto challenges solved;
- "Contacts reçus" (contacts received): the business cards received (not your card);
- "Virus": the state of the virus (healthy);
- "Succès et niveau" (achievements and level): the achievements obtained and their counters (the level starts again
  from 1; the encounters still count in the XP as long as the social scores are not reset);
- "Contrebande" (smuggling): the hold of the smuggling game (the goods are given again at the next opening);
- "Tout" (all): all of this at once, plus the progress of the gamebook. The name, the settings and your business card
  are kept;
- "Annonces d'origine" (original announcements): the 6 announcements of the admin menu get their original texts back
  (not part of "Tout").

**Mode démo** (Demo mode): right wing: start. The badge goes through, in a loop, with rainbow LEDs: name tag (8 s),
achievements (6 s), images of the SD card (18 s, a new one every 6 s), the first video (20 s), skills (5 s),
the first music (10 s), screen demo (15 s), program (6 s), radar (6 s), gamebooks (5 s), credits (6 s), info (5 s).
Without an SD card (or without a file), the images, the video and the music are skipped. The screensaver does not
start during the demo; **any button stops it** ("Mode démo arrêté"), and the LEDs go back to their animation.


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
  Escape: cancel; Backspace: erase. The arrow keys are still the buttons. Through the keyboard: the characters
  without accent and the letters é è ê à â ç ô î ù û ë ï É È À Ç; the others are chosen with the flanks.
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
├── TEXTES/     .txt texts for speed reading
├── SONNERIES/  RTTTL ringtones .txt, .rtttl, .rtx or PICAXE .bas, sub-folders allowed, any number of files
├── RTTTL/      same (either one)
└── LIVRES/     .txt gamebooks (16 at most)
```

Examples of `SONNERIES` and `LIVRES` are in the repository, in [docs/sd/](../sd): copy these folders to the root of
the card.

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

A WAV file that was not converted plays too (8, 16, 24 or 32-bit PCM, 32-bit float, up to 192 kHz), but quieter and
less clear on the buzzer.

**Texts**: any `.txt` file (UTF-8 or Windows/Latin-1) in `TEXTES`.

**Ringtones**: a text file, one RTTTL ringtone per line (`name:d=4,o=5,b=120:e,e,f,g`), see
[ringtones.md](ringtones.md).

**Gamebooks**: one text file per book, with sections `== 12` and choices `-> 34 : ...`, see
[gamebooks.md](gamebooks.md); `python tools/gamebook_check.py LIVRES/my_book.txt` checks it before you copy it.

> Respect copyright: use works that are free of rights or that you hold the rights to.


## 8. Troubleshooting

| Problem | Solution |
|---|---|
| The badge does not turn on | Is the switch on ON? Is the battery charged (plug it into USB)? |
| The computer does not see the badge | Use a USB data cable (not a charge-only cable); switch on ON. |
| "Carte SD absente" (SD card missing) | Is the card pushed all the way in? Formatted as FAT32 or exFAT? |
| A file does not show up | Right extension (`.epi`, `.epv`, `.wav`, `.txt`, `.rtttl`, `.rtx`, `.bas`) and right folder? Name shorter than 64 characters? |
| A ringtone is marked `(!)` | An error in the line: the right wing shows the column and the reason (see [ringtones.md](ringtones.md)). |
| No sound | Volume at 0? (Médias > Volume). The buzzer is quiet: put your ear close to it. |
| The radio message does not reach the Flipper | The Flipper must be on 433.92 MHz in "SubGHz chat". In Infos, the crystal must be 26 or 27 MHz. |
| No more sound or LEDs | The mute mode may be on (command of the organizers during a talk): Réglages > Mode muet. |
| The badge does not obey the Flipper | Réglages > Télécommande : oui? Princeton code `0xC16Axx`, sent from a `.sub` file and held for a second. |
| The other cicadas are no longer heard | Normal while the 433 MHz decoder, the weather station, the 433 MHz hunt, the talk badge, the contact exchange or the pirate radio (sending or listening) is open: they use the radio. |
| "Trop loin ou absente" at the assassin, no cicada "à portée de main" in the smuggling game | Hold the badges against each other: these games need a very strong signal. |
| The remote controls or the other cicadas are poorly received | Réglages > Réglage radio, close to other badges switched on. |
| The screen keeps ghost images | Normal after many fast refreshes: it cleans itself at the next full refresh. |
| The badge stops responding | Press the RESET button, or switch it off and on again. |
