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
  car la mesure varie d'environ 500 ppm. Le réglage automatique de la radio (§ 6.21) corrige ensuite le petit écart
  de fréquence entre les badges.
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

**Version du firmware** : `BADGE_VERSION` dans [src/menu/CMakeLists.txt](../../src/menu/CMakeLists.txt), à augmenter
pour chaque version installée sur les badges. À chaque compilation, [version.cmake](../../src/menu/version.cmake)
écrit `version.h` (dans le dossier de compilation) avec ce numéro, le commit git court et la date ; un « + » après le
commit signale des sources de `src/` ou `tools/` modifiées depuis (`sans-git` si git ne répond pas). Le badge
l'affiche dans Réglages > Infos (« Version 1.0.0 (5d95da4) » et « Compilée le 2026-09-30 »), au démarrage et avec `!`
sur le port série (`version: 1.0.0 (5d95da4 2026-09-30)`).


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
├── badge_screens.py    captures de tous les écrans et vérification des textes (docs/screens, ecrans.md)
├── badge_media_test.py lit toutes les vidéos et musiques de la carte SD, rapport media_report.txt
├── score_check.py      vérification des QR codes de score, classement
├── contacts_export.py  cartes de visite reçues par le badge → fichier vCard
├── crypto_ctf_make.py  génère les défis crypto (fichier des solutions : spoilers)
├── ook_sub.py          fichiers .sub du Flipper Zero : télécommandes et sondes météo de test
├── flipper_net_sub.py  fichiers .sub du Flipper Zero : paquets du réseau des cigales, préréglage « SecSea »
├── skills_icons.py     pictogrammes des compétences → src/menu/skills_icons.h
├── smuggler_icons.py   icônes des marchandises de la contrebande → src/menu/smuggler_goods.c
├── gamebook_check.py   vérifie un livre-jeu, y joue dans le terminal, génère le livre intégré
├── test_party_games.py test à deux badges : tir à la corde et assassin
├── test_werewolf.py    test à deux badges : loup-garou (robots en mode admin)
├── test_smuggler.py    test à deux badges : contrebande
└── flipper/            télécommandes du Flipper : SecSea_general.sub, SecSea_talk.sub (§ 6.13)
docs/           documentation, idées, PVSR, synchronisation du choeur (chorus_sync.md),
                écrans du badge (fr/ecrans.md, en/screens.md et screens/, générés par badge_screens.py),
                fonctions détaillées (fr/contrebande.md, fr/loup_garou.md, fr/livres_jeux.md, fr/radio_pirate.md,
                fr/sonneries.md et leurs traductions dans en/), exemples pour la carte SD (sd/LIVRES, sd/SONNERIES)
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
    social_task(now); party_task(now); smuggler_task(now); tug_service(now); ... battery_task(now);
    achv_task(); store_task(now); ...
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
| `radio_tools.c` | message et porteuse radio, profil GFSK, mesure du quartz, correction de fréquence |
| `radio_tune.c` | réglage automatique de la radio : quartz, bruit, fréquence (§ 6.21) |
| `messages.c`, `contacts.c`, `vcard.c`, `program.c`, `vote.c` | messages relayés, cartes de visite et leur format vCard (§ 6.20), programme, votes |
| `hotcold.c`, `infection.c`, `chorus.c` | chasse chaud - froid, radar et Chasse 433 MHz (§ 6.14), virus des cigales, choeur (§ 6.15) |
| `ledcast.c`, `announce.c` | LEDs des cigales pilotées par un badge admin (§ 6.22), annonces (§ 6.23) |
| `duel.c`, `battle.c` | pierre-feuille-ciseaux et bataille navale entre deux badges (§ 6.16) |
| `image_radio.c` | envoi et réception d'une image par radio (§ 6.17) |
| `crypto_ctf.c`, `crypto_app.c` | défis de cryptographie et leurs pages (§ 6.18) |
| `lamp.c`, `nametag.c`, `talk.c`, `admin.c` | lampe, badge nominatif, badge de talk, commandes radio et type du badge (admin) |
| `skills.c`, `skills_icons.h` | compétences et leurs pictogrammes (§ 6.24) |
| `achievements.c` | succès, XP et niveau de la cigale (§ 6.24) |
| `party.c` | salon des jeux de groupe (§ 6.25) |
| `tug.c`, `tug_logic.c` | tir à la corde (§ 6.25) |
| `assassin.c`, `assassin_logic.c` | assassin (§ 6.25) |
| `werewolf.c`, `werewolf_logic.c`, `werewolf_cards.c` | loup-garou, les cartes de l'aide dessinées dans `tools/werewolf_icons.py` (§ 6.25, [loup_garou.md](loup_garou.md)) |
| `smuggler.c`, `smuggler_goods.c`, `smuggler_trade.c` | la cigale contrebandière (§ 6.26, [contrebande.md](contrebande.md)) |
| `pirate_radio.c` | radio pirate : émission FM et écoute (§ 6.27, [radio_pirate.md](radio_pirate.md)) |
| `rtttl.c`, `rtttl_parse.c` | sonneries RTTTL (§ 6.28, [sonneries.md](sonneries.md)) |
| `gamebook.c`, `gamebook_parse.c`, `gamebook_builtin.c` | livres-jeux (§ 6.28, [livres_jeux.md](livres_jeux.md)) |
| `demo.c` | page du mode démo (admin), la démo elle-même est dans `main.c` (§ 6.29) |
| `reset.c` | remise à zéro des scores et de la progression (admin) |
| `credits.c` | pages des crédits |
| `score_code.c` | scores signés affichés en QR code (§ 6.11) |
| `battery.c`, `battcal.c` | niveau de batterie, page de calibration (admin) |
| `store.c` | réglages, scores, progression et cartes de visite en flash, réglages usine |
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
`app_show_still(render)` quitte l'application et affiche la page dessinée par `render` comme la veille : forme d'onde
OTP de l'écran (sans fantôme), gardée sans courant ; n'importe quel bouton ramène au menu. Elle s'appelle depuis
`start()` (badge nominatif) ou depuis `task()` (fin de la réception d'une image).

Pour ajouter une application :
1. écrire `foo.c` avec `const app_t app_foo = {...}` ;
2. ajouter `APP_FOO` à `app_id_t` dans [apps.h](../../src/menu/apps.h), et `[APP_FOO] = &app_foo` dans `APPS[]`
   ([apps.c](../../src/menu/apps.c)) ;
3. placer `M_APP(APP_FOO)` dans un thème de `SUBMENUS` (`main.c`) ;
4. ajouter `foo.c` à [src/menu/CMakeLists.txt](../../src/menu/CMakeLists.txt).

Un **service** (réception radio en tâche de fond) s'abonne à son type de paquet dans une fonction `*_init()` appelée
par `main()`, et signale un événement par une fonction interrogée dans la boucle (`vote_new()`, `duel_invited()`...) :
`notify()` émet un bip, écrit le texte en bas de l'écran et ouvre l'application si l'on est dans les menus ou la veille.

**Succès** : une nouvelle fonction peut donner un succès ([achievements.h](../../src/menu/achievements.h)) :
1. ajouter un `ACHV_*` à `achv_id_t` et sa ligne dans `ACHV[]` de `achievements.c` : nom, comment l'obtenir
   (affiché par la page), XP. Platine (`ACHV_ALL`) demande tous les succès placés **avant** lui ;
2. appeler `achv_unlock(ACHV_FOO)` quand il est atteint : une seule fois, enregistré, annoncé en bas de l'écran
   (« Succès : ... » ou « Niveau n : ... ! ») avec un carillon ; ou `achv_add(ACHV_CNT_FOO, n)` pour un compteur
   (`achv_counter_t`, 16 au plus, `store_t.achv_counters` : affaires, victimes, parties de groupe, victoires, fins de
   livres, sonneries). `achv_add()` ne fait que compter : aucun succès n'a encore de seuil sur un compteur, il faut
   le tester après l'appel (`if (achv_add(...) >= 10) achv_unlock(...)`) ;
3. pour un succès qui découle de l'état du badge (nombre de rencontres...), le vérifier dans `achv_task()`, appelée
   chaque seconde par la boucle.

Les bits de `store_t.achievements` sont l'index dans `achv_id_t` (64 au plus) : une fois les badges distribués, ne
jamais réordonner ni retirer un succès. Un succès inséré avant `ACHV_ALL` prend le bit de Platine (un badge qui
l'avait obtenu aurait alors le nouveau succès) : à faire avant la distribution, ou en ajoutant le succès après
`ACHV_ALL` (Platine ne le demande pas).

### Les aides de dessin et l'éditeur de texte (`ui.h`)

[ui.h](../../src/menu/ui.h) donne l'aspect commun des pages : `ui_title()` (bandeau noir), `ui_footer()`,
`ui_list()` (liste avec défilement), `ui_lines()` / `ui_text()` (lignes centrées ou à gauche), `ui_fit()`
(texte tronqué avec « ... »), `ui_wrapped()` (texte centré, coupé aux espaces pour tenir dans la largeur, sur
`max_lines` lignes au plus ; utilisé par les annonces), `ui_box()`, `ui_gauge()`.

**Vérification des textes** (touche `U` du port série, `ui_check`) : `ui_title()`, `ui_footer()`, `ui_fit()`,
`ui_lines()`, `ui_text()`, `ui_wrapped()` et les pages de `main.c` tracent les textes coupés
(`uicheck: cut "..."`), trop larges (`uicheck: title too wide (...)`, `footer`, `wrapped text cut`) ou dessinés sous
le pied de page (`uicheck: under the footer (y ...)`). `tools/badge_screens.py` l'active en parcourant toutes les pages.

L'éditeur de texte à 4 boutons (`ui_edit_t`, 56 caractères au plus, `UI_EDIT_MAX`) sert aux cartes de visite, aux défis crypto
et au remède du virus :
- `ui_edit_start(e, texte, longueur, jeu_de_caractères)` : `UI_CHARSET_TEXT` (lettres, chiffres, ponctuation
  pour les noms, e-mails, URL), `UI_CHARSET_PHONE` (chiffres, +, espace), `UI_CHARSET_UPPER` (A-Z, 0-9, espace),
  `UI_CHARSET_LONG` (lettres avec les accents français é è ê à â ç ô î ù û ë ï É È À Ç, chiffres, plus de ponctuation :
  texte et contenu du QR code des annonces). Dans l'éditeur, une lettre accentuée occupe une case (un octet,
  0x80 + son rang dans `ACCENTS`) ; `ui_edit_start()` la lit en UTF-8 et `ui_edit_result()` la rend en UTF-8
  (2 octets) : la destination doit avoir la place ;
- flancs : `ui_edit_change(e, -1 / +1)` (la répétition en maintenant est à la charge de l'appelant) ;
- aile droite (appui court) : `ui_edit_move(e, 1)` ; aile gauche : `ui_edit_move(e, -1)`, qui renvoie `false`
  depuis la première position (annuler) ;
- aile droite (appui long) : terminé, `ui_edit_result()` rend le texte sans les espaces de fin ;
- `ui_edit_render()` dessine la question, le texte autour du curseur et le mode d'emploi ;
- le clavier du PC (mode clavier de `badge_remote.py` : octet 0x02 suivi du caractère sur l'USB) **insère** le
  caractère au curseur de l'éditeur ouvert (la suite se décale, le dernier caractère est perdu si le texte est plein),
  par `ui_edit_apply_typed()` (Entrée : terminé ; Échap : annuler ; retour arrière : effacer). Les boutons, eux,
  changent le caractère sous le curseur. Seul l'ASCII passe (0x20 à 0x7E) : pas de lettre accentuée par le clavier.

### Ajouter une entrée de menu

Pour une entrée gérée directement par `main.c` (les fonctions historiques) :
1. Ajouter une valeur à `menu_item_t` et son libellé dans `item_label()`.
2. La placer dans un thème de `SUBMENUS` (24 entrées au plus par thème, `items[24]` de `submenu_t` ; le nombre
   d'entrées `n` est écrit à la main : Médias 7, Jeux 19, Social 12, Radio & IR 9, Badge 8, Réglages 6, Admin 14).
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
le thème Admin ; « Quitter le mode admin » le remet à 0. Sur le port série, l'octet 0x01 suivi de `A` (marche) ou `a`
(arrêt) fait de même (case « Mode admin » de `badge_remote.py`). Les trois passent par `set_admin()` (`main.c`),
qui trace `admin: on` / `admin: off` et redessine le menu (le thème Admin disparaît s'il était ouvert).

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
  - suspend ce rafraîchissement complet pendant un jeu en action (`display_set_periodic_full(false)`) ;
  - **nettoie une page qui reste** comme l'écran de veille : après 15 s sans changement (`DISPLAY_SETTLE_MS`),
    rafraîchissements complets avec la forme d'onde de l'écran (OTP) en noir, en blanc, puis la page (~9 s, une
    fois) ; les formes d'onde courtes laissent des fantômes qui reviennent. Une page faite pour rester l'appelle plus
    tôt (1,5 s) avec `display_settle_soon()` avant de se dessiner : la page qui suit l'écran de veille ou une page
    fixe (annonce, badge nominatif), le détail d'un talk avec son QR code, l'aperçu d'une annonce. Une nouvelle
    image, ou `display_is_idle()` (quelqu'un veut l'écran), arrête le nettoyage après son étape en cours (~3 s au
    plus). Les pages qui changent sans cesse (radar, jeux, vidéo) ne sont jamais nettoyées. Trace :
    `display: cleaning the page that stays` / `page cleaned` / `cleaning stopped`.

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
- `wav.c` lit les WAV PCM 8, 16, 24 et 32 bits, flottants 32 bits (format 3) et `WAVE_FORMAT_EXTENSIBLE`, mono ou
  stéréo, de 4 à 192 kHz ; au-dessus de 48 kHz, un échantillon sur 2 (96 kHz), 3 ou 4 est joué (décimation).
  La vidéo utilise le son comme horloge.
- La sortie peut aussi piloter la radio (`AUDIO_OUT_RADIO`) : c'est la modulation de la radio pirate (§ 6.27).
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
  - une balise `NET_BEACON` toutes les 2 s ± 0,5 s à +10 dBm (mesuré entre deux badges à 1 m environ : +10 dBm
    arrive vers −70 à −83 dBm, −10 dBm vers −97 dBm, à la limite de la sensibilité, −20 dBm n'arrive pas) ;
  - paquet de 22 octets : `0xC1`, type 1, identifiant (hash FNV de l'identifiant unique), numéro, score, nom
    (8 caractères), puis les compétences (masque de 4 octets, § 6.24) et le niveau (1 à 10) ; un badge d'un firmware
    plus ancien envoie 17 octets (`BEACON_LEN`), lus sans compétences ni niveau ;
  - une rencontre = RSSI ≥ −80 dBm (`SOCIAL_RSSI_CLOSE`, provisoire : à calibrer sur place avec le radar)
    sur 3 balises en 10 s ;
  - +10 points pour un nouveau badge, +1 pour un badge connu, au plus une fois par heure ;
  - les voisins entendus (`social_neighbours()` : nom, RSSI, déjà rencontré, compétences, niveau) servent au radar,
    aux messages, aux invitations des jeux, aux compétences, à la jauge de l'assassin et à la contrebande ;
  - un voisin proche (même seuil) qui partage une compétence est annoncé une fois par visite
    (`social_event()` : « X aime aussi : ... »).

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
  `puzzle_records` (records des casse-têtes), `radio_tuned` (`STORE_RADIO_TUNED` = 0xA5 : radio réglée),
  `radio_noise_dbm` (bruit mesuré, dBm), `radio_freq_offset` (correction de fréquence, FSCTRL0) (§ 6.21) ;
- la **seconde génération** de champs, valide quand `v2_magic` vaut `STORE_V2_MAGIC` (0x5A) : `skills` (bit n :
  compétence n cochée), `achievements` (64 bits, bit n : succès n), `achv_counters[16]` (compteurs des succès),
  `book_hash` et `book_section` (livre-jeu en cours et sa section), `cargo[32]` (la cale de la contrebande) et
  `cargo_seeded`. Quand `v2_magic` est différent (secteur d'une version précédente : 0xFF), `store_init()` met ces
  champs à 0 et écrit `v2_magic` : sans cela, une flash effacée (0xFF) voudrait dire « tous les succès ».

Principe :
- l'écriture a lieu 5 s après la dernière modification (`store_changed()`), avec `flash_safe_execute` (environ 50 ms,
  interruptions coupées) ;
- `store_save_now()` écrit tout de suite : pour un changement qui ne doit pas être perdu si le badge est éteint dans
  les 5 s (la cale après un échange de contrebande : sinon une marchandise pourrait être dupliquée) ;
- les nouveaux champs sont ajoutés **à la fin** de la structure : dans un secteur écrit par une version précédente,
  ils valent 0xFF, ce que leurs utilisateurs vérifient (ou, pour un groupe de champs, un octet magique comme
  `v2_magic`) ;
- `_Static_assert` garantit que la structure tient dans le secteur.

Les cartes de visite ne tiennent pas dans ce secteur : elles sont dans un second stockage, `store_ext_t`, de 8 Ko
(deux secteurs) juste avant le premier (`STORE_OFFSET - 8192`), avec son propre en-tête (magique `"CONT"`, version) :
le masque des champs envoyés, ma carte et les 12 cartes reçues (`STORE_CONTACTS`), chacune de 512 octets à champs fixes
(`CONTACT_BYTES`). Il est réinitialisé si l'en-tête ne correspond pas : le passage aux cartes de 512 octets
(`STORE_EXT_VERSION` 2) efface donc, à la mise à jour, les cartes reçues **et** ma carte (le masque revient à
prénom + nom). `store_ext_changed()` l'écrit de la même façon, 5 s plus tard.
Les annonces de l'admin (§ 6.23) ont été ajoutées ensuite à la fin de `store_ext_t`, sans changer la version :
`announce_magic` puis 6 `store_announce_t` (`STORE_ANNOUNCES` : heure 6 octets, texte 112, type du QR code, contenu
64), valides quand `announce_magic` vaut `STORE_ANNOUNCE_MAGIC` (`"ANNO"`) ; sinon `announce_list()` y met les
annonces par défaut. Puis `contact_skills[12]` : les compétences de chaque carte reçue (§ 6.20), déplacées avec les
cartes.

Le troisième stockage, les **réglages usine** (`store_factory_t`, magique `"FACT"`), a son propre secteur juste avant
`store_ext_t` : jamais effacé par la Remise à zéro, un changement de version ou un nouveau firmware ; il garde la
calibration de la batterie (§ 6.9) et s'écrit tout de suite (`store_factory_save()`).

### 6.9 Batterie

- La mesure brute (ADC3, 16 échantillons, filtrée) n'est convertie en tension qu'avec une **calibration en deux points**
  (linéaire entre les points, distants d'au moins `BATTERY_CAL_MIN_RAW` = 150 pas d'ADC).
- Sans calibration, rien n'est affiché : le badge ne doit jamais afficher de valeur fausse.
- Les points se règlent sur le badge : Admin > Batterie (calibration) (battcal.c, voir le guide utilisateur § 5).
  `battery_set_point(mv, raw)` pose le premier point, le second, ou remplace le plus proche.
- Ce sont des **réglages usine** : `store_factory_t` (magic `"FACT"`), dans son propre secteur de flash juste
  avant le second store. Écrit tout de suite (`store_factory_save()`), il n'est jamais effacé par la Remise à zéro,
  un changement de version du store ou un nouveau firmware (picotool n'écrit que les secteurs du programme).
- Repli pour un badge sans points usine : `BATTERY_CAL_RAW1/MV1/RAW2/MV2` à la compilation
  ([battery.h](../../src/menu/battery.h)).
- Le diagnostic `!` affiche la mesure et `battery: factory points <raw> = <mV>, <raw> = <mV>`.

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
  | `NET_QUIET` | −20 dBm : ~−107 dBm à 1 m, sous la sensibilité (inutilisé) |
  | `NET_MEDIUM` | −10 dBm : ~−97 dBm à 1 m, à la limite de la sensibilité (inutilisé) |
  | `NET_LOUD` | +10 dBm : ~−70 à −83 dBm à 1 m ; utilisé par toutes les fonctions (la proximité se juge au RSSI) |
  | `NET_JITTER` | délai aléatoire (jusqu'à 300 ms) avant l'envoi : quand beaucoup de badges répondent au même paquet |

- **Partage de la radio** : les autres fonctions (message au Flipper, porteuse, récepteur OOK) prennent la radio ;
  le réseau se met en pause (`net_pause()`, `radio_tools_idle()`) et reconfigure la radio ensuite.
- **Mode chat** : `net_set_chat(gestionnaire)` passe la radio sur le profil du « SubGHz chat » du Flipper Zero
  (`radio_tools_profile_chat()` : même modem GFSK 9,99 kbit/s, mot de synchronisation 0x464C) ; `net_set_chat(NULL)`
  revient au réseau. Les paquets sont du texte brut, sans en-tête (61 octets au plus) : un Flipper (`subghz chat`)
  ou tout CC1101 les lit et les écrit. `net_send_text()` les envoie à +10 dBm ; le gestionnaire reçoit chaque texte
  avec son RSSI. Dans ce mode, le réseau n'entend rien, `net_send()` refuse les paquets et la file est vidée au
  changement de mode ; `net_chat()` indique le mode, `net_queue_free()` la place libre dans la file.
  Utilisé par l'échange de cartes (§ 6.20).

| Type | Nom | Module | Données |
|---|---|---|---|
| 0x01 | `NET_BEACON` | `social.c` | numéro, score (2 octets), nom (8), compétences (4), niveau |
| 0x02 | `NET_COMMAND` | `remote.c` | commande, nonce (2) |
| 0x03 | `NET_MESSAGE` | `messages.c` | uid (2), TTL, origine (4), destinataire (4, 0 = tous), nom (8), numéro du message |
| 0x04 | `NET_VOTE_QUESTION` | `vote.c` | session (2), question, ouverte (0 / 1) ; toutes les 3 s tant que la question est ouverte |
| 0x05 | `NET_VOTE_ANSWER` | `vote.c` | session (2), question, réponse ; envoyée 3 fois |
| 0x06 | `NET_GAME` | `duel.c`, `battle.c` | session (2), genre, destinataire (4), manche ou tour, puis selon le genre (§ 6.16) |
| 0x07 | `NET_TRADE` | `smuggler.c` | id de l'affaire (4), genre, destinataire (4), données (§ 6.26) ; ancien `NET_CONTACT`, inutilisé depuis que les cartes passent en mode chat (§ 6.20) |
| 0x08 | `NET_HOTCOLD` | `hotcold.c` | balise chaud - froid, une par seconde : [1][dBm du haut de l'échelle, signé] (jauge de -30 dB sous cette valeur à cette valeur, « BRÛLANT » dès 85 %) |
| 0x09 | `NET_INFECTION` | `infection.c` | génération (0 = patient zéro) ; une « toux » toutes les 4 à 5 s, à +10 dBm ; contagion à RSSI ≥ −80 dBm (provisoire) |
| 0x0A | `NET_IMAGE` | `image_radio.c` | transfert (2), bloc, 48 octets (§ 6.17) |
| 0x0B | `NET_SONG` | `chorus.c` | morceau, genre, session (2), ms (4), voix (§ 6.15) |
| 0x0C | `NET_LEDS` | `ledcast.c` | nonce (2), mode, R, G, B, durée 1 (2), durée 2 (2) ; envoyé 5 fois (§ 6.22) |
| 0x0D | `NET_ANNOUNCE` | `announce.c` | nonce (2), morceau, nombre de morceaux, 48 octets au plus ; le tout 3 fois (§ 6.23) |
| 0x0E | `NET_PARTY` | `party.c`, puis `tug.c`, `assassin.c`, `werewolf.c` | jeu, session (2), genre, destinataire (4, 0 = tous), 47 octets au plus (§ 6.25) |
| 0x0F | `NET_PING` | `net.c` | numéro (touche `P`) |

Les messages sont relayés par inondation : chaque badge renvoie une fois un message pas encore vu (origine + uid),
TTL décrémenté, avec `NET_MEDIUM | NET_JITTER`, jusqu'à TTL 0 (3 au départ).

**Tester avec un seul badge** : la touche `L` active le mode *loopback* : chaque paquet émis revient comme s'il était
envoyé par un « jumeau » d'identifiant `net_id() ^ NET_TWIN` (0x00FF00FF). Le badge peut ainsi voter à sa propre
question, recevoir son propre message ou s'infecter lui-même. Le réseau des cigales ignore ce jumeau (pas de rencontre).
`V` trace chaque paquet émis et reçu (en mode chat, le texte des paquets reçus), `P` envoie un ping, `!` affiche
les compteurs (émis, reçus, perdus) et l'état du réseau (`net state: ...` : en pause ou non, propriétaire de la radio,
envoi en cours, mode chat, file, nombre de réparations, MARCSTATE et principaux registres, GDO0).

**Robustesse de la radio** : un badge pouvait devenir sourd et muet (GDO0 resté haut : le réseau attendait la fin
d'un paquet qui ne venait jamais).
- **PKTLEN = 61** : `configure()` de [radio_tools.c](../../src/menu/radio_tools.c) limite la longueur des paquets
  à 61 octets (la FIFO de réception moins l'octet de longueur et les deux octets d'état). Avec la valeur par défaut
  (255), un mot de synchronisation trouvé dans le bruit suivi d'une grande « longueur » laissait la radio attendre
  la fin d'un paquet qui n'existait pas.
- **Surveillance** : chaque seconde (`CHECK_MS`, hors envoi), `check_radio()` vérifie que la radio est dans la
  configuration du réseau : PKTLEN 61, PKTCTRL0 0x05, MDMCFG2 0x12, IOCFG0 0x06, le mot de synchronisation du mode
  (0xC16A, ou 0x464C en mode chat) et MARCSTATE entre IDLE et RX (ni débordement de FIFO, ni émission). GDO0 haut
  plus de 300 ms (`GDO0_STUCK_US` ; un paquet dure 60 ms au plus) est bloqué : la broche redevient une entrée.
  Dans tous ces cas, la radio passe en IDLE, les FIFO sont vidées, le réseau la reconfigure au tour suivant
  et trace `net: radio repaired (...)` ; le compteur `repairs` est dans la ligne `net state:` de `!`.
- **Reconfiguration complète** : quand le réseau reprend la radio (après le récepteur OOK, l'émission OOK, la mesure
  du quartz...), il appelle d'abord `radio_tools_reconfigure()` (tous les registres du profil), puis son profil :
  un autre utilisateur peut avoir laissé d'autres registres (une radio restée en OOK n'envoie rien d'utilisable).
- **Écart de fréquence** : pour chaque paquet reçu d'un autre badge, le réseau additionne le FREQEST du CC1101
  (écart mesuré, pas de fXOSC / 2^14) ; `net_freq_offsets(&somme, reset)` le rend au réglage de la radio (§ 6.21),
  et `net_reconfigure()` fait reconfigurer la radio par le réseau.


### 6.13 Commandes à distance et mode muet (`remote.c`)

Une commande arrive de deux façons :
- d'un **badge admin** : un paquet `NET_COMMAND` `[commande][nonce 2]`, à +10 dBm, envoyé 5 fois en 2 s
  (`remote_send()`) ; la même paire émetteur + nonce n'est exécutée qu'une fois (pendant 10 s) ;
- d'une **télécommande 433 MHz**, un Flipper Zero par exemple : un code Princeton 24 bits `0xC16A00 | commande`,
  décodé par le récepteur OOK (§ 6.14) ; le même code répété (bouton maintenu) n'est exécuté qu'une fois (1,5 s).

Le badge admin envoie d'abord la commande comme une télécommande : 12 trames Princeton (`OOK_FRAMES`, te = 400 µs, ~0,6 s,
[ook_tx.c](../../src/menu/ook_tx.c) : CC1101 en mode série asynchrone, fronts sur GDO0 cadencés par une alarme
matérielle), puis les paquets réseau. Le badge de talk, qui n'écoute qu'en OOK, reçoit ainsi les commandes admin.
Une commande reçue par les deux voies n'est exécutée qu'une fois (même commande dans les 4 s).

| Commande | Effet | Traitée par |
|---|---|---|
| 0x01 | la cigale chante 6 s | `remote.c` |
| 0x02 / 0x03 | mode muet / fin du mode muet | `remote.c` |
| 0x10 à 0x14 | lumières du badge de talk : éteint, vert, orange, rouge, rouge énervé | `talk.c` (page ouverte) |
| 0x30 + n | lance le morceau n du choeur | `chorus.c` |

Un module traite un groupe de commandes (le quartet de poids fort) avec `remote_subscribe(groupe, gestionnaire)` ;
le gestionnaire reçoit le quartet de poids faible. `remote_execute()` exécute une commande locale.
Le badge obéit sauf si Réglages > Télécommande est à « non » (`store_t.remote_off = 1`).

**Écoute des télécommandes** : le CC1101 ne peut pas écouter en GFSK et en OOK à la fois. Il n'y a plus de fenêtre
d'écoute à l'aveugle : le réseau écoute en permanence, et `remote_task()` lit le RSSI toutes les 20 ms (`RSSI_POLL_MS`)
quand la radio et le réseau sont libres. Un émetteur au-dessus du seuil `remote_trigger_dbm()` (le bruit mesuré par
le réglage de la radio + 15 dB, `OOK_TRIGGER_ABOVE_NOISE`, borné entre −95 et −70 dBm ; −90 dBm, `OOK_TRIGGER_DBM`,
sans réglage : le bruit est vers −105 dBm) mesuré deux fois de suite (`OOK_TRIGGER_POLLS`) sans paquet du réseau en cours (pas de mot de synchronisation
reconnu, `net_transmitting()`) est peut-être une télécommande : une fenêtre OOK de 150 ms s'ouvre (`OOK_WINDOW_MS`).
Elle est prolongée par pas de 100 ms, jusqu'à 1,5 s (`OOK_WINDOW_MAX_MS`), tant qu'une télécommande émet (au moins
30 fronts en 100 ms, `OOK_ACTIVE_PULSES` : une trame Princeton en donne environ 95, le bruit et les paquets GFSK des
badges bien moins). Après une fenêtre, la suivante attend au moins 800 ms (`OOK_PERIOD_MS`, contre un émetteur qui
ne s'arrête jamais), durée doublée après chaque fenêtre sans code décodé (un parasite), jusqu'à 3,2 s
(`OOK_PERIOD_MAX_MS`) ; un signal fort, −75 dBm ou plus (`OOK_STRONG_DBM` : une télécommande tout près du badge),
ouvre une fenêtre même pendant cette attente. Une fenêtre s'ouvre de plus toutes les 10 s (`OOK_FORCED_MS`), pour une
télécommande plus faible que le seuil. Mesuré entre deux badges : 97 à 98 % des pings reçus (82 à 85 % avec des fenêtres de 80 ms
toutes les 800 ms, 73 % avec les anciennes fenêtres de 220 ms).
Pendant une fenêtre, le réseau n'entend rien. Les fonctions qui ont besoin de tous les paquets (choeur, image,
échange de cartes) suspendent les fenêtres avec `remote_pause_windows()` (appels comptés), et il n'y en a pas en
mode chat. Le badge de talk, lui, écoute la télécommande en permanence : il ne reçoit les commandes admin qu'en OOK.
Si la radio reste occupée, le badge admin renonce aux trames Princeton au bout de 2 s (`OOK_GIVE_UP_MS`,
`remote: princeton not sent (radio busy)`) et envoie quand même les paquets réseau. `!` trace l'état de l'écoute
(`remote state: ...` : fenêtre, pauses, récepteur et émetteur OOK, trames en attente, envois restants, attente
entre deux fenêtres).

**Boutons de la télécommande du Flipper** : l'application Sub-GHz du Flipper, sur un fichier Princeton enregistré,
envoie le code du fichier avec OK, et avec les flèches le même code avec un autre bouton dans le quartet de poids
faible : haut = 2, bas = 4, gauche = 8, droite = F. `flipper_buttons()` place sur ces boutons les commandes qui
manquent à un groupe, pour qu'un seul fichier pilote tout le groupe :

| Code reçu | Commande exécutée |
|---|---|
| 0x04 | 0x03 : fin du mode muet |
| 0x18 | 0x10 : talk éteint |
| 0x1F | 0x13 : talk rouge (fini) |

Les fichiers `tools/flipper/SecSea_general.sub` (`C16A01` : OK cigale, haut muet, bas fin du mode muet) et
`tools/flipper/SecSea_talk.sub` (`C16A11` : OK vert, haut orange, droite rouge, bas rouge énervé, gauche éteint)
sont des clés Princeton 24 bits, te = 400 µs, préréglage `FuriHalSubGhzPresetOok650Async`.

**Mode muet** (`store_t.muted`) : `audio_set_mute()` continue de jouer les échantillons au niveau 0 (les lecteurs
gardent leur horloge), `set_leds()` et les LEDs des jeux restent éteintes, `app_tone()` et `app_leds()` le respectent.
Une application `owns_leds` (le badge de talk) pilote ses LEDs et son buzzer même en mode muet. Dans l'état
« STOP ! » (rouge énervé), le badge de talk fait chanter la cigale (`noise_gen_set_enabled(true)` après
`audio_close()` : le générateur de bruit pilote le buzzer à pleine amplitude) au lieu d'un bip.

**Depuis un Flipper Zero** : la commande `subghz tx` de la ligne de commande du Flipper ne transmet pas la clé telle
quelle (elle remplace le quartet de poids faible par 6) ; utiliser un fichier `.sub` (§ 6.19) ou Sub-GHz > Add Manually > Princeton_433 puis modifier la ligne `Key:`.


### 6.14 Récepteur OOK et décodeurs 433 MHz

- [ook_rx.c](../../src/menu/ook_rx.c) met le CC1101 en réception OOK asynchrone (registres du préréglage « AM650 »
  du Flipper, bande de 650 kHz) : le signal démodulé sort sur GDO0. Une interruption (`gpio_add_raw_irq_handler()`)
  mesure les durées entre les fronts dans un anneau de 1024 ; la boucle principale découpe les transmissions
  (50 ms de silence) et les décode avec `ookdec_decode()`.
- **Décodage par morceaux** : quand l'anneau remplit le décodeur sans silence (transmission longue, bruit), il est
  décodé par morceaux, avec un recouvrement de 120 durées (`OVERLAP`, plus de deux trames Princeton) : une trame à
  cheval sur la coupure n'est pas perdue. Un morceau qui ne décode rien (200 durées au plus, `RETAIN_MAX`) est gardé
  et décodé à nouveau avec le suivant, s'il arrive dans les 400 ms (`RETAIN_US`) : le Flipper laisse parfois plus de
  50 ms entre deux trames, et une trame seule ne suffit pas (le code doit être vu deux fois). Vérifié : 10 codes sur
  10 envoyés par le Flipper décodés, contre 2 sur 8 avant. La touche `O` trace les tentatives de décodage.
- **Radio prêtée** : quand une autre fonction a utilisé la radio pendant une écoute (message, porteuse),
  `radio_tools.c` appelle `ook_rx_resume()`, qui réécrit les registres OOK et relance la réception.
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
- **Puissance des trames** : pendant que la porteuse est présente (GDO0 haut), `ook_rx_task()` lit le RSSI et garde
  le maximum ; ce pic est attribué à la trame décodée suivante, `ook_rx_last_rssi()` (−128 : inconnu). Le pic d'un
  signal qui ne décode rien (du bruit) est oublié.
- **Chasse 433 MHz** (`app_hunt433`, [hotcold.c](../../src/menu/hotcold.c)) : le récepteur OOK écoute en permanence
  (`ook_rx_start()`) ; la page liste les codes entendus (8 au plus, `TARGETS` : protocole, code, dBm de la dernière
  trame, nombre de trames), puis suit celui choisi avec la vue, les LEDs et les bips du chaud - froid entre badges,
  sur la puissance de chacune de ses trames. Il est perdu après 15 s sans trame (`TARGET_LOST_MS` ; 5 s pour
  une balise de badge). Trace : `hunt433: <trame> at <n> dBm`.
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
  un bloc toutes les 70 ms à +10 dBm (un bloc dure environ 53 ms sur l'air : la file garde de la place pour les
  autres fonctions), et toute la séquence **deux fois** (un bloc manqué au premier passage arrive au second),
  soit environ 17 s ;
- le récepteur n'affiche que la progression (nombre de blocs et jauge, redessinés tous les 25 blocs : chaque
  rafraîchissement rapide laisse un peu de fantôme) ; l'image complète est affichée par `app_show_still()`, comme la
  veille (forme d'onde OTP, sans fantôme), avec le nombre de blocs corrigés. Les fenêtres des télécommandes sont
  suspendues pendant l'envoi et la réception.


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
  Ne pas utiliser `subghz tx` pour les codes Princeton : la clé n'est pas transmise telle quelle (quartet de poids
  faible remplacé par 6).
- **Télécommandes prêtes** : `tools/flipper/SecSea_general.sub` et `SecSea_talk.sub` (§ 6.13).
- **Cartes de visite** : `subghz chat 433920000 0` lit les cartes envoyées par les badges en mode échange,
  et peut en envoyer une en tapant ses lignes (§ 6.20).


### 6.20 Cartes de visite (vCard en mode chat)

[contacts.c](../../src/menu/contacts.c) échange les cartes en clair, sur le profil du chat du Flipper (mode chat,
§ 6.12) ; [vcard.c](../../src/menu/vcard.c) les met en forme et les relit, sans dépendre du matériel (testé sur PC) :
- **Format** : une vCard 3.0 (RFC 6350), une ligne par paquet de texte, terminée par `\r\n`, 60 octets au plus
  (`VCARD_PACKET_MAX`) ; une ligne plus longue est pliée (*folding*) : les paquets de suite commencent par une espace.
  Propriétés : `N`, `FN`, `ADR`, `TEL`, `EMAIL`, `ORG`, `TITLE`, `URL;TYPE=linkedin`, `URL;TYPE=git`, `URL`,
  `X-MASTODON`, `NOTE` ; seuls les champs cochés sont envoyés. Puis `CATEGORIES` : les compétences cochées
  (`skills_to_text()` : « Électronique,Flipper Zero »), dès qu'il y en a une ; à la réception,
  `skills_from_text()` les retrouve (sans tenir compte des majuscules, des accents, des espaces ni des « / - . »,
  les noms inconnus sont ignorés) et elles sont gardées dans `store_ext_t.contact_skills` ; `contacts_export()`
  (touche `k`) les rend en `CATEGORIES`.
- **Contrôle** : avant `END:VCARD`, une ligne `X-SECSEA-CHECK:<lignes>-<crc>` (extension permise par la norme) :
  le nombre de lignes depuis `BEGIN:VCARD` et le CRC-16/CCITT-FALSE (hexadécimal) de ces lignes jointes par `\n`.
  Une carte dont une ligne est perdue ou mélangée avec celles d'une autre carte est rejetée : elle arrivera à l'envoi
  suivant. Une carte sans cette ligne (tapée sur un Flipper, `nom: LIGNE`) est acceptée telle quelle ; le préfixe
  du nom et les codes de couleur du chat du Flipper sont retirés.
- **Envoi** : la carte entière est renvoyée toutes les 2,5 s plus un délai aléatoire d'une seconde au plus
  (deux badges ne restent pas en phase), une ligne toutes les 70 ms. Après chaque paquet de chat entendu,
  l'envoi suivant de la carte attend 2 s : une radio n'entend rien pendant qu'elle émet (semi-duplex).
  La même carte reçue à nouveau (même CRC) n'est proposée qu'une fois.
- **Tailles** : 512 octets par carte (`CONTACT_BYTES`) ; e-mail 47 caractères, LinkedIn, Git et site web 55,
  Mastodon 47 (tailles de `FIELDS`, 0 final compris) ; l'éditeur de texte va jusqu'à 56 caractères (`UI_EDIT_MAX`).
- **Vie privée** : pendant l'échange, n'importe quel Flipper à portée en mode chat lit la carte ; seuls les champs
  cochés sont envoyés. L'ancien type `NET_CONTACT` (0x07) sert maintenant à la contrebande (`NET_TRADE`).


### 6.21 Réglage automatique de la radio

[radio_tune.c](../../src/menu/radio_tune.c) (Réglages > Réglage radio) s'ouvre tout seul au démarrage tant que
`store_t.radio_tuned` n'est pas `STORE_RADIO_TUNED` (0xA5) : premier démarrage, ou mise à jour qui l'apporte
(le champ vaut alors 0xFF). Un réglage interrompu recommence donc au démarrage suivant. Les fenêtres des
télécommandes sont suspendues pendant les mesures (le réseau écoute en permanence). Trois étapes :
1. **Quartz** : la mesure de `radio_tools.c` (`radio_tools_measure_xosc()`, 26 ou 27 MHz).
2. **Bruit** : 150 lectures du RSSI en 3 s (`NOISE_MS`, une toutes les 20 ms) quand aucun paquet n'est en cours de
   réception ; la médiane est le bruit (`radio_noise_dbm`). Le seuil d'écoute des télécommandes devient
   `remote_trigger_dbm()` = bruit + 15 dB, borné entre −95 et −70 dBm (§ 6.13) : plus sensible dans un endroit
   calme, pas trompé dans un endroit bruyant.
3. **Fréquence** : pendant 10 s (`FREQ_MS`), les FREQEST des paquets des autres badges (`net_freq_offsets()`).
   Avec au moins 2 paquets (`FREQ_MIN_PACKETS`), la **moitié** de l'écart moyen est ajoutée à la correction
   (FSCTRL0, pas de fXOSC / 2^14, environ 1,6 kHz ; en émission et en réception) : deux badges réglés en même temps
   se rejoignent au lieu de se croiser. Un écart moyen de plus de 60 pas (`FREQ_MAX_STEPS`, environ 95 kHz) est une
   mesure fausse : ignoré ; la correction reste entre −60 et +60.

À la fin, `radio_tuned`, `radio_noise_dbm` et `radio_freq_offset` sont enregistrés, `radio_tools_set_freq_offset()`
applique la correction et `net_reconfigure()` fait reconfigurer la radio ; `radio_tools_init()` la relit au démarrage.
Trace : `tune: crystal ... Hz, noise ... dBm (remotes above ... dBm), frequency offset <avant> -> <après> (<n> packets,
mean <m>)`. La page montre les étapes avec une jauge (pied de page « G : arrêter »), puis le quartz, le bruit, le
seuil des télécommandes, la correction et le nombre de paquets entendus (« D : régler  G : retour »).


### 6.22 LEDs des cigales (badge admin)

[ledcast.c](../../src/menu/ledcast.c) (Admin > LEDs des cigales) : une couleur (liste de 9 couleurs, ou R, G, B de
0 à 255) et un mode : `Fixe`, `Clignotant` (durées allumé / éteint) ou `Fondu` (vers la couleur / vers le noir),
durées de 50 ms à 5 s par pas de 50 ms.
- **Paquet** `NET_LEDS` : `[nonce 2][mode][r][g][b][durée 1, ms, u16 LE][durée 2, ms, u16 LE]`, à +10 dBm, envoyé
  5 fois en 2 s (toutes les 450 ms). Mode 0 : « Rétablir ». Le même émetteur + nonce n'est appliqué qu'une fois ;
  les durées reçues sont bornées à 50 ms - 5 s. Trace : `leds: <mode>, color r g b, times t1 / t2 ms (from <id>)`.
- **Réception** : `ledcast_show()`, appelée par `set_leds()` de `main.c`, remplace l'animation du badge
  (`leds_anim_fixed()`, `leds_anim_blink()`, `leds_anim_fade()`) jusqu'à « Rétablir » ou un redémarrage (rien n'est
  gardé en flash). Le mode muet éteint toujours les LEDs (vérifié avant) ; la boucle principale n'applique pas
  l'ordre reçu pendant un jeu ni dans une application `owns_leds` (badge de talk) : il s'applique à leur sortie.
- **Émission** : le badge admin s'applique l'ordre à lui-même. La page est `owns_leds` : elle montre la couleur
  et le mode choisis sur ses propres LEDs pendant le réglage.
- **Bibliothèque `leds`** : deux animations ajoutées, `leds_anim_blink(couleur, allumé_us, éteint_us)` et
  `leds_anim_fade(couleur, montée_us, descente_us)` (du noir à la couleur puis retour au noir, en boucle) ;
  `leds_anim_t` a un second temps, `period2`.


### 6.23 Annonces

[announce.c](../../src/menu/announce.c) : un badge admin envoie une annonce, les cigales **construisent l'écran**
à partir de ce qu'elles reçoivent (pas d'image transmise).
- **Contenu** (`store_announce_t`) : l'heure (`"10:30"`), le texte (56 caractères au plus, UTF-8), le type du QR code
  (`ANNOUNCE_QR_NONE`, `URL`, `TEXT`, `TEL`, `SMS`, `EMAIL`, `WIFI`, `GEO`) et son contenu (64 octets).
  `announce_qr_text()` le met dans la forme standard que lisent les téléphones : URL (`https://` ajouté s'il n'y a pas
  de `://`), texte tel quel, `tel:`, `SMSTO:`, `mailto:`, `WIFI:T:WPA;S:<réseau>;P:<mot de passe>;;` à partir de
  `réseau;mot de passe` (`WIFI:T:nopass;S:<réseau>;;` sans « ; »), `geo:`. Un contenu vide : pas de QR code.
- **Écran** (`announce_draw()`) : l'heure en grand dans un bandeau noir de 38 pixels (« Annonce » sans heure), le texte
  en police moyenne coupé aux espaces (`ui_wrapped()`, 3 lignes avec un QR code, 6 sans), puis le QR code
  (`score_code_draw()`, 2 à 4 pixels par module selon la place ; « (QR code trop grand) » en dessous de 2).
- **Radio** : `NET_ANNOUNCE` `[nonce 2][morceau][nombre de morceaux][48 octets au plus]`. L'annonce est sérialisée
  (`heure\0texte\0<type>contenu\0`), coupée en morceaux de 48 octets (5 au plus), un toutes les 70 ms, et le tout est
  envoyé 3 fois (600 ms entre deux tours), à +10 dBm. Le récepteur rassemble les morceaux d'un même émetteur + nonce ;
  une annonce complète n'est prise qu'une fois (`announce: received ...`).
- **Réception** : les 5 dernières annonces sont gardées en mémoire (Social > Annonces, pas en flash).
  `announce_new()` donne à la boucle principale le texte de la notification (« Annonce : ... ») ;
  `announce_open_newest()` fait que la prochaine ouverture de la page affiche la plus récente avec `app_show_still()`,
  comme la veille. `notify()` ouvre la page tout de suite si le badge est sur les menus ou la veille ; sinon, elle
  s'affichera à la prochaine ouverture de Social > Annonces.
- **Admin** (Admin > Annonces (admin), `app_announce_admin`) : 6 annonces modifiables (`STORE_ANNOUNCES`), gardées
  dans `store_ext_t` (§ 6.8), des exemples au départ (`DEFAULTS`). Lignes : Heure (éditeur `UI_CHARSET_TEXT`,
  5 caractères), Texte et Contenu (`UI_CHARSET_LONG`, 56 caractères), type du QR code (ailes), Aperçu
  (`announce_draw()` en rafraîchissement rapide), Envoyer (`announce_send()`, en tâche de fond).


### 6.24 Compétences et succès

- **Compétences** ([skills.c](../../src/menu/skills.c)) : 20 noms (`SKILLS`, `SKILLS_COUNT` ≤ 32), un masque de
  32 bits dans `store_t.skills`. Les pictogrammes de 16 × 16 pixels sont dessinés en ASCII art dans
  [tools/skills_icons.py](../../tools/skills_icons.py), qui génère `skills_icons.h` (un `uint16_t` par ligne, bit de
  poids fort = pixel de gauche ; `--preview icones.png` : une planche de contrôle) ; même ordre que `SKILLS`.
  `skills_draw_icon()` / `skills_draw_row()` les dessinent (badge nominatif : 10 au plus ; page : 9 ; liste « Qui les
  partage ? » : 4). Le masque part dans la balise (§ 6.6) et dans la vCard (`CATEGORIES`, § 6.20). Trace :
  `skills: <nom> on/off`.
- **Succès** ([achievements.c](../../src/menu/achievements.c)) : 32 succès (`ACHV[]` : nom, comment, XP), XP = somme
  des succès obtenus + 2 par cigale rencontrée (`XP_PER_MEETING`, `store_t.n_met`) ; 10 niveaux (`LEVEL_XP` : 0, 20,
  50, 100, 170, 260, 380, 530, 720, 1000 ; `LEVEL_NAMES` : Oeuf ... Cigale d'or). `achievements_init()` donne
  « Premiers pas » ; `achv_task()` (chaque seconde) donne les succès des rencontres et « Expert » ; `achv_event()`
  rend une fois le texte de l'annonce, que la boucle écrit en bas de l'écran avec un carillon (sauf si un son joue).
  Platine est donné dès que tous les succès d'avant `ACHV_ALL` sont obtenus. Le niveau part dans la balise.
  Trace : `achievement: <nom> (+<xp> XP, level <n>)`. Pour ajouter un succès, voir § 5.


### 6.25 Jeux de groupe : le salon (`party.c`) et les jeux

[party.h](../../src/menu/party.h) : un **hôte** ouvre une partie, les cigales autour la rejoignent, l'hôte voit la
liste des joueurs et lance le jeu ; le jeu échange ensuite ses propres messages par `party_send()`. Un seul salon à la
fois sur un badge (une page de jeu affiche « Une partie de ... est en cours » si un autre jeu de groupe tourne).

- **Paquet** `NET_PARTY` : `[jeu][session 2][genre][destinataire 4][données ≤ 47]`, destinataire 0 = tous, à +10 dBm.
  Jeux : `PARTY_GAME_TUG` (1), `PARTY_GAME_ASSASSIN` (2), `PARTY_GAME_WEREWOLF` (3).

  | Genre | De → à | Données |
  |---|---|---|
  | OPEN (1) | hôte → tous, chaque seconde tant que le salon est ouvert | joueurs, maximum, options (`flags` du jeu), nom de l'hôte (8) |
  | JOIN (2) | joueur → hôte, chaque seconde jusqu'à être dans la liste | clé aléatoire (4), nom (8) |
  | ROSTER (3) | hôte → tous | page, pages, joueurs, puis 3 joueurs au plus (id 4, nom 8) ; 4 pages par seconde pendant le salon et les 10 s qui suivent le départ, puis une par seconde |
  | START (4) | hôte → tous, 5 fois (300 ms d'écart), puis chaque seconde pendant le jeu | joueurs, délai (2, ms : le jeu commence ce délai après le paquet), graine (4) |
  | LEAVE (5) | tous | le joueur part ; l'hôte : partie annulée avant le départ ; après le départ, le jeu décide (`PARTY_KIND_LEAVE` donné au gestionnaire) |
  | ≥ 16 (`PARTY_KIND_GAME`) | selon le jeu | messages du jeu, donnés au gestionnaire de `party_set_handler()` une fois la partie lancée |

- **Départ synchronisé** : chaque badge calcule `party_start_time()` = réception du START + délai (même principe que
  le choeur) ; `party_seed()` est la même graine sur tous les badges. Elle passe en clair : jamais pour un secret.
- **Secret** : la clé envoyée par chaque joueur dans son JOIN n'est connue que de l'hôte (les autres reçoivent 0) ;
  `party_mask(clé, sel)` (FNV-1a) sert à masquer un secret destiné à ce joueur (rôle, cible). La clé passe en clair
  dans le JOIN : quelqu'un qui a enregistré le salon peut la retrouver (limite assumée, c'est un jeu).
- **États** (`party_state()`) : IDLE, HOSTING, SCANNING (`party_found()` : les parties entendues, la plus proche en
  premier), JOINING, JOINED, STARTED, CANCELLED. 40 joueurs au plus (`PARTY_MAX`). Trace : `party: ...`.
- Chaque jeu a un **service** appelé par la boucle (`tug_service()`, `assassin_service()`, `werewolf_service()`) : la
  partie continue quand le joueur regarde une autre page ; ses nouvelles passent par `assassin_event()` /
  `werewolf_event()` et `notify()`.

**Tir à la corde** ([tug.c](../../src/menu/tug.c), règles sans matériel dans [tug_logic.c](../../src/menu/tug_logic.c)) :
2 joueurs au moins. `tug_teams()` tire les équipes Cigales / Fourmis avec la graine commune (Fisher-Yates, xorshift :
les mêmes sur tous les badges), l'arbitre est le joueur en trop. Phases calculées depuis l'heure de départ commune :
équipes 5 s, compte à rebours 3 s, tirage 20 s (`PULL_MS`), décompte final, résultat. Une traction = aile gauche
puis aile droite (`tug_pull()`). Chaque badge envoie son total (`K_COUNT` : tractions 2 octets, drapeau « arrêté »)
toutes les 300 ms + 40 ms par joueur ; chaque badge additionne les totaux de chaque équipe : les écrans sont d'accord
même loin de l'hôte. Une équipe en avance de `tug_margin()` (20 tractions par joueur de l'équipe) arrête la partie
aussitôt (le drapeau arrête les autres). Après le résultat, le badge quitte la partie (5 s). Trace : `tug: ...`.

**Assassin** ([assassin.c](../../src/menu/assassin.c), [assassin_logic.c](../../src/menu/assassin_logic.c)) : 3 joueurs
au moins (2 quand l'hôte est en mode admin, pour les essais). L'hôte tire un **cercle secret** avec son propre
générateur (pas la graine commune) et un code secret par joueur, et envoie à chacun **sa** cible seulement
(`K_TARGET`, masquée avec `party_mask()` de sa clé) jusqu'à l'accusé (`K_TARGET_ACK`).
- **Élimination** : aile droite envoie `K_KILL` à la cible (toutes les 400 ms pendant 4 s) avec une preuve (hash du code
  de la cible et de l'id du tueur). La cible l'accepte avec une preuve juste et un RSSI ≥ `ASSASSIN_KILL_RSSI`
  (**−50 dBm**, badges presque collés, **à calibrer** : la victime trace `assassin: KILL from <nom>, rssi <n>` à chaque
  tentative ; `-DASSASSIN_KILL_RSSI=-90` pour un essai). Elle renvoie alors `K_DEAD` au tueur : sa propre cible,
  scellée avec son code (seul le tueur peut l'ouvrir), jusqu'à `K_DEAD_ACK`.
- **STATUS** : chaque badge envoie de temps en temps (20 s ± 5 s, en rafale après un événement) les morts, les
  abandons et le gagnant ; les badges les fusionnent : un paquet perdu ne perd pas la partie. Un joueur qui abandonne
  met sa cible scellée dans ses STATUS : son chasseur l'ouvre et continue.
- **Faiblesses connues** (listées en tête de `assassin.c`) : la clé en clair dans le JOIN, une preuve refusée
  (trop loin) rejouable de près, un émetteur plus fort tue de loin, de faux STATUS. Trace : `assassin: ...`.
- La jauge chaud - froid suit les balises de la cible (§ 6.6) ou ses paquets du jeu.

**Loup-garou** ([werewolf.c](../../src/menu/werewolf.c), règles dans [werewolf_logic.c](../../src/menu/werewolf_logic.c)) :
un meneur (qui ne joue pas : `party_host(..., false, ...)`) et 8 à 18 joueurs (2 loups jusqu'à 11, 3 dès 12 ; un rôle par case : voyante, sorcière, chasseur, Cupidon, petite fille, capitaine, voleur). Le badge du meneur distribue les rôles
et enchaîne les phases ; ses messages (STATE, NAMES, PRIV masqué par la clé de chaque joueur, ACT, ABORT) sont
décrits en tête de `werewolf.c`. La nuit, tous les joueurs vivants choisissent dans une liste et reçoivent le même
genre de paquet : personne ne devine les rôles. Mode test : meneur en mode admin, des robots complètent la partie.
Règles et déroulement : [loup_garou.md](loup_garou.md).


### 6.26 Contrebande

[smuggler.c](../../src/menu/smuggler.c) : 26 marchandises ([smuggler_goods.c](../../src/menu/smuggler_goods.c),
icônes 32 × 32 générées par `tools/smuggler_icons.py`), la cale dans `store_t.cargo[32]`, des trouvailles aux
rencontres, des échanges et des cadeaux entre deux badges « à portée de main » : RSSI des balises ≥
`SMUGGLER_TRADE_RSSI` (**−55 dBm**, badges collés, **à calibrer** sur place). Le protocole d'échange
([smuggler_trade.c](../../src/menu/smuggler_trade.c), sans matériel, testé sur PC) passe par `NET_TRADE` (0x07) :
`[id de l'affaire 4][genre][destinataire 4][données]`, une validation en deux phases où l'invitant décide ; la cale
est écrite tout de suite après un échange (`store_save_now()`), pour qu'une marchandise ne soit jamais dupliquée.
Tout est décrit dans [contrebande.md](contrebande.md).


### 6.27 Radio pirate

[pirate_radio.c](../../src/menu/pirate_radio.c) (Admin > Radio pirate) émet du son en FM bande étroite : le CC1101
en 2-FSK, mode série asynchrone, à 500 kbauds, et la sortie audio (`AUDIO_OUT_RADIO`) qui pilote GDO0 avec une PWM
de ~122 kHz dont le rapport cyclique suit le son ; un récepteur NFM n'en voit que la moyenne, la fréquence
instantanée suit le son. L'écoute (Radio & IR > Écouter la radio pirate) met le CC1101 d'un autre badge en 2-FSK
asynchrone en réception (canal de 406 kHz) : GDO2 suit la PWM de l'émetteur, une tranche PWM du RP2040 compte le temps
haut et un DMA à 16 kHz le recopie ; la boucle en tire le son, mesure la tonalité (Goertzel) et corrige l'écart de
fréquence (AFC). Le réseau est en pause pendant l'émission et l'écoute ; l'émission s'arrête au bout de 10 minutes.

**Essai entre deux badges** : la tonalité de test de 1 kHz est retrouvée par le badge récepteur avec l'émetteur à
**+10 dBm** (pureté 84 %) ; à **0 dBm**, elle arrive faible : le canal large (406 kHz) du récepteur du badge le rend
peu sensible. Détails, réglages et écoute avec un Portapack ou un SDR : [radio_pirate.md](radio_pirate.md).


### 6.28 Sonneries et livres-jeux

- **Sonneries** ([rtttl.c](../../src/menu/rtttl.c), analyseur sans matériel
  [rtttl_parse.c](../../src/menu/rtttl_parse.c)) : 12 sonneries intégrées (`RTTTL_BUILTIN`), puis les fichiers
  `.txt`, `.rtttl` et `.rtx` du dossier `SONNERIES` (24 fichiers, 128 sonneries). Le son est synthétisé (onde carrée,
  enveloppe) environ 150 ms en avance dans le tampon audio ; la note affichée et les LEDs suivent `audio_played()`.
  Trace : `rtttl: ...`. Format : [sonneries.md](sonneries.md).
- **Livres-jeux** ([gamebook.c](../../src/menu/gamebook.c), analyseur sans matériel
  [gamebook_parse.c](../../src/menu/gamebook_parse.c)) : le livre intégré
  ([gamebook_builtin.c](../../src/menu/gamebook_builtin.c), généré par
  `python tools/gamebook_check.py docs/sd/LIVRES/tresor_cigalon.txt --c src/menu/gamebook_builtin.c`) et les `.txt`
  du dossier `LIVRES` (16 au plus). Le badge ne lit que la section affichée ; la progression est dans
  `store_t.book_hash` et `book_section`. Trace : `gamebook: ...`. Format et limites : [livres_jeux.md](livres_jeux.md).


### 6.29 Mode démo

Admin > Mode démo ([demo.c](../../src/menu/demo.c)) explique puis appelle `demo_start()` de `main.c` : la boucle
enchaîne les étapes de `DEMO_STEPS` (nom, durée) : badge nominatif 8 s, succès 6 s, images 18 s (`image_next()`
toutes les 6 s), vidéo 20 s, compétences 5 s, musique 10 s, démo écran 15 s, programme 6 s, radar 6 s,
livres-jeux 5 s, crédits 6 s, infos 5 s, puis recommence. Les médias jouent le premier fichier du navigateur (ou de
son premier dossier) ; sans carte SD ni fichier, l'étape est sautée (`demo: <étape> skipped`). `demo_back()` revient
au menu pas à pas (une vidéo s'arrête en plusieurs tours). N'importe quel bouton arrête la démo (l'appui n'est pas
donné à la page) ; les LEDs (arc-en-ciel pendant la démo) reprennent leur mode, la veille est retenue pendant la démo.
Traces : `demo: on`, `demo: <étape>`, `demo: off`.


## 7. Formats de fichiers

| Format | Contenu |
|---|---|
| `.EPI` (image) | en-tête de 16 octets : `EPIMAGE1`, largeur, hauteur, bits par pixel (1 ou 2), 0 (uint16 little endian) ; puis le plan « lsb », puis le plan « msb » (si 2 bits). Chaque plan fait 5000 octets, bit 7 = pixel de gauche, ligne par ligne. Gris = msb×2 + lsb (0 = noir, 3 = blanc). |
| `.EPV` (vidéo) | en-tête de 512 octets : `EPVIDEO2`, largeur, hauteur, fps, bits par pixel, nombre d'images, fréquence audio, nombre d'échantillons ; puis les images de 5000 octets (1 = blanc) ; puis le son, 8 bits non signé, synchronisé sur la première image. `EPVIDEO1` = sans son. |
| `.WAV` | PCM 8, 16, 24 ou 32 bits, flottant 32 bits, `WAVE_FORMAT_EXTENSIBLE` ; mono ou stéréo, 4 à 192 kHz (au-dessus de 48 kHz, décimé) ; 8 bits 16 kHz mono conseillé (`audio2wav.py`). |
| `.TXT` | UTF-8 (avec ou sans BOM) ou Windows-1252, détecté automatiquement. |
| Sonneries (`.txt`, `.rtttl`, `.rtx`) | une sonnerie RTTTL par ligne, `nom:d=4,o=5,b=120:notes` ; lignes vides et `#` ignorées ; 2048 caractères par ligne au plus ([sonneries.md](sonneries.md)). |
| Livre-jeu (`.txt`) | titre, auteur, puis des sections `== n [FIN [gagné\|perdu]]`, des choix `-> n [objet\|!objet\|dé a-b] : texte`, des objets `+[x]` / `-[x]` ; 400 sections, 8 choix, 8 objets au plus ([livres_jeux.md](livres_jeux.md)). |
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
| `!` | diagnostic : OLED, IR, réseau des cigales et voisins, CTF, compteurs du réseau (émis, reçus, perdus, loopback), état du réseau et de la radio (`net state:`, § 6.12), état de l'écoute des télécommandes (`remote state:`, § 6.13), télécommande (activée, muet, mode admin), version du firmware (`version:`), radio (version, quartz utilisé et mesuré), batterie |
| `?` | diagnostic du son |
| `i` | test IR : décodage d'une trame NEC puis émission (environ 68 ms) |
| `o` | état du récepteur OOK : actif, impulsions, trames, RSSI, MARCSTATE, GDO0 |
| `p` | durées du dernier signal donné au décodeur OOK |
| `O` | trace des tentatives de décodage du récepteur OOK : marche / arrêt |
| `k` | export des cartes de visite reçues en vCard (lu par `tools/contacts_export.py`) |
| `V` | trace de chaque paquet réseau émis et reçu (avec l'écart de fréquence mesuré, FREQEST ; en mode chat, le texte reçu) : marche / arrêt |
| `r` | registres du CC1101 et PATABLE |
| `M` | envoie le « Radio : message » (profil chat du Flipper : `subghz chat 433920000 0` le reçoit) |
| `P` | ping à +10 dBm : les badges qui l'entendent écrivent `net: ping #n from <id>, rssi ...` |
| `L` | mode *loopback* (§ 6.12) : marche / arrêt |
| `R` | redémarrage (watchdog) |
| `U` | vérification des textes (§ 5, `ui.h`) : trace `uicheck: ...` pour les textes coupés, trop larges ou sous le pied de page : marche / arrêt |
| 0x01 + `A` / `a` | mode admin : marche / arrêt (case « Mode admin » de `badge_remote.py`) ; le badge répond `admin: on` / `admin: off` |
| 0x02 + caractère | insère le caractère (ASCII) dans l'éditeur de texte ouvert (mode clavier de `badge_remote.py`) ; `\r` : terminé, Échap : annuler, `\b` : effacer |

Le badge envoie des lignes de texte :
- `ui: <titre>` à chaque changement de page (le nom de l'application à son ouverture) ;
- `browser:`, `image:`, `saver: on/off`, `radio:`, `social:`, `game:`, `ctf: code right/wrong`, `credits:`, `name:`...
- `notify:` (notification), `net:`, `remote:`, `admin:`, `ook:`, `talk:`, `vote:`, `message:`, `program:`,
  `infection:`, `hotcold:`, `hunt433:`, `contacts:`, `chorus:`, `duel:`, `battle:`, `crypto:`, `store:`, `tune:`,
  `leds:`, `announce:`, `reset:`, `uicheck:`, `skills:`, `achievement:`, `party:`, `tug:`, `assassin:`, `werewolf:`,
  `smuggler:`, `pirate:`, `rtttl:`, `gamebook:`, `demo:`, `wav:`...
- `version: <numéro> (<commit> <date>)` au démarrage ;
- `music: end at <n>s of <durée>s` à la fin d'une musique (ou sur une erreur de lecture : la position est alors
  avant la fin), utilisé par `tools/badge_media_test.py`.

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
| `vcard` | paquets de la vCard (60 octets au plus, pliage, champs cochés), ligne de contrôle : ligne perdue (ou suite de ligne pliée) et valeur altérée rejetées ; carte tapée sur un Flipper acceptée, autres textes du chat ignorés, valeurs les plus longues (55 caractères) |
| `ookdec` | télécommandes (Princeton, CAME, Nice FLO) et sondes (Nexus-TH, ThermoPRO-TX4, GT-WT02, inFactory, LaCrosse, Acurite) avec gigue, parasites, horloge ±20 %, répétition exigée sans somme de contrôle ; le bruit ne décode rien |
| `werewolf` | règles du loup-garou (rôles, votes, morts, amoureux, gagnants), puis des parties entières entre badges simulés (`werewolf.c` compilé une fois par badge) sur une radio qui perd des paquets, boutons au hasard, textes vérifiés |
| `rsvp` | découpage des mots, typographie française, BOM, Windows-1252, durées, mots longs, avance / recul |
| `gamebook` | format des livres (CRLF, BOM, commentaires, objets, dé, fins), sections manquantes, lignes et textes longs, typographie, coupure des lignes ; le livre intégré : identique à `docs/sd/LIVRES`, liens valides, des parties au hasard arrivent toutes à une fin |
| `rtttl` | valeurs par défaut, durées, points, dièses, octaves, silences, espaces, majuscules, erreurs et leur position, texte quelconque (pas de plantage), sonneries du firmware et de `docs/sd/SONNERIES` |
| `screen` | fenêtres RAM, copie de l'écran, rétablissement après `screen_clear` |
| `party_games` | tir à la corde (équipes identiques sur tous les badges, arbitre, tractions) et assassin (cercle, sceau, parties) |
| `smuggler` | la cale, les tirages, `smuggler_goods.c` à jour des icônes ; le protocole d'échange sur une radio qui perd, répète et retarde les paquets : aucune marchandise créée ni perdue |
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
Le groupe `admin` vérifie la séquence secrète des flancs ; les autres ouvrent le thème Admin par la commande série
0x01 `A` (`open_admin()`), comme la case de `badge_remote.py`. Les listes `SOCIAL` et `ADMIN` du script suivent
l'ordre des menus : à mettre à jour avec `SUBMENUS`.

Les groupes (`--only`) :

| Groupe | Vérifie |
|---|---|
| `diag` | diagnostic `!` : version du firmware, version du CC1101, quartz, batterie |
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
| `net` | avec un seul badge, en *loopback* : vote, virus, message |

### 9.3 Tests à deux badges

Deux badges branchés en USB, `badge_menu` flashé, ports série libres. Les boutons sont simulés et les traces des deux
badges vérifiées ; captures d'écran dans un dossier daté. Les badges sont redémarrés d'abord (sauf `--no-reboot`).

```bash
python tools/test_party_games.py --ports COM9 COM11 [--only tug,assassin]   # tir à la corde, assassin
python tools/test_werewolf.py --ports COM9 COM11 [--advanced]               # loup-garou (meneur + robots)
python tools/test_smuggler.py --ports COM9 COM11                            # contrebande
```

- `test_party_games.py` : le badge 1 crée la partie, le badge 2 la rejoint ; au tir à la corde, seul le badge 2 tire
  et son équipe doit gagner, avec le même résultat sur les deux ; à l'assassin (le badge 1 passe en mode admin pour
  jouer à 2), le badge 1 élimine le badge 2 : badges côte à côte, le RSSI mesuré par la victime est affiché (pour
  calibrer `ASSASSIN_KILL_RSSI`).
- `test_werewolf.py` : le badge 1 est le meneur, passé en mode admin le temps du test (des robots complètent la
  partie jusqu'au minimum), le badge 2 le seul vrai joueur ; vérifie le rôle reçu, l'accusé de chaque choix, les
  mêmes phases et la même fin sur les deux badges, et qu'aucun texte n'est coupé.
- `test_smuggler.py` : badges collés ; un échange, un cadeau, un refus, une annulation après que l'invité a scellé sa
  marchandise. La cale des badges change : à faire sur des badges de test.

### 9.4 Applications de test par module

`src/tests/*.c` : une application par module (écran, radio, LEDs, son...), à flasher pour explorer un périphérique.


## 10. Outils

| Script | Usage |
|---|---|
| `tools/badge_remote.py` | fenêtre avec l'écran du badge en grand (zoom 2–4), flèches / Entrée = boutons, Maj = appui long (et boutons « appui long » à l'écran), capture PNG ; liste « Badge : » pour choisir parmi plusieurs badges (ou `--port`) ; case « Mode clavier » : le texte tapé va à l'éditeur du badge (0x02 + caractère ; Entrée : terminé, Échap : annuler, retour arrière : effacer) ; case « Mode admin » (0x01 + `A` / `a`), qui suit l'état du badge (lignes `admin: on/off`, et `!` envoyé à la connexion) ; le fil de lecture ne s'arrête jamais (erreurs dans la ligne d'état et le journal) ; `--snapshot fichier.png` pour une capture seule |
| `tools/badge_selftest.py` | test automatique du badge (§ 9.2) |
| `tools/badge_screens.py` | parcourt tous les thèmes, toutes les entrées (Admin compris) et les pages des applications, enregistre une image PNG de chaque écran dans `docs/screens/` et écrit [docs/fr/ecrans.md](ecrans.md) et [docs/en/screens.md](../en/screens.md) ; avec la vérification des textes (`U`), liste les textes coupés, trop larges ou sous le pied de page (`docs/screens/checks.txt` et fin des pages) ; `--port`, `--only Jeux,Social`. Le badge est redémarré ; les interrupteurs des menus ne sont pas pressés |
| `tools/badge_media_test.py` | lit toutes les vidéos et musiques de la carte SD (sous-dossiers compris) et vérifie que chacune va jusqu'au bout (erreur de lecture, arrêt avant la fin, images par seconde des vidéos) ; `--videos`, `--music` (les deux par défaut), `--max N` (N secondes de chaque fichier au plus, 0 = en entier), `--port` ; rapport affiché et écrit dans `media_report.txt` |
| `tools/score_check.py` | vérifie les QR codes de score et fait le classement (§ 6.11) |
| `tools/contacts_export.py` | cartes de visite reçues par le badge → `.vcf` (`--port`, `-o`) |
| `tools/crypto_ctf_make.py` | génère la table des défis crypto (`--update`, `--answers`) : fichier des solutions (§ 6.18) |
| `tools/ook_sub.py` | `.sub` du Flipper : Princeton, CAME, Nice FLO, sondes météo ; `check`, `selftest` (§ 6.19) |
| `tools/flipper_net_sub.py` | `.sub` du Flipper pour le réseau des cigales (`command`, `ping`, `raw`) et préréglage « SecSea » (`preset`) (§ 6.19) |
| `tools/skills_icons.py` | pictogrammes 16 × 16 des compétences en ASCII art → `src/menu/skills_icons.h` ; `--preview icones.png` (§ 6.24) |
| `tools/smuggler_icons.py` | icônes 32 × 32 des marchandises → `src/menu/smuggler_goods.c` ; `--png planche.png`, `--check` (§ 6.26) |
| `tools/gamebook_check.py` | vérifie un livre-jeu (erreurs, remarques), `--pages`, `--play` (y jouer dans le terminal), `--c` (écrit le livre intégré) (§ 6.28) |
| `tools/test_party_games.py`, `tools/test_werewolf.py`, `tools/test_smuggler.py` | tests à deux badges (§ 9.3) |
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
| Aucun paquet radio reçu | quartz : comparer « crystal used » et « measured » de la ligne `radio:` de `!` ; même préréglage des deux côtés ; Réglages > Réglage radio près d'autres badges (écart de fréquence, § 6.21) |
| `net: radio repaired (...)` dans les traces | la surveillance du réseau a remis la radio dans sa configuration (§ 6.12) ; si cela se répète, chercher la fonction qui laisse la radio dans un autre état (registres de la ligne `net state:` de `!`) |
| Écran figé ou uniforme | appel pendant `screen_busy()` ; après `screen_clear()`, le bypass RAM est rétabli au dessin suivant |
| Son inaudible | volume ; fichier converti par `audio2wav.py` (sans `--no-filter`) |
| Deux modules se disputent une interruption GPIO | utiliser `gpio_add_raw_irq_handler()` |
| Nouveau champ de `store_t` à 0xFF | normal pour un badge déjà utilisé : tester et prendre une valeur par défaut |
| Tester une fonction entre badges avec un seul badge | touche `L` (*loopback*, § 6.12), et `V` pour voir les paquets |
| Un paquet n'arrive pas | `V` sur les deux badges ; compteur « dropped » de `!` (file pleine, données trop longues, mode chat) ; une fenêtre OOK (émetteur non-badge entendu, ou toutes les 10 s) rend le réseau sourd un moment : répéter les paquets importants, ou suspendre les fenêtres avec `remote_pause_windows()` (§ 6.13) |
| Un code du Flipper n'est pas décodé | touche `O` : tentatives de décodage ; le code doit être vu deux fois (§ 6.14) |
| Plus aucun badge n'entend ce badge après une écoute OOK | FREND0 resté à 0x11 : la configuration GFSK doit réécrire FREND0, FREND1, MDMCFG0 (§ 6.14) |
| Le Flipper envoie une autre clé Princeton | `subghz tx` en ligne de commande : utiliser un `.sub` et `subghz tx_from_file` (§ 6.19) |
| Une élimination de l'assassin ou une cigale « à portée de main » de la contrebande ne passe pas | seuils de RSSI provisoires (`ASSASSIN_KILL_RSSI` −50 dBm, `SMUGGLER_TRADE_RSSI` −55 dBm) : lire les RSSI tracés (`assassin: KILL from ..., rssi`, `smuggler: at hand ...`) et ajuster à la compilation (`-D...`) |
| Le Flipper n'enregistre rien des badges | les badges émettent en GFSK (modulation de fréquence) : Read RAW en AM650 / AM270 (par défaut) ne les voit pas. Utiliser le préréglage « SecSea » ajouté à `subghz/assets/setting_user` (§ 6.19), ou à défaut FM476. Vérifier l'émission : `subghz chat 433920000 0` sur le Flipper et `M` sur le badge |
