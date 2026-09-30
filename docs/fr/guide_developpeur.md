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
| LEDs | 2 × WS2812 (RGB) | 11 (PIO) |
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
├── radio/      pilote CC1101, décodeurs OOK 433 MHz (ookdec.c : télécommandes, sondes météo)
├── ir/         infrarouge : réception, émission 38 kHz, décodage NEC
├── qrcode/     qrcodegen de Project Nayuki (QR codes des scores et du programme)
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
├── badge_selftest.py   test automatique du badge par l'USB
├── score_check.py      vérification des QR codes de score, classement
├── contacts_export.py  cartes de visite reçues par le badge → fichier vCard
├── crypto_ctf_make.py  génère les défis crypto (fichier des solutions : spoilers)
├── ook_sub.py          fichiers .sub du Flipper Zero : télécommandes et sondes météo de test
└── flipper_net_sub.py  fichiers .sub du Flipper Zero : paquets du réseau des cigales, préréglage « SecSea »
docs/           documentation, idées, PVSR, synchronisation du choeur (chorus_sync.md)
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
    ... traitement des boutons selon la page affichée (app), ou cur_app->buttons() pour une application ...
    radio_tools_task(now); net_task(now); remote_task(now); ook_rx_task(now);
    vote_task(now); infection_task(now); chorus_task(now); ... notifications ...
    social_task(now); battery_task(now); store_task(now); ...
    switch (app) { ... video_task(now) / rsvp_task(now) / games_task(now) / cur_app->task(now) / display_task(now) ... }
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
| `main.c` | menus par thèmes (`SUBMENUS`), mode admin, notifications, navigateur SD, lecteurs, réglages, veille, nom de la cigale, CTF, protocole USB ; ouvre et fait tourner les applications |
| `app.h`, `apps.h`, `apps.c` | le cadre des applications (`app_t`) et la liste des applications des menus |
| `ui.c` | aides de dessin communes (titre, pied de page, listes, jauge...) et éditeur de texte à 4 boutons |
| `display.c` | affichage non bloquant d'un frame buffer (rafraîchissement rapide ou complet, mise en veille de l'écran) |
| `games.c` | les mini-jeux, indépendants du matériel (sons, LEDs, hasard fournis par des *hooks*) |
| `puzzles.c`, `puzzles_app.c` | les casse-têtes (Démineur, 2048, Taquin, Sokoban, Mastermind, Pendu), indépendants du matériel, et leurs applications |
| `rsvp.c` | lecture rapide PVSR ([docs/pvsr.md](../pvsr.md)) |
| `net.c` | couche réseau radio partagée par toutes les fonctions entre badges (§ 6.12) |
| `social.c` | réseau des cigales (balises radio, rencontres, score, voisins) |
| `remote.c` | commandes à distance (badge admin, Flipper Zero), mode muet (§ 6.13) |
| `ook_rx.c`, `radio433.c` | récepteur OOK 433 MHz ; pages Décodeur 433 MHz et Station météo (§ 6.14) |
| `radio_tools.c` | message et porteuse radio, profil GFSK, mesure du quartz |
| `messages.c`, `contacts.c`, `program.c`, `vote.c` | messages relayés, cartes de visite, programme, votes |
| `hotcold.c`, `infection.c`, `chorus.c` | chasse chaud - froid et radar, virus des cigales, choeur (§ 6.15) |
| `duel.c`, `battle.c` | pierre-feuille-ciseaux et bataille navale entre deux badges (§ 6.16) |
| `image_radio.c` | envoi et réception d'une image par radio (§ 6.17) |
| `crypto_ctf.c`, `crypto_app.c` | défis de cryptographie et leurs pages (§ 6.18) |
| `lamp.c`, `nametag.c`, `talk.c`, `admin.c` | lampe, badge nominatif, badge de talk, commandes radio et type du badge (admin) |
| `credits.c` | pages des crédits |
| `score_code.c` | scores signés affichés en QR code (§ 6.11) |
| `battery.c` | niveau de batterie (calibration) |
| `store.c` | réglages, scores et cartes de visite en flash |
| `ctf.c`, `oled_demo.c`, `screen_demo.c` | CTF, démos OLED, démo de l'écran |

### Ajouter une application

Les nouvelles fonctions sont écrites comme des **applications** ([app.h](../../src/menu/app.h)) : `main.c` les ouvre
depuis les menus, leur passe les boutons, les fait tourner dans la boucle et dessine leur page quand elles le demandent.
Une application est un `const app_t` :

| Champ | Rôle |
|---|---|
| `name` | nom dans les menus, tracé `ui: <nom>` à l'ouverture |
| `label` | facultatif : texte dynamique dans les menus (« Lampe : 50 % ») |
| `start(now)` | à l'ouverture |
| `buttons(b, now)` | à chaque appui, relâchement ou appui long, et tant qu'un bouton est maintenu ; `false` : retour au menu |
| `task(now)` | facultatif : à chaque tour de boucle ; `true` quand la page doit être redessinée |
| `render(fb, now)` | dessine la page dans le frame buffer (déjà effacé en blanc) |
| `calm()` | facultatif : `false` tant que la page change souvent (pas de rafraîchissement complet lent) |
| `stop()` | facultatif : en quittant (LEDs, son, radio...) |
| `no_saver` | la veille ne doit pas démarrer (badge nominatif, radar, décodeur...) |
| `owns_leds` | l'application pilote les LEDs elle-même, même en mode muet (badge de talk) |

