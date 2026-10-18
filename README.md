# Badge SecSea — la cigale

![Le badge](imgs/badge_front.webp)

Le badge électronique de [SecSea](https://secsea.org/), la conférence de hacking de **Hack In Provence** à La Ciotat.
Les cigales se rencontrent par radio, échangent leurs contacts et des messages, jouent ensemble (mini-bolides,
loup-garou, tir à la corde...), chantent en chœur, affichent le programme d'une conférence, et servent d'outil
pédagogique de radio (433 MHz) et d'infrarouge.

Version : voir [VERSION](VERSION) et l'[historique des versions](CHANGELOG.md). *English: [below](#english).*


## Le matériel

| | |
|---|---|
| Microcontrôleur | RP2040 (2 cœurs à 133 MHz, 264 Ko de RAM), flash 16 Mo |
| Écran | e-Paper 1,54" 200 × 200 noir et blanc (SSD1681) |
| Radio | CC1101, 433 MHz, antenne céramique |
| Boutons | 4 : **Y A B X** de gauche à droite (A et B sur les ailes, Y et X sur les flancs) |
| Lumière, son | 2 LEDs RGB WS2812B, un buzzer |
| Stockage | carte micro-SD (images, vidéos, musique, livres-jeux, programmes de conférences...) |
| Alimentation | USB-C, batterie Li-ion avec chargeur (TP4056), régulateur 3,3 V, interrupteur |
| Extensions | 2 ports 2 × 6 : I2C, UART, SPI, infrarouge |

Le rôle de chaque composant : [l'électronique du badge](docs/fr/electronique.md). Le projet KiCad est dans
[hardware/kicad](hardware/kicad).


## Mettre en route un badge

### 1. Flasher le firmware

Le firmware est le fichier **`badge_menu.uf2`** : dans les [releases](../../releases) du dépôt, ou compilé (plus bas).

1. Interrupteur du badge sur **ON**.
2. Maintenir le bouton **BOOTSEL** (« BOOTLOAD ») du badge, brancher le câble USB, relâcher le bouton.
3. Un disque **RPI-RP2** apparaît sur l'ordinateur : copier le fichier `.uf2` dessus.
4. Le badge redémarre tout seul avec le nouveau firmware.

Avec [picotool](https://github.com/raspberrypi/picotool), sans toucher au bouton : `picotool load -f -x badge_menu.uf2`.

Au premier démarrage, le badge règle sa radio (quelques secondes), puis affiche le menu. Le numéro de version est
dans Réglages > Infos.

### 2. Préparer la carte micro-SD (facultatif)

Carte formatée en FAT32 ou exFAT. Copier à la racine le contenu de [docs/sd](docs/sd) (exemples : sonneries,
livres-jeux, programmes de conférences), puis vos images, vidéos et musiques converties (outils plus bas).

Sans sortir la carte, par l'USB : `python tools/sd_files.py` (une fenêtre : le PC à gauche, la carte à droite).

### 3. Utiliser le badge

Y / X : monter / descendre ; B : choisir ; A : retour (appui long sur A pour quitter un jeu). Tout est dans le
[guide utilisateur](docs/fr/guide_utilisateur.md).

### 4. Les outils du PC

Python 3, et selon l'outil `pip install pyserial pillow numpy` et [ffmpeg](https://ffmpeg.org/) dans le PATH.

| Outil | Usage |
|---|---|
| [tools/badge_remote.py](tools/badge_remote.py) | l'écran du badge en grand sur le PC, la télécommande, les captures (pyserial) |
| [tools/sd_files.py](tools/sd_files.py) | les fichiers de la carte SD par l'USB, dates conservées (pyserial) |
| [tools/contacts_export.py](tools/contacts_export.py) | les contacts reçus par le badge en `.vcf` (pyserial) |
| [src/images/image2epi.py](src/images/image2epi.py) | images → `.epi` pour l'écran (pillow) |
| [src/video/video2epaper.py](src/video/video2epaper.py) | vidéos → `.epv` (ffmpeg, numpy, pillow) |
| [src/audio/audio2wav.py](src/audio/audio2wav.py) | sons → `.wav` 8 bits 16 kHz (ffmpeg) |
| [tools/rtttl_sort.py](tools/rtttl_sort.py) | trier une collection de sonneries RTTTL |
| [tools/gamebook_check.py](tools/gamebook_check.py) | vérifier un livre-jeu avant de le copier |

Le site de la documentation, avec ces outils et le firmware : `python tools/docs_site.py` (dans `build/docs_site`).


## Compiler le firmware

Pré-requis : le [Pico SDK](https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html) 2.x, CMake, Ninja,
la chaîne ARM GCC, Python 3 avec Pillow. Le plus simple : l'extension VS Code **Raspberry Pi Pico**, qui installe tout
(ou le script `pico_setup.sh` de Raspberry Pi sous Debian / Ubuntu).

```bash
git clone https://github.com/HackInProvence/badge_hip.git
cd badge_hip
mkdir build && cd build
cmake .. -G Ninja -DPICO_BOARD=badge_secsea
ninja badge_menu           # → build/src/menu/badge_menu.uf2
```

Tests sur le PC : `python src/tests/host/run_tests.py` ; sur un badge branché : `python tools/badge_selftest.py`.

Tout le détail (architecture, mécanismes, formats, protocole série, tests, nouvelle version) : le
[guide développeur](docs/fr/guide_developpeur.md).


## Documentation

| | Français | English |
|---|---|---|
| Utiliser le badge | [Guide utilisateur](docs/fr/guide_utilisateur.md) | [User guide](docs/en/user_guide.md) |
| Organisateurs, carte SD, outils, flashage, dépannage | [Guide avancé](docs/fr/guide_avance.md) | [Advanced guide](docs/en/advanced_guide.md) |
| Développer | [Guide développeur](docs/fr/guide_developpeur.md) | [Developer guide](docs/en/developer_guide.md) |
| Électronique | [L'électronique du badge](docs/fr/electronique.md) | [Electronics](docs/en/electronics.md) |
| Tous les écrans | [Écrans](docs/fr/ecrans.md) | [Screens](docs/en/screens.md) |
| Badge conférence | [Badge conférence](docs/fr/badge_conference.md) | [Conference badge](docs/en/conference_badge.md) |
| Traduire | [Traduction](docs/fr/traduction.md) | [Translation](docs/en/translation.md) |

Notes de conception : [réseau, extensions et CTF](docs/idees_reseau_extensions_ctf.md),
[idées](docs/idees_brainstorm.md), [module ESP32](docs/etude_module_esp32.md) et [ses usages](docs/idees_esp32.md).


## Licence

[CC BY-NC-SA 4.0](LICENSE) — Hack In Provence.


---

## English

The electronic badge of [SecSea](https://secsea.org/), the hacking conference of Hack In Provence in La Ciotat
(France): an RP2040, a 200 × 200 e-Paper screen, a CC1101 433 MHz radio, 2 RGB LEDs, a buzzer, 4 buttons
(**Y A B X** from left to right), a micro-SD card, USB-C and a Li-ion battery.

- **Flash it**: hold **BOOTSEL**, plug the USB cable, copy `badge_menu.uf2` (from the [releases](../../releases)) to
  the **RPI-RP2** drive. Or `picotool load -f -x badge_menu.uf2`.
- **SD card** (optional): copy [docs/sd](docs/sd) to a FAT32 / exFAT card, or use `python tools/sd_files.py` over
  USB.
- **Build**: Pico SDK 2.x, then `cmake .. -G Ninja -DPICO_BOARD=badge_secsea && ninja badge_menu` in `build/`.
- **Docs**: [user guide](docs/en/user_guide.md), [advanced guide](docs/en/advanced_guide.md),
  [developer guide](docs/en/developer_guide.md), [electronics](docs/en/electronics.md),
  [changelog](CHANGELOG.md) (in French).
