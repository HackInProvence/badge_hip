# Badge SecSea — guide développeur

Ce guide s'adresse aux personnes qui veulent compiler, modifier ou étendre le logiciel du badge.
Il décrit l'architecture, les mécanismes mis en œuvre, les formats de fichiers, les protocoles et les tests.
Pour l'utilisation du badge, voir le [guide utilisateur](guide_utilisateur.md).
L'installation du SDK est détaillée dans le [README](../../README.md).

*English version: [developer guide](../en/developer_guide.md).*


## 1. Matériel

| Élément | Composant | Liaison (GPIO) |
|---|---|---|
| Microcontrôleur | RP2040, flash W25Q128 16 Mo | — |
| Écran | e-Paper 1,54" 200×200, contrôleur SSD1681 | SPI0 : SCK 6, MOSI 7, MISO 4 ; DC 8, BUSY 9, RST 10 |
| Carte SD | lecteur micro-SD soudé | SPI0 partagé, CS commandé à la main |
| Sélection SPI0 | décodeur 74HC139 | A0/A1 = 18/19, CSn = 5 |
| Radio | CC1101 433 MHz | SPI1 : SCK 26, MOSI 27, MISO 24, CS 25 ; GDO0/GDO2 |
| Boutons | 4 poussoirs | Y (flanc G) 0, A (aile G) 1, B (aile D) 14, X (flanc D) 15 |
| LEDs | 6 × WS2812 | 11 (PIO) |
| Buzzer | via transistor 2N7002 | 28 (PWM) |
| Batterie | TP4056 (charge), pont 100k/200k + LM321 | 29 (ADC3) |
| Extensions | 2 connecteurs 2×6 | port droit J2 (IR), port gauche J3 (I2C1 : SDA 2, SCL 3) |

Les brochages sont dans [src/pinouts.h](../../src/pinouts.h) et la carte dans [src/badge_secsea.h](../../src/badge_secsea.h)
(`-DPICO_BOARD=badge_secsea`). Les schémas sont dans [hardware/](../../hardware).

Points d'attention :
- **Quartz de la radio** : le code d'origine supposait 26,998 MHz ; les badges mesurés ont un quartz de 26 MHz.
  Le firmware le mesure au démarrage et se cale sur la valeur nominale la plus proche (26 ou 27 MHz),
  car la mesure varie d'environ 500 ppm.