Les boutons arrivent dans un `app_buttons_t` : `pressed` (appuyés depuis le dernier appel), `released_short`
(relâchés sans appui long : à utiliser pour un bouton qui a aussi un appui long), `long_pressed` (maintenus
`APP_LONG_PRESS_MS` = 800 ms, signalé une fois), `held` et `held_ms[]` (pour répéter en maintenant).
Bits : `UI_BTN_A` (aile gauche), `UI_BTN_B` (aile droite), `UI_BTN_X` (flanc droit), `UI_BTN_Y` (flanc gauche).

Pour le son et les LEDs, utiliser `app_tone()` et `app_leds()` : ils respectent le mode muet ; `app_leds(0, 0, 0)`
rend les LEDs à l'animation du badge. `app_open(app)` ouvre une application depuis un service (une notification).

Pour ajouter une application :
1. écrire `foo.c` avec `const app_t app_foo = {...}` ;
2. ajouter `APP_FOO` à `app_id_t` dans [apps.h](../../src/menu/apps.h), et `[APP_FOO] = &app_foo` dans `APPS[]`
   ([apps.c](../../src/menu/apps.c)) ;
3. placer `M_APP(APP_FOO)` dans un thème de `SUBMENUS` (`main.c`) ;
4. ajouter `foo.c` à [src/menu/CMakeLists.txt](../../src/menu/CMakeLists.txt).

Un **service** (réception radio en tâche de fond) s'abonne à son type de paquet dans une fonction `*_init()` appelée
par `main()`, et signale un événement par une fonction interrogée dans la boucle (`vote_new()`, `duel_invited()`...) :
`notify()` émet un bip, écrit le texte en bas de l'écran et ouvre l'application si l'on est dans les menus ou la veille.

### Les aides de dessin et l'éditeur de texte (`ui.h`)

[ui.h](../../src/menu/ui.h) donne l'aspect commun des pages : `ui_title()` (bandeau noir), `ui_footer()`,
`ui_list()` (liste avec défilement), `ui_lines()` / `ui_text()` (lignes centrées ou à gauche), `ui_fit()`
(texte tronqué avec « ... »), `ui_box()`, `ui_gauge()`.

L'éditeur de texte à 4 boutons (`ui_edit_t`, 48 caractères au plus) sert aux cartes de visite, aux défis crypto
et au remède du virus :
- `ui_edit_start(e, texte, longueur, jeu_de_caractères)` : `UI_CHARSET_TEXT` (lettres, chiffres, ponctuation
  pour les noms, e-mails, URL), `UI_CHARSET_PHONE` (chiffres, +, espace), `UI_CHARSET_UPPER` (A-Z, 0-9, espace) ;
- flancs : `ui_edit_change(e, -1 / +1)` (la répétition en maintenant est à la charge de l'appelant) ;
- aile droite (appui court) : `ui_edit_move(e, 1)` ; aile gauche : `ui_edit_move(e, -1)`, qui renvoie `false`
  depuis la première position (annuler) ;
- aile droite (appui long) : terminé, `ui_edit_result()` rend le texte sans les espaces de fin ;
- `ui_edit_render()` dessine la question, le texte autour du curseur et le mode d'emploi.

### Ajouter une entrée de menu

Pour une entrée gérée directement par `main.c` (les fonctions historiques) :
1. Ajouter une valeur à `menu_item_t` et son libellé dans `item_label()`.
2. La placer dans un thème de `SUBMENUS` (16 entrées au plus par thème, `items[16]` de `submenu_t`).
   Le dernier thème, Admin, n'est affiché qu'en mode admin (`store_t.admin == STORE_ADMIN_ON`).
3. Dans `validate()`, lancer l'action ou passer dans un nouvel état `app` (`app_state_t`).
4. Si l'état a sa propre page :
   - traiter ses boutons dans la boucle ;
   - la dessiner dans le `if (redraw)` ;
   - gérer le retour dans `cancel()`.
5. Appeler `draw_title()` (ou `ui_trace()`) pour que la page soit tracée `ui: <titre>` sur l'USB,
   ce qu'utilisent les tests automatiques.

Une application (ci-dessus) évite les étapes 3 à 5.

**Mode admin** : dans le menu principal, la séquence `ADMIN_SEQUENCE` (`"LLRRLRLR"`, L = flanc gauche,
R = flanc droit) tapée en moins de 8 s (`ADMIN_SEQUENCE_MS`) met `store_t.admin` à `STORE_ADMIN_ON` et sélectionne
le thème Admin ; « Quitter le mode admin » le remet à 0.

### Ajouter un jeu

Les jeux ([games.h](../../src/menu/games.h)) ne dépendent que de `gfx`. Un jeu fournit 5 fonctions :
- `*_buttons()` ;
- `*_task()` ;
- `*_render()` ;
- un démarrage ;
- un état « calme » pour `games_calm()`.

Il les branche dans les `switch` de l'API. Les records sont dans `store_t.game_records` (0xFFFF = aucun).
Ajouter ensuite des tests dans [tests/host/test_games.c](../../src/tests/host/test_games.c).

Les casse-têtes ([puzzles.h](../../src/menu/puzzles.h)) suivent le même principe, avec les *hooks* de `games.h` :
appuis courts pour se déplacer, appuis longs pour agir (aile gauche longue : quitter), une page d'aide au départ.
Chacun devient une application par la macro `PUZZLE_APP` de [puzzles_app.c](../../src/menu/puzzles_app.c) ;
leurs records sont dans `store_t.puzzle_records`, leurs tests dans
[tests/host/test_puzzles.c](../../src/tests/host/test_puzzles.c).


## 6. Mécanismes

### 6.1 Écran e-Paper (SSD1681)

- **Formes d'onde** :
  - `screen_ws_1681_bw` : rafraîchissement complet noir et blanc, environ 2 s ;
  - `screen_ws_1681_4grays` : 4 niveaux de gris, avec les deux RAM ;
  - `screen_ws_10fps`, `20fps`, `30fps` : mode *multiframe* rapide, seules les différences sont redessinées.
- **Orientation** : l'entrée des données décrémente X et Y ; l'octet RAM (x, y) est l'octet (24 − x, 199 − y) de l'image.
  `screen_set_image_position()` gère les fenêtres, y compris les octets partiellement couverts.
- **Nettoyage** : `screen_clean()` efface l'écran avec la forme d'onde complète de l'écran (OTP, commande 0x22 0xF7,
  compensée en température, environ 3 s). Les formes d'onde personnalisées sont trop courtes pour effacer les fantômes
  des rafraîchissements rapides : le fantôme revient un moment après l'image.
- **Veille** : nettoyage en noir puis en blanc, puis l'image **tramée en noir et blanc** (2×2 ordonné) dessinée avec
  la forme d'onde OTP (`screen_show_image_bw_otp()`). Essais sur le badge : avec la forme d'onde 4 gris du projet,
  un gros fantôme revient quelques secondes après l'image (aussi avec EOPT 0x22) ; avec l'OTP, l'image tient.
  La visionneuse d'images garde les 4 gris.
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
- **Réseau des cigales** (`social.c`, voir [docs/idees_reseau_extensions_ctf.md](../idees_reseau_extensions_ctf.md)),
  au-dessus de la couche réseau (§ 6.12) :
  - une balise `NET_BEACON` toutes les 2 s ± 0,5 s à −10 dBm (à −20 dBm, elle n'était entendue qu'à 50 cm environ) ;
  - paquet de 17 octets : `0xC1`, type 1, identifiant (hash FNV de l'identifiant unique), numéro, score, nom (8 caractères) ;
  - une rencontre = RSSI ≥ −65 dBm (`SOCIAL_RSSI_CLOSE`, à calibrer avec de vrais badges) sur 3 balises en 10 s ;
  - +10 points pour un nouveau badge, +1 pour un badge connu, au plus une fois par heure ;
  - les voisins entendus (`social_neighbours()`) servent au radar, aux messages et aux invitations des jeux.

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
- records des jeux ;
- champs ajoutés ensuite : `lamp_percent` (luminosité de la lampe), `badge_type` (`STORE_TYPE_PARTICIPANT`,
  `SPEAKER`, `STAFF`), `admin` (`STORE_ADMIN_ON` = 0xA5 : mode admin), `remote_off` (1 : commandes à distance ignorées),
  `muted` (1 : mode muet), `infection` (état du virus), `crypto_solved` (bit n : défi n résolu),
  `puzzle_records` (records des casse-têtes).

Principe :
- l'écriture a lieu 5 s après la dernière modification, avec `flash_safe_execute` (environ 50 ms, interruptions coupées) ;
- les nouveaux champs sont ajoutés **à la fin** de la structure : dans un secteur écrit par une version précédente,
  ils valent 0xFF, ce que leurs utilisateurs vérifient ;
- `_Static_assert` garantit que la structure tient dans le secteur.

Les cartes de visite ne tiennent pas dans ce secteur : elles sont dans un second stockage, `store_ext_t`, de 8 Ko
(deux secteurs) juste avant le premier (`STORE_OFFSET - 8192`), avec son propre en-tête (magique `"CONT"`, version) :
le masque des champs envoyés, ma carte et les 12 cartes reçues (`STORE_CONTACTS`), chacune de 448 octets à champs fixes.
Il est réinitialisé si l'en-tête ne correspond pas. `store_ext_changed()` l'écrit de la même façon, 5 s plus tard.

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


### 6.11 Scores signés (QR code)

- À la fin d'une partie, `games.c` construit le texte `HIP26:<jeu>:<score>:<id du badge>:<nom>:<signature>` :
  - jeux : `MORPION`, `P4` (victoires-défaites-nuls de la session), `SIMON`, `REFLEX` (moyenne en ms), `SNAKE` ;
  - depuis le menu (appui long sur un jeu, `games_show_record()`), c'est le record : meilleur score, meilleure moyenne,
    ou nombre total de victoires contre la cigale (`12V`) pour le Morpion et le Puissance 4 ;
  - l'identifiant est celui du réseau des cigales (hash de l'identifiant unique du RP2040).
- La signature est un SipHash-2-4 sur 64 bits du texte qui la précède, avec une clé de 128 bits
  ([score_code.c](../../src/menu/score_code.c)). La clé n'est pas écrite en clair : elle est masquée par un flux xorshift,
  reconstruite sur la pile le temps du calcul puis effacée. Quelqu'un qui lit le firmware peut la retrouver : c'est
  une protection contre la triche à la main, et un défi pour les curieux, pas un secret cryptographique fort.