- **Mesure de batterie** : le LM321 est alimenté par la batterie elle-même. Son entrée et sa sortie ne dépassent pas
  environ V_bat − 1,5 V, alors que le pont lui présente 2/3 V_bat : il sature.
  La mesure doit être calibrée (voir [§ 6.9](#69-batterie)).
  Pour une prochaine version de la carte : un ampli rail-à-rail (MCP6001...) ou le pont directement sur l'ADC.
- **Interrupteur** : sur OFF, le badge n'est pas alimenté, même en USB (seule la charge fonctionne).


## 2. Compiler et flasher

Pré-requis : le Pico SDK 2.x (par l'extension VS Code « Raspberry Pi Pico » ou `pico_setup.sh`, voir le README),
Python 3 avec Pillow (conversion des images à la compilation).

```bash
mkdir build && cd build
cmake .. -G Ninja -DPICO_BOARD=badge_secsea
ninja badge_menu            # l'application principale : build/src/menu/badge_menu.uf2
```

Sous Windows, si `python3` n'est que l'alias du Microsoft Store, préciser l'interpréteur :
`-DPython3_EXECUTABLE=C:/.../python.exe`.

Pour flasher :
- **avec picotool**, badge allumé et branché : `picotool load -f -x build/src/menu/badge_menu.uf2`
  (`-f` redémarre le badge en mode flash, `-x` lance l'application) ;
- **sans picotool** : maintenir BOOTLOADER en branchant le badge, puis copier le `.uf2` sur le disque « RPI-RP2 » qui apparaît.

Autres exécutables : les applications de test de chaque module (`src/tests/*.c`, cibles `test_screen`, `test_radio`...).


## 3. Organisation du dépôt

```
src/
├── pinouts.h, badge_*.h   brochages et définitions de cartes
├── screen/     pilote de l'écran e-Paper SSD1681 (formes d'onde, fenêtres, rafraîchissement rapide)
├── gfx/        dessin 1 bit : rectangles, texte UTF-8, polices générées (gen_fonts.py)
├── sd/         pilote SD en SPI + FatFs R0.15 (lecture seule)
├── audio/      sortie son PWM + DMA, lecteur WAV, audio2wav.py
├── video/      lecteur .EPV (image + son), video2epaper.py
├── images/     image2epi.py (images .EPI pour la carte SD)
├── radio/      pilote CC1101
├── ir/         infrarouge : réception, émission 38 kHz, décodage NEC
├── oled/       écran OLED SSD1306 en I2C
├── leds/       WS2812 et animations
├── btns/       boutons
├── music/, noise_gen/   mélodies et chant de la cigale
├── log/        journaux désactivables
├── menu/       application principale badge_menu (voir § 5)
├── tests/      applications de test sur le badge + tests PC (tests/host)
└── image2epaper.py   conversion des images intégrées au firmware
tools/
├── badge_remote.py     écran du badge sur le PC + pilotage au clavier
└── badge_selftest.py   test automatique du badge par l'USB
docs/           documentation, idées, PVSR
hardware/       schémas et projet KiCad
```

Chaque module est une bibliothèque CMake (`add_library(... INTERFACE)`), liée par les applications qui en ont besoin,
comme les bibliothèques du SDK.


## 4. La règle d'or : ne jamais bloquer

Il n'y a pas de système d'exploitation. `main()` est une boucle qui appelle la tâche de chaque module ;
**aucune fonction ne doit bloquer plus de 20 ms** (hors initialisation).
Tout ce qui dure est une machine à états, avancée à chaque tour de boucle :

```c
while (true) {
    absolute_time_t now = get_absolute_time();
    uint8_t pressed = buttons_pressed(now);   // fronts des boutons (anti-rebond 20 ms) + touches USB
    ... traitement des boutons selon la page affichée (app) ...
    radio_tools_task(now); social_task(now); battery_task(now); store_task(now); ...
    switch (app) { ... video_task(now) / rsvp_task(now) / games_task(now) / display_task(now) ... }
}
```

Le son, les LEDs et la réception infrarouge fonctionnent par interruptions et DMA, sans la boucle.
Exemples de découpage :
- le joueur de Puissance 4 évalue une colonne par tour de boucle ;
- l'écran est piloté par étapes (envoi, attente de BUSY, fin) ;
- la sauvegarde en flash est différée de 5 s.

Interruptions GPIO : le SDK n'a qu'un seul callback GPIO par cœur. Les modules utilisent donc
`gpio_add_raw_irq_handler()` (un gestionnaire par broche), pour cohabiter sans se remplacer.


## 5. L'application `badge_menu`

[src/menu/main.c](../../src/menu/main.c) contient l'interface : l'état `app` (menu, navigateur de fichiers, lecteur,
jeu...), les pages dessinées dans un frame buffer, les actions des boutons.

| Fichier | Rôle |
|---|---|
| `main.c` | menus par thèmes (`SUBMENUS`), navigateur SD, lecteurs, réglages, veille, nom de la cigale, CTF, protocole USB |
| `display.c` | affichage non bloquant d'un frame buffer (rafraîchissement rapide ou complet, mise en veille de l'écran) |
| `games.c` | les mini-jeux, indépendants du matériel (sons, LEDs, hasard fournis par des *hooks*) |
| `rsvp.c` | lecture rapide PVSR ([docs/pvsr.md](../pvsr.md)) |
| `social.c` | réseau des cigales (balises radio, rencontres, score) |
| `radio_tools.c` | message et porteuse radio, mesure du quartz |
| `credits.c` | pages des crédits |
| `battery.c` | niveau de batterie (calibration) |
| `store.c` | réglages et scores en flash |
| `ctf.c`, `oled_demo.c`, `screen_demo.c` | CTF, démos OLED, démo de l'écran |

### Ajouter une entrée de menu

1. Ajouter une valeur à `menu_item_t` et son libellé dans `item_label()`.
2. La placer dans un thème de `SUBMENUS` (8 entrées au plus par thème).
3. Dans `validate()`, lancer l'action ou passer dans un nouvel état `app` (`app_state_t`).
4. Si l'état a sa propre page :
   - traiter ses boutons dans la boucle ;
   - la dessiner dans le `if (redraw)` ;
   - gérer le retour dans `cancel()`.
5. Appeler `draw_title()` (ou `ui_trace()`) pour que la page soit tracée `ui: <titre>` sur l'USB,
   ce qu'utilisent les tests automatiques.

### Ajouter un jeu

Les jeux ([games.h](../../src/menu/games.h)) ne dépendent que de `gfx`. Un jeu fournit 5 fonctions :
- `*_buttons()` ;
- `*_task()` ;
- `*_render()` ;
- un démarrage ;
- un état « calme » pour `games_calm()`.

Il les branche dans les `switch` de l'API. Les records sont dans `store_t.game_records` (0xFFFF = aucun).
Ajouter ensuite des tests dans [tests/host/test_games.c](../../src/tests/host/test_games.c).


## 6. Mécanismes

### 6.1 Écran e-Paper (SSD1681)

- **Formes d'onde** :
  - `screen_ws_1681_bw` : rafraîchissement complet noir et blanc, environ 2 s ;
  - `screen_ws_1681_4grays` : 4 niveaux de gris, avec les deux RAM ;
  - `screen_ws_10fps`, `20fps`, `30fps` : mode *multiframe* rapide, seules les différences sont redessinées.
- **Orientation** : l'entrée des données décrémente X et Y ; l'octet RAM (x, y) est l'octet (24 − x, 199 − y) de l'image.
  `screen_set_image_position()` gère les fenêtres, y compris les octets partiellement couverts.
- **Deep sleep** : après 20 s sans mise à jour, `display.c` endort l'écran (recommandation du constructeur).
- **Copie de l'écran** : `screen_shot()` renvoie une copie de ce qui a été envoyé (shadow des deux RAM) et sa nature
  (BW, 4G, blanc, noir). C'est ce qu'envoie le protocole USB (`@FB`).
- `display.c` :
  - enchaîne des rafraîchissements rapides (RAM « nouvelle image » + RAM « image affichée ») ;
  - fait un rafraîchissement complet tous les 15, pour effacer les fantômes ;
  - suspend ce rafraîchissement complet pendant un jeu en action (`display_set_periodic_full(false)`).

### 6.2 SPI0 partagé : écran et carte SD

L'écran et la carte SD sont sur le même SPI0. Un décodeur 74HC139 sélectionne l'écran selon A0/A1 ;
le CS de la carte SD est piloté comme une GPIO. Un seul utilisateur à la fois :
- le navigateur et les lecteurs attendent `display_is_idle()` avant de lire la carte ;
- ils appellent `display_invalidate()` ensuite, pour forcer un rafraîchissement complet.

### 6.3 Carte SD et FatFs

- FatFs R0.15 en lecture seule, avec :
  - noms longs, **en UTF-8** (`FF_LFN_UNICODE 2`) ;
  - exFAT et partitions GPT (`FF_LBA64`) ;
  - `f_findfirst` ;
  - page de code 437.
- `sd_mount()` monte à la demande ; `sd_list_files()` et `sd_list_dirs()` rendent des listes triées
  (noms de moins de 64 octets).

### 6.4 Son

- **Sortie** :
  - PWM sur GPIO28 à environ 61 kHz, avec la valeur de l'échantillon comme rapport cyclique ;
  - les échantillons sont poussés par DMA, cadencé par un timer DMA à la fréquence d'échantillonnage ;
  - un tampon circulaire de 8192 échantillons (8 bits) est rempli par `audio_write()`.
- **Volume** : 0 à 8, 6 par défaut.
- **Silence** : le temps de silence est compté pour garder la position de lecture juste.
- `wav.c` lit les WAV PCM ; la vidéo utilise le son comme horloge.
- `audio2wav.py` et `video2epaper.py` compressent la dynamique (passe-haut 250 Hz, passe-bas 5 kHz, compresseur,
  normalisation) : le buzzer a peu de rendement, le son doit être fort et médium.

### 6.5 Vidéo

- `video_start(path)` ouvre un `.EPV`.
- Les images sont affichées en *multiframe* rapide.
- Le son est lu depuis la fin du fichier, avec un second descripteur de fichier.
- L'horloge est la position audio : quand l'écran prend du retard, des images sont sautées.

### 6.6 Radio CC1101

- `radio.c` fournit :
  - `radio_reset` ;
  - l'accès aux registres ;
  - `radio_tx_packet` ;
  - la puissance (`PATABLE` : 0xC0 = +10 dBm, 0x0E = −20 dBm) ;
  - le réglage du quartz (`radio_set_xosc`).
- **Message** : préréglage GFSK 9,99 kbit/s du Flipper Zero, mot de synchronisation 0x464C, compatible avec
  l'application *SubGHz chat*.
- **Réseau des cigales** (`social.c`, voir [docs/idees_reseau_extensions_ctf.md](../idees_reseau_extensions_ctf.md)) :
  - une balise toutes les 2 s ± 0,5 s à −20 dBm, mot de synchronisation 0xC16A ;
  - paquet de 17 octets : `0xC1`, type 1, identifiant (hash FNV de l'identifiant unique), numéro, score, nom (8 caractères) ;
  - une rencontre = RSSI ≥ −50 dBm sur 3 balises en 10 s ;
  - +10 points pour un nouveau badge, +1 pour un badge connu, au plus une fois par heure.

### 6.7 Infrarouge

- **Réception** : interruption sur les fronts, durées marque / espace.
- **Émission** : porteuse 38 kHz par PWM, modulée par une alarme.
- **Décodage** : `ir_decode_nec()` décode le NEC (standard et étendu), avec une tolérance de ±25 %.
- Quatre emplacements sont gardés en flash.

### 6.8 Stockage en flash

`store.c` garde un `store_t` dans le dernier secteur de 4 Ko de la flash :
- nom, score et rencontres ;
- flags du CTF ;
- signaux IR ;
- réglages (vitesse et position de lecture, veille) ;
- records des jeux.

Principe :
- l'écriture a lieu 5 s après la dernière modification, avec `flash_safe_execute` (environ 50 ms, interruptions coupées) ;
- les nouveaux champs sont ajoutés **à la fin** de la structure : dans un secteur écrit par une version précédente,
  ils valent 0xFF, ce que leurs utilisateurs vérifient ;
- `_Static_assert` garantit que la structure tient dans le secteur.

### 6.9 Batterie

- La mesure brute (ADC3, 16 échantillons, filtrée) n'est convertie en tension qu'avec une **calibration en deux points**
  (`BATTERY_CAL_RAW1/MV1/RAW2/MV2` dans [battery.h](../../src/menu/battery.h)).
- Sans calibration, rien n'est affiché : le badge ne doit jamais afficher de valeur fausse.
- Pour calibrer :
  1. lire la ligne `battery:` du diagnostic `!` (valeur `ADC raw`) ;
  2. relever au même moment la tension de la batterie au multimètre ;
  3. recommencer une fois en charge et une fois sur batterie ;
  4. renseigner les quatre valeurs.

### 6.10 Texte et polices

- `gfx_text()` dessine de l'UTF-8.
- Polices générées par `gen_fonts.py` (Aileron) :
  - ASCII et accents français ;
  - chasse en 1/16 de pixel, pour un espacement régulier.
- Une lettre accentuée absente de la police est dessinée sans son accent ; tout autre caractère inconnu devient « ? ».
- `gfx_set_size()` permet de dessiner pour l'OLED 128×64.


## 7. Formats de fichiers

| Format | Contenu |
|---|---|
| `.EPI` (image) | en-tête de 16 octets : `EPIMAGE1`, largeur, hauteur, bits par pixel (1 ou 2), 0 (uint16 little endian) ; puis le plan « lsb », puis le plan « msb » (si 2 bits). Chaque plan fait 5000 octets, bit 7 = pixel de gauche, ligne par ligne. Gris = msb×2 + lsb (0 = noir, 3 = blanc). |
| `.EPV` (vidéo) | en-tête de 512 octets : `EPVIDEO2`, largeur, hauteur, fps, bits par pixel, nombre d'images, fréquence audio, nombre d'échantillons ; puis les images de 5000 octets (1 = blanc) ; puis le son, 8 bits non signé, synchronisé sur la première image. `EPVIDEO1` = sans son. |
| `.WAV` | PCM 8 ou 16 bits, mono ou stéréo ; 8 bits 16 kHz mono conseillé (`audio2wav.py`). |
| `.TXT` | UTF-8 (avec ou sans BOM) ou Windows-1252, détecté automatiquement. |

Les images intégrées au firmware (démo écran, logos) sont converties à la compilation par `image2epaper.py`
(fonction CMake `badge_image2epaper`), en tableaux C.


## 8. Le protocole USB série

Le badge est un port série USB (115200 bauds, sans importance en USB). Une touche par caractère :

| Touche | Action |
|---|---|
| `a` `b` `x` `y` | appui court : aile G, aile D, flanc D, flanc G |
| `B` `X` `Y` | appui long (enregistrer le nom, avance / recul de la lecture rapide) |
| `[` / `]` | envoi de l'écran à chaque changement : marche / arrêt |
| `s` | envoi de l'écran une fois |
| `!` | diagnostic : OLED, IR, réseau des cigales, CTF, radio (version, quartz), batterie |
| `?` | diagnostic du son |
| `i` | test IR : décodage d'une trame NEC puis émission (environ 68 ms) |
| `R` | redémarrage (watchdog) |

Le badge envoie des lignes de texte :
- `ui: <titre>` à chaque changement de page ;
- `browser:`, `image:`, `saver: on/off`, `radio:`, `social:`, `game:`, `ctf: code right/wrong`, `credits:`, `name:`...

L'écran est envoyé ainsi :

```
@FB <BW|4G|WHITE|BLACK> [<plan lsb en base64> [<plan msb en base64>]]
```

Les messages envoyés avant l'ouverture du port par le PC sont perdus : c'est une limite de l'USB CDC.


## 9. Tests

### 9.1 Tests sur PC (sans badge)

```bash
python src/tests/host/run_tests.py          # tous les tests
python src/tests/host/run_tests.py games -v # un test, avec ses traces
```

Les modules C sont compilés avec le compilateur du PC (gcc ou clang, variable `CC`).
Des remplaçants du Pico SDK sont fournis dans `tests/host/stubs` : GPIO, SPI enregistré, temps simulé.

| Test | Vérifie |
|---|---|
| `gfx` | format du frame buffer, découpage, texte UTF-8, accents, tailles |
| `ir` | décodage NEC exact, tolérance ±20 %, adresses étendues, trames invalides |
| `games` | le morpion ne perd jamais (toutes les parties), Puissance 4 gagne / bloque, règles de Simon, Réflexes et Snake, records |
| `rsvp` | découpage des mots, typographie française, BOM, Windows-1252, durées, mots longs, avance / recul |
| `screen` | fenêtres RAM, copie de l'écran, rétablissement après `screen_clear` |
| `image2epi`, `video2epaper`, `audio2wav` | fichiers produits (en-têtes, tailles, son non vide). Les deux derniers sont ignorés sans ffmpeg. |

### 9.2 Test du badge (par l'USB)

```bash
python tools/badge_selftest.py              # badge_menu flashé, port série libre
python tools/badge_selftest.py --only games,radio -v
python tools/badge_selftest.py --ctf         # teste aussi le code Konami (marque le flag comme trouvé)
```

Déroulement :
1. Le script redémarre le badge (`R`).
2. Il simule les boutons et vérifie les traces : menus, jeux, veille, crédits, radio, balises, IR, images de la carte SD.
3. Il écrit un tableau PASS / FAIL / SKIP et des captures d'écran PNG (dossier `selftest_<date>`).

Un test est SKIP quand le matériel manque (pas de carte SD, batterie non calibrée).
Aucun réglage du badge n'est modifié, sauf avec `--ctf`.

### 9.3 Applications de test par module

`src/tests/*.c` : une application par module (écran, radio, LEDs, son...), à flasher pour explorer un périphérique.


## 10. Outils

| Script | Usage |
|---|---|
| `tools/badge_remote.py` | fenêtre avec l'écran du badge en grand (zoom 2–4), flèches / Entrée / Échap = boutons, capture PNG ; `--snapshot fichier.png` pour une capture seule |
| `tools/badge_selftest.py` | test automatique du badge (§ 9.2) |
| `src/images/image2epi.py` | images → `.EPI` (options `--fit`, `--bw`, `--contrast`, `--equalize`, `--preview`) |
| `src/video/video2epaper.py` | vidéo → `.EPV` (`--fps`, `--fit`, `--dither bayer\|fs\|threshold`, `--start`, `--duration`, `--no-audio`, `--preview`) |
| `src/audio/audio2wav.py` | sons → WAV 8 bits 16 kHz (jokers, dossiers, `--no-filter`, `--start`, `--duration`) |
| `src/image2epaper.py` | images → tableaux C (utilisé par CMake) |
| `src/gfx/gen_fonts.py` | génère `gfx_fonts.c` depuis une police TrueType |


## 11. Dépannage du développement

| Symptôme | Piste |
|---|---|
| Le badge n'apparaît pas en USB | interrupteur sur ON ; câble de données ; mode BOOTLOADER pour flasher |
| `picotool` ne trouve pas le badge | le port série est-il ouvert par une autre application (badge_remote, terminal) ? |
| Aucun paquet radio reçu | quartz : comparer « Quartz mesuré » et « utilisé » dans Infos ; même préréglage des deux côtés |
| Écran figé ou uniforme | appel pendant `screen_busy()` ; après `screen_clear()`, le bypass RAM est rétabli au dessin suivant |
| Son inaudible | volume ; fichier converti par `audio2wav.py` (sans `--no-filter`) |
| Deux modules se disputent une interruption GPIO | utiliser `gpio_add_raw_irq_handler()` |
| Nouveau champ de `store_t` à 0xFF | normal pour un badge déjà utilisé : tester et prendre une valeur par défaut |