- Le QR code est produit par la bibliothèque [qrcodegen](../../src/qrcode/qrcodegen.h) de Project Nayuki
  (licence MIT, sans allocation), niveau de correction M, version 6 au plus, 4 pixels par module quand il tient.
- `tools/score_check.py` vérifie les textes scannés et fait le classement (meilleur score de chaque badge, un QR code
  scanné deux fois compte une fois). La clé se donne par `--key` ou la variable `BADGE_SCORE_KEY` ;
  `--make-key` génère une nouvelle clé et la table masquée à recopier dans `score_code.c`.


### 6.12 Le réseau radio des badges (`net.c`)

Toutes les fonctions qui parlent aux autres badges partagent le CC1101 par [net.h](../../src/menu/net.h) :
- **Profil** : GFSK 9,99 kbit/s (le préréglage du Flipper), 433,92 MHz, mot de synchronisation 0xC16A, CRC du CC1101.
- **Paquet** : `[0xC1][type][identifiant de l'émetteur, 4 octets little endian][données, 55 octets au plus]`,
  61 octets au plus (la FIFO de réception moins l'octet de longueur et les deux octets d'état).
  L'identifiant est `net_id()`, un hash FNV-1a de l'identifiant unique de la flash.
- **Réception** : le badge écoute en permanence. `net_subscribe(type, gestionnaire)` (un gestionnaire par type) reçoit
  un `net_packet_t` : type, émetteur, données, longueur, RSSI en dBm, instant de réception (fin du paquet).
  Les paquets de ce badge lui-même sont ignorés.
- **Émission** : `net_send(type, données, longueur, drapeaux)` met le paquet dans une file de 8 ; il part quand le canal
  est libre (pas de réception en cours sur GDO0). `false` quand la file est pleine ou les données trop longues.
- **Drapeaux** :

  | Drapeau | Effet |
  |---|---|
  | `NET_QUIET` | −20 dBm : les badges tout proches (quelques mètres) |
  | `NET_MEDIUM` | −10 dBm : quelques dizaines de mètres (balises, messages, jeux, cartes de visite) |
  | `NET_LOUD` | +10 dBm : toute la salle (commandes, votes, image, choeur) |
  | `NET_JITTER` | délai aléatoire (jusqu'à 300 ms) avant l'envoi : quand beaucoup de badges répondent au même paquet |

- **Partage de la radio** : les autres fonctions (message au Flipper, porteuse, récepteur OOK) prennent la radio ;
  le réseau se met en pause (`net_pause()`, `radio_tools_idle()`) et reconfigure la radio ensuite.

| Type | Nom | Module | Données |
|---|---|---|---|
| 0x01 | `NET_BEACON` | `social.c` | numéro, score (2 octets), nom (8) |
| 0x02 | `NET_COMMAND` | `remote.c` | commande, nonce (2) |
| 0x03 | `NET_MESSAGE` | `messages.c` | uid (2), TTL, origine (4), destinataire (4, 0 = tous), nom (8), numéro du message |
| 0x04 | `NET_VOTE_QUESTION` | `vote.c` | session (2), question, ouverte (0 / 1) ; toutes les 3 s tant que la question est ouverte |
| 0x05 | `NET_VOTE_ANSWER` | `vote.c` | session (2), question, réponse ; envoyée 3 fois |
| 0x06 | `NET_GAME` | `duel.c`, `battle.c` | session (2), genre, destinataire (4), manche ou tour, puis selon le genre (§ 6.16) |
| 0x07 | `NET_CONTACT` | `contacts.c` | uid de la carte (2), numéro du morceau, nombre de morceaux, 48 octets au plus |
| 0x08 | `NET_HOTCOLD` | `hotcold.c` | balise chaud - froid, une par seconde |
| 0x09 | `NET_INFECTION` | `infection.c` | génération (0 = patient zéro) ; une « toux » toutes les 4 à 5 s, à −20 dBm |
| 0x0A | `NET_IMAGE` | `image_radio.c` | transfert (2), bloc, 48 octets (§ 6.17) |
| 0x0B | `NET_SONG` | `chorus.c` | morceau, genre, session (2), ms (4), voix (§ 6.15) |
| 0x0F | `NET_PING` | `net.c` | numéro (touche `P`) |

Les messages sont relayés par inondation : chaque badge renvoie une fois un message pas encore vu (origine + uid),
TTL décrémenté, avec `NET_MEDIUM | NET_JITTER`, jusqu'à TTL 0 (3 au départ).

**Tester avec un seul badge** : la touche `L` active le mode *loopback* : chaque paquet émis revient comme s'il était
envoyé par un « jumeau » d'identifiant `net_id() ^ NET_TWIN` (0x00FF00FF). Le badge peut ainsi voter à sa propre
question, recevoir son propre message ou s'infecter lui-même. Le réseau des cigales ignore ce jumeau (pas de rencontre).
`V` trace chaque paquet émis et reçu, `P` envoie un ping, `!` affiche les compteurs (émis, reçus, perdus).


### 6.13 Commandes à distance et mode muet (`remote.c`)

Une commande arrive de deux façons :
- d'un **badge admin** : un paquet `NET_COMMAND` `[commande][nonce 2]`, à +10 dBm, envoyé 5 fois en 2 s
  (`remote_send()`) ; la même paire émetteur + nonce n'est exécutée qu'une fois (pendant 10 s) ;
- d'une **télécommande 433 MHz**, un Flipper Zero par exemple : un code Princeton 24 bits `0xC16A00 | commande`,
  décodé par le récepteur OOK (§ 6.14) ; le même code répété (bouton maintenu) n'est exécuté qu'une fois (1,5 s).

| Commande | Effet | Traitée par |
|---|---|---|
| 0x01 | la cigale chante 6 s | `remote.c` |
| 0x02 / 0x03 | mode muet / fin du mode muet | `remote.c` |
| 0x10 à 0x14 | lumières du badge de talk : éteint, vert, orange, rouge, rouge énervé | `talk.c` (page ouverte) |
| 0x20 + n | affiche le talk n du programme | `program.c` |
| 0x30 + n | lance le morceau n du choeur | `chorus.c` |

Un module traite un groupe de commandes (le quartet de poids fort) avec `remote_subscribe(groupe, gestionnaire)` ;
le gestionnaire reçoit le quartet de poids faible. `remote_execute()` exécute une commande locale.
Le badge obéit sauf si Réglages > Télécommande est à « non » (`store_t.remote_off = 1`).

**Écoute des télécommandes** : le CC1101 ne peut pas écouter en GFSK et en OOK à la fois. Quand la radio est libre,
`remote_task()` ouvre une fenêtre d'écoute OOK de 220 ms (deux trames Princeton d'environ 50 ms, quel que soit
le moment du début) toutes les 800 ms. La fenêtre est prolongée par pas de 100 ms, jusqu'à 1,5 s, tant qu'une
télécommande émet (au moins 8 impulsions en 100 ms : les paquets GFSK des badges n'en donnent qu'une ou deux).
Pendant une fenêtre, le réseau n'entend rien : les paquets importants sont répétés. Les fonctions qui ont besoin
de tous les paquets (choeur, image) suspendent les fenêtres avec `remote_pause_windows()` (appels comptés) ;
le badge de talk, lui, écoute la télécommande en permanence.

**Mode muet** (`store_t.muted`) : `audio_set_mute()` continue de jouer les échantillons au niveau 0 (les lecteurs
gardent leur horloge), `set_leds()` et les LEDs des jeux restent éteintes, `app_tone()` et `app_leds()` le respectent.
Une application `owns_leds` (le badge de talk) pilote ses LEDs et son buzzer même en mode muet.

**Depuis un Flipper Zero** : la commande `subghz tx` de la ligne de commande du Flipper ne transmet pas la clé telle
quelle ; utiliser un fichier `.sub` (§ 6.19) ou Sub-GHz > Add Manually > Princeton_433 puis modifier la ligne `Key:`.


### 6.14 Récepteur OOK et décodeurs 433 MHz

- [ook_rx.c](../../src/menu/ook_rx.c) met le CC1101 en réception OOK asynchrone (registres du préréglage « AM650 »
  du Flipper, bande de 650 kHz) : le signal démodulé sort sur GDO0. Une interruption (`gpio_add_raw_irq_handler()`)
  mesure les durées entre les fronts dans un anneau de 1024 ; la boucle principale découpe les transmissions
  (50 ms de silence) et les décode avec `ookdec_decode()`.
- Plusieurs utilisateurs peuvent écouter en même temps (`ook_rx_start()` / `ook_rx_stop()`, comptés) : les fenêtres
  des télécommandes, les pages Décodeur et Station météo, le badge de talk. Le réseau est en pause pendant l'écoute.
  Les codes Princeton sont transmis à `remote_princeton()`, les pages lisent la dernière trame avec `ook_rx_get()`.
- [ookdec.c](../../src/radio/ookdec.c) ne dépend d'aucun matériel (testé sur PC) : télécommandes Princeton (PT2262,
  EV1527...), CAME 12/24 bits, Nice FLO 12/24 bits ; thermomètres Nexus-TH, inFactory-TH, ThermoPRO-TX4, GT-WT02,
  LaCrosse TX141TH-Bv2, Acurite 592TXR. Les protocoles sans somme de contrôle doivent être reçus deux fois à l'identique,
  comme le fait le Flipper ; les parasites de moins de 80 µs sont fusionnés. Ambiguïté connue : une trame ThermoPRO
  a une somme GT-WT02 juste environ une fois sur 300.
- [radio433.c](../../src/menu/radio433.c) : les pages Décodeur 433 MHz (8 dernières trames) et Station météo
  (dernière mesure de 4 sondes au plus). Réception seule.
- **Retour au GFSK** : le récepteur OOK modifie FREND0, FREND1 et MDMCFG0, que le préréglage GFSK laisse à leurs valeurs
  de reset. Avec FREND0 = 0x11, les paquets partiraient avec `PATABLE[1]`, sans puissance : la configuration GFSK de
  `radio_tools.c` réécrit ces registres.


### 6.15 Choeur des cigales

Le protocole est détaillé dans [docs/chorus_sync.md](../chorus_sync.md). En bref :
- le chef diffuse « départ dans 2 000 ms » (`NET_SONG`, genre 1) trois fois, en recalculant le délai restant :
  chaque badge planifie le départ à « réception + délai » (principe RBS : tous reçoivent le même paquet au même instant) ;
- pendant le morceau, le chef diffuse sa position toutes les 2 s (genre 2) : un badge qui arrive en retard rejoint le choeur
  à cette position ; à la fin, un arrêt (genre 3) ;
- le chef chante la voix 1, les autres se répartissent les autres voix d'après leur identifiant ;
- les morceaux sont dans le firmware (domaine public) ; les notes sont synthétisées en signaux carrés à 16 kHz,
  juste à temps (100 ms d'avance) ;
- deux départs en concurrence : le plus proche gagne, à égalité le plus petit identifiant de chef.


### 6.16 Jeux à deux badges : pas de triche

Duel ([duel.c](../../src/menu/duel.c)) et bataille navale ([battle.c](../../src/menu/battle.c)) partagent le type
`NET_GAME` : `[session 2][genre][destinataire 4][manche ou tour][...]`, genres 1 à 5 pour le duel, 10 et plus pour
la bataille (`duel.c` les passe à `battle_handle()`). On invite un voisin (liste des balises entendues) ;
le message en cours est renvoyé toutes les 500 ms jusqu'à ce que l'autre badge passe à la suite,
et la partie est perdue sans nouvelles pendant 20 s (duel) ou 60 s (bataille).

- **Duel : engagement puis révélation** (*commit / reveal*). À chaque manche, un badge envoie d'abord l'engagement
  `[hash 4]` : un FNV-1a 32 bits du choix, de la manche, de la session, d'un nombre aléatoire de 32 bits et de son
  identifiant. Il ne révèle `[choix][nonce 4]` qu'après avoir reçu l'engagement de l'autre, qui vérifie la révélation.
  Personne ne peut donc attendre le choix de l'autre.
- **Bataille navale : hash de la flotte**. Au départ, chaque badge envoie le hash de sa flotte (36 bits, un par case)
  avec un nombre aléatoire et son identifiant. Pendant la partie, il répond « touché » ou « à l'eau » à chaque tir.
  À la fin, il révèle sa flotte et le nombre aléatoire (`[flotte 5][nonce 4]`, répétés pendant 10 s) ; l'autre vérifie
  le hash, les 7 cases de bateaux et chacune des réponses reçues : « Flotte OK » ou « TRICHE ! ».

Un hash de 32 bits protège de la triche improvisée ; ce n'est pas une preuve cryptographique.


### 6.17 Image par radio

[image_radio.c](../../src/menu/image_radio.c) envoie une image de la galerie (`.EPI` du dossier `IMAGES`)
ou l'image SecSea intégrée :
- l'image est tramée en noir et blanc, 1 bit par pixel : 5000 octets (1 = blanc) ;
- découpée en 105 blocs de 48 octets (le dernier complété), plus 14 blocs de parité : le XOR de chaque groupe de 8 blocs.
  Un bloc perdu dans un groupe est reconstitué à partir des 7 autres et de la parité, sans rien redemander ;
- paquet `NET_IMAGE` `[transfert 2][bloc][48 octets]`, blocs 0 à 104 pour l'image, 105 à 118 pour les parités ;
  un bloc toutes les 40 ms à +10 dBm, et toute la séquence **deux fois** (un bloc manqué au premier passage arrive au second) ;
- le récepteur affiche l'image au fur et à mesure, les blocs manquants en gris. Les fenêtres des télécommandes sont
  suspendues pendant l'envoi et la réception. Entre deux badges : complète en 12 s environ.


### 6.18 Défis crypto

[crypto_ctf.c](../../src/menu/crypto_ctf.c) contient 13 défis, du plus simple au plus difficile. Leurs textes sont
générés par `tools/crypto_ctf_make.py`, **le fichier des solutions** (réponses en clair : ne pas le distribuer aux
joueurs), qui calcule les chiffrés, vérifie que tout tient à l'écran (7 lignes de texte, 3 d'indice)
et remplace la table entre les marqueurs `GENERATED` (`--update` ; `--answers` affiche les réponses).
Le firmware ne garde que le SipHash-2-4 des réponses normalisées (majuscules, espaces) et les morceaux du flag final,
masqués par un flux SipHash. La progression est dans `store_t.crypto_solved`.
Le défi « Ultrason » (`ultrasound` dans la table) joue son Morse à 19 kHz avec `audio_pwm_tone()` : un signal
carré sorti directement du PWM (le lecteur audio à 16 kHz ne peut pas dépasser 8 kHz), sans les LEDs.


### 6.19 Flipper Zero

- **Télécommandes et sondes de test** : `tools/ook_sub.py` écrit des fichiers Sub-GHz RAW (`.sub`, préréglage
  `FuriHalSubGhzPresetOok650Async`) avec les mêmes chronogrammes que `ookdec.c` :

  ```bash
  python tools/ook_sub.py princeton 0xC16A02 -o muet.sub      # commande 0x02 (mode muet)
  python tools/ook_sub.py came 0x5A1 --bits 24
  python tools/ook_sub.py weather --temp 21.5 --hum 45 --channel 2 -o meteo   # un fichier par protocole
  python tools/ook_sub.py check fichier.sub                   # vérifie le format
  ```

- **Paquets du réseau des cigales** : `tools/flipper_net_sub.py` écrit des fichiers RAW avec un préréglage GFSK
  personnalisé (`FuriHalSubGhzPresetCustom` et sa ligne `Custom_preset_data`). Le script calcule les bits comme les
  envoie le CC1101 : 4 octets de préambule 0xAA, le mot de synchronisation 0xC1 0x6A, la longueur, le paquet, puis le
  CRC-16 du CC1101 ; le Flipper les émet en GFSK asynchrone.

  ```bash
  python tools/flipper_net_sub.py command 0x02 -o muet_gfsk.sub    # commande à distance (NET_COMMAND)
  python tools/flipper_net_sub.py ping -o ping.sub
  python tools/flipper_net_sub.py raw 0x0F 01 -o paquet.sub        # type, puis les octets de données en hexadécimal
  ```

- **Enregistrer les paquets d'un badge avec le Flipper** : le Flipper ne connaît pas le profil GFSK des badges.
  `python tools/flipper_net_sub.py preset` affiche le préréglage « SecSea » (la même chaîne `Custom_preset_data`)
  à ajouter au fichier `subghz/assets/setting_user` de la carte SD du Flipper ; Read RAW avec ce préréglage enregistre
  alors les paquets (un envoi d'image par exemple), qui peuvent être rejoués.
- **Envoyer un fichier depuis le PC** par la ligne de commande du Flipper (port série USB) :
  `storage write_chunk /ext/subghz/<fichier>.sub <taille>` puis les octets du fichier, et
  `subghz tx_from_file /ext/subghz/<fichier>.sub 1 0` (1 répétition, radio interne).
  Ne pas utiliser `subghz tx` pour les codes Princeton : la clé n'est pas transmise telle quelle.


## 7. Formats de fichiers

| Format | Contenu |
|---|---|
| `.EPI` (image) | en-tête de 16 octets : `EPIMAGE1`, largeur, hauteur, bits par pixel (1 ou 2), 0 (uint16 little endian) ; puis le plan « lsb », puis le plan « msb » (si 2 bits). Chaque plan fait 5000 octets, bit 7 = pixel de gauche, ligne par ligne. Gris = msb×2 + lsb (0 = noir, 3 = blanc). |
| `.EPV` (vidéo) | en-tête de 512 octets : `EPVIDEO2`, largeur, hauteur, fps, bits par pixel, nombre d'images, fréquence audio, nombre d'échantillons ; puis les images de 5000 octets (1 = blanc) ; puis le son, 8 bits non signé, synchronisé sur la première image. `EPVIDEO1` = sans son. |
| `.WAV` | PCM 8 ou 16 bits, mono ou stéréo ; 8 bits 16 kHz mono conseillé (`audio2wav.py`). |
| `.TXT` | UTF-8 (avec ou sans BOM) ou Windows-1252, détecté automatiquement. |
| `.sub` (Flipper Zero) | fichier Sub-GHz RAW : lignes d'en-tête (`Frequency`, `Preset`, éventuellement `Custom_preset_data`), puis des lignes `RAW_Data:` de 512 durées au plus, en µs, alternativement positives (porteuse) et négatives (silence). Écrit par `ook_sub.py` et `flipper_net_sub.py` (§ 6.19). |
| `.vcf` (vCard 3.0) | cartes de visite exportées par la touche `k` et `tools/contacts_export.py`. |

Les images intégrées au firmware (démo écran, logos) sont converties à la compilation par `image2epaper.py`
(fonction CMake `badge_image2epaper`), en tableaux C.


## 8. Le protocole USB série

Le badge est un port série USB (115200 bauds, sans importance en USB). Une touche par caractère :

| Touche | Action |
|---|---|
| `a` `b` `x` `y` | appui court : aile G, aile D, flanc D, flanc G |
| `A` `B` `X` `Y` | appui long (quitter un casse-tête, enregistrer le nom, avance / recul de la lecture rapide...) |
| `[` / `]` | envoi de l'écran à chaque changement : marche / arrêt |
| `s` | envoi de l'écran une fois |
| `!` | diagnostic : OLED, IR, réseau des cigales et voisins, CTF, compteurs du réseau (émis, reçus, perdus, loopback), télécommande (activée, muet, mode admin), radio (version, quartz), batterie |
| `?` | diagnostic du son |
| `i` | test IR : décodage d'une trame NEC puis émission (environ 68 ms) |
| `o` | état du récepteur OOK : actif, impulsions, trames, RSSI, MARCSTATE, GDO0 |
| `p` | durées du dernier signal donné au décodeur OOK |
| `k` | export des cartes de visite reçues en vCard (lu par `tools/contacts_export.py`) |
| `V` | trace de chaque paquet réseau émis et reçu (avec l'écart de fréquence mesuré, FREQEST) : marche / arrêt |
| `r` | registres du CC1101 et PATABLE |
| `M` | envoie le « Radio : message » (profil chat du Flipper : `subghz chat 433920000 0` le reçoit) |
| `P` | ping à +10 dBm : les badges qui l'entendent écrivent `net: ping #n from <id>, rssi ...` |
| `L` | mode *loopback* (§ 6.12) : marche / arrêt |
| `R` | redémarrage (watchdog) |

Le badge envoie des lignes de texte :
- `ui: <titre>` à chaque changement de page (le nom de l'application à son ouverture) ;
- `browser:`, `image:`, `saver: on/off`, `radio:`, `social:`, `game:`, `ctf: code right/wrong`, `credits:`, `name:`...
- `notify:` (notification), `net:`, `remote:`, `admin:`, `ook:`, `talk:`, `vote:`, `message:`, `program:`,
  `infection:`, `hotcold:`, `contacts:`, `chorus:`, `duel:`, `battle:`, `crypto:`, `store:`...

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
| `puzzles` | Démineur, 2048, Taquin, Sokoban, Mastermind, Pendu : règles, fins de partie, records, textes qui tiennent à l'écran |
| `score` | SipHash (vecteurs de référence), texte et signature du score, dessin du QR code |
| `crypto` | textes qui tiennent à l'écran, chaque réponse pour son seul défi, normalisation, mauvaises réponses, morceaux et flag final |
| `ookdec` | télécommandes (Princeton, CAME, Nice FLO) et sondes (Nexus-TH, ThermoPRO-TX4, GT-WT02, inFactory, LaCrosse, Acurite) avec gigue, parasites, horloge ±20 %, répétition exigée sans somme de contrôle ; le bruit ne décode rien |
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
2. Il simule les boutons et vérifie les traces : menus, jeux, veille, crédits, radio, balises, IR, images de la carte SD,
   applications, mode admin, récepteur OOK, fonctions sociales.
3. Il écrit un tableau PASS / FAIL / SKIP et des captures d'écran PNG (dossier `selftest_<date>`).

Un test est SKIP quand le matériel manque (pas de carte SD, batterie non calibrée).
Aucun réglage du badge n'est modifié, sauf avec `--ctf` (et les records des jeux, une partie pouvant finir à 0 point).
Les groupes `admin` et `net` activent le mode admin, le mode muet ou le virus le temps du test, puis les désactivent.

Les groupes (`--only`) :

| Groupe | Vérifie |
|---|---|
| `diag` | diagnostic `!` : version du CC1101, quartz, batterie |
| `menus` | chaque thème s'ouvre, retour aux thèmes |
| `games` | mini-jeux, QR code du score, record (appui long) |
| `puzzles` | chaque casse-tête : page d'aide, départ, un coup, sortie par l'appui long sur l'aile gauche |
| `ctf` | code Konami (seulement avec `--ctf`) |
| `settings` | aperçu et réveil de la veille, crédits |
| `radio` | message radio, mode test, balises du réseau des cigales |
| `ir` | décodage et émission d'une trame NEC |
| `images` | images de la carte SD |
| `apps` | badge nominatif et lampe |
| `admin` | séquence secrète, commandes mode muet / fin du mode muet, sortie du mode admin |
| `radio433` | pages Décodeur 433 MHz et Station météo, puis retour des balises du réseau |
| `social` | pages du thème Social (contacts, radar, chaud - froid...) et états du badge de talk |
| `net` | avec un seul badge, en *loopback* : vote, annonce d'un talk, virus, message |

### 9.3 Applications de test par module

`src/tests/*.c` : une application par module (écran, radio, LEDs, son...), à flasher pour explorer un périphérique.


## 10. Outils

| Script | Usage |
|---|---|
| `tools/badge_remote.py` | fenêtre avec l'écran du badge en grand (zoom 2–4), flèches / Entrée / Échap = boutons, capture PNG ; `--snapshot fichier.png` pour une capture seule |
| `tools/badge_selftest.py` | test automatique du badge (§ 9.2) |
| `tools/score_check.py` | vérifie les QR codes de score et fait le classement (§ 6.11) |
| `tools/contacts_export.py` | cartes de visite reçues par le badge → `.vcf` (`--port`, `-o`) |
| `tools/crypto_ctf_make.py` | génère la table des défis crypto (`--update`, `--answers`) : fichier des solutions (§ 6.18) |
| `tools/ook_sub.py` | `.sub` du Flipper : Princeton, CAME, Nice FLO, sondes météo ; `check`, `selftest` (§ 6.19) |
| `tools/flipper_net_sub.py` | `.sub` du Flipper pour le réseau des cigales (`command`, `ping`, `raw`) et préréglage « SecSea » (`preset`) (§ 6.19) |
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
| Tester une fonction entre badges avec un seul badge | touche `L` (*loopback*, § 6.12), et `V` pour voir les paquets |
| Un paquet n'arrive pas | `V` sur les deux badges ; compteur « dropped » de `!` (file pleine, données trop longues) ; pendant une fenêtre d'écoute OOK, le réseau n'entend rien : répéter les paquets importants |
| Plus aucun badge n'entend ce badge après une écoute OOK | FREND0 resté à 0x11 : la configuration GFSK doit réécrire FREND0, FREND1, MDMCFG0 (§ 6.14) |
| Le Flipper envoie une autre clé Princeton | `subghz tx` en ligne de commande : utiliser un `.sub` et `subghz tx_from_file` (§ 6.19) |
| Le Flipper n'enregistre rien des badges | les badges émettent en GFSK (modulation de fréquence) : Read RAW en AM650 / AM270 (par défaut) ne les voit pas. Utiliser le préréglage « SecSea » ajouté à `subghz/assets/setting_user` (§ 6.19), ou à défaut FM476. Vérifier l'émission : `subghz chat 433920000 0` sur le Flipper et `M` sur le badge |
