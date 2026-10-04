# Badge SecSea — guide avancé

Ce guide s'adresse aux utilisateurs avancés et aux organisateurs (staff) de l'événement : ceux qui branchent le
badge sur un ordinateur, utilisent les scripts du dépôt, la console série USB, le menu Admin, le Flipper Zero ou
préparent des cartes SD pour plusieurs badges. Il ne demande pas de savoir programmer.

- L'utilisation de tous les jours (menus, jeux, social...) : [guide utilisateur](guide_utilisateur.md).
- Compiler, modifier le firmware, l'architecture : [guide développeur](guide_developpeur.md).
- Les fonctions détaillées : [sonneries](sonneries.md), [radio pirate](radio_pirate.md),
  [contrebande](contrebande.md), [loup-garou](loup_garou.md), [livres-jeux](livres_jeux.md),
  et toutes les pages du badge en images : [écrans](ecrans.md).

*English version: [advanced guide](../en/advanced_guide.md).*

## Sommaire

1. [Avant de commencer](#1-avant-de-commencer)
2. [Les outils du PC](#2-les-outils-du-pc)
3. [La console série USB](#3-la-console-série-usb)
4. [Le menu Admin](#4-le-menu-admin)
5. [Télécommande, mode muet, sommeil et mode démo](#5-télécommande-mode-muet-sommeil-et-mode-démo)
6. [La carte SD](#6-la-carte-sd)
7. [Flasher et mettre à jour le firmware](#7-flasher-et-mettre-à-jour-le-firmware)
8. [Langue du badge](#8-langue-du-badge)
9. [Dépannage](#9-dépannage)


## 1. Avant de commencer

### 1.1 Les boutons et leurs lettres

Le badge est vu de face, la tête de la cigale en haut. La console série et les scripts désignent les boutons
par une lettre :

| Bouton | Lettre (appui court) | Lettre (appui long) | Dans les menus |
|---|---|---|---|
| Aile gauche (G) | `a` | `A` | retour |
| Aile droite (D) | `b` | `B` | valider |
| Flanc droit | `x` | `X` | descendre |
| Flanc gauche | `y` | `Y` | monter |

Un appui long dure 0,8 s sur le badge.

### 1.2 Ce qu'il faut sur l'ordinateur

| Besoin | Pour quoi |
|---|---|
| Python 3 | tous les scripts |
| `pip install pyserial` | tout ce qui parle au badge par l'USB (`badge_remote.py`, `badge_selftest.py`, `badge_screens.py`, `badge_media_test.py`, `contacts_export.py`, `test_*.py`) |
| Tkinter | la fenêtre de `badge_remote.py` (fourni avec Python sous Windows et macOS ; paquet `python3-tk` sous Debian / Ubuntu) |
| `pip install pillow numpy` | conversion des images et vidéos, `skills_icons.py --preview`, `gen_fonts.py`, `image2epaper.py` |
| [ffmpeg](https://ffmpeg.org/) dans le `PATH` | conversion des sons et des vidéos |
| gcc ou clang | les tests sur PC (`run_tests.py`) |
| picotool (facultatif) | flasher sans manipuler le bouton du badge |

Les autres scripts (Flipper, sonneries, livres-jeux, scores, icônes) n'utilisent que la bibliothèque standard de
Python. Lancez-les depuis la racine du dépôt : plusieurs relisent des fichiers du dépôt (polices, sources C).

### 1.3 Trouver le badge et libérer son port série

- Branché en USB **et interrupteur sur ON**, le badge apparaît comme un port série : `COM9` sous Windows,
  `/dev/ttyACM0` sous Linux, `/dev/cu.usbmodem...` sous macOS. Sur OFF, seule la charge fonctionne.
- Sans `--port`, les scripts prennent le premier appareil Raspberry Pi (identifiant USB 0x2E8A) : avec plusieurs
  badges branchés, précisez le port.
- **Un port série ne s'ouvre qu'une fois** : fermez `badge_remote.py`, le terminal ou un autre script avant d'en
  lancer un nouveau. C'est la première cause des messages « cannot open » / « used by another application ».
- Les messages envoyés par le badge avant l'ouverture du port sont perdus (limite de l'USB) : la ligne `version:`
  du démarrage n'est visible que si le port était déjà ouvert ; tapez `!` pour la revoir.


## 2. Les outils du PC

Tous les scripts sont dans `tools/` (sauf les convertisseurs de médias, dans `src/`). `-h` affiche l'aide de
chacun. Dans les tableaux, « défaut » est la valeur sans l'option.

### 2.1 Tester un badge

#### `tools/badge_selftest.py` — test automatique d'un badge

Simule les boutons par l'USB, vérifie les traces et l'écran du badge : menus, jeux, veille, radio, IR, images,
sonneries, mode admin, batterie, social... Écrit un tableau PASS / FAIL / SKIP et des captures PNG.

- Prérequis : un badge avec `badge_menu`, branché, port libre ; pyserial. Une carte SD avec des images et des
  sonneries évite des SKIP.
- Code de sortie 0 si aucun test n'échoue (un SKIP n'est pas un échec : pas de carte SD, batterie déjà calibrée...).

| Option | Rôle |
|---|---|
| `--port COM9` | port série (défaut : le premier badge trouvé) |
| `--out dossier` | dossier des captures (défaut : `selftest_<date>`) |
| `--only g1,g2` | seulement ces groupes (voir ci-dessous) |
| `--ctf` | ajoute le groupe `ctf` : tape le code Konami (**marque le flag du CTF comme trouvé**) |
| `--no-reboot` | ne redémarre pas le badge (il doit être sur le menu principal) |
| `--verbose`, `-v` | affiche toutes les lignes du badge |

Groupes : `diag`, `menus`, `games`, `puzzles`, `ctf` (seulement avec `--ctf` ou `--only ctf`), `settings`, `radio`,
`ir`, `images`, `ringtones`, `apps`, `admin`, `battery`, `radio433`, `social`, `net`.
Le détail de chaque groupe : [guide développeur § 9.2](guide_developpeur.md#92-test-du-badge-par-lusb).

```bash
python tools/badge_selftest.py
python tools/badge_selftest.py --port COM9 --only games,radio -v
```

Précautions :
- le badge est **redémarré** au début (touche `R`) ; s'il n'a jamais réglé sa radio, le réglage passe d'abord ;
- les records des jeux peuvent changer (une partie de Simon ou de Snake peut finir à 0) ;
- le mode admin, le mode muet et le virus sont activés le temps de certains groupes, puis désactivés : **le badge
  finit hors du mode admin**, même s'il y était avant ;
- le groupe `battery`, sur un badge **non calibré** seulement, enregistre deux points de calibration dans le secteur
  usine puis les efface : un badge calibré n'est pas touché ;
- avec `BADGE_SCORE_KEY` dans l'environnement (voir `score_check.py`), la signature du QR code de score est vérifiée.

#### `tools/badge_media_test.py` — lire tous les médias de la carte SD

Ouvre Médias > Vidéos puis Médias > Musique, joue chaque fichier (sous-dossiers compris) et vérifie qu'il démarre et
va jusqu'au bout (ou jusqu'à la durée demandée) : erreurs de lecture, arrêt prématuré, images par seconde des vidéos.

| Option | Rôle |
|---|---|
| `--port COM9` | port série |
| `--videos` | seulement les vidéos |
| `--music` | seulement les musiques (sans `--videos` ni `--music` : les deux) |
| `--max N` | secondes lues de chaque **vidéo** (défaut 0 : en entier) |
| `--music-max N` | secondes lues de chaque **musique** (défaut 5 : le début ; 0 : en entier) |

```bash
python tools/badge_media_test.py --port COM9 --videos --max 10
```

Produit : un rapport affiché et écrit dans `media_report.txt` (dossier courant) ; code de sortie 1 en cas
d'échec. Le badge est redémarré au début. Pratique pour valider une carte SD avant de la distribuer.

#### `tools/badge_screens.py` — capturer tous les écrans

Parcourt tous les thèmes, toutes les entrées (Admin compris) et des pages internes des applications, enregistre une
capture PNG de chaque écran et vérifie les textes (touche `U` : textes coupés, trop larges, sous le pied de page).

| Option | Rôle |
|---|---|
| `--port COM9` | port série |
| `--only Jeux,Social` | seulement ces thèmes (noms du menu, séparés par des virgules) |

Produit : **réécrit** `docs/screens/*.png`, `docs/screens/checks.txt`, [docs/fr/ecrans.md](ecrans.md) et
[docs/en/screens.md](../en/screens.md).

Précautions : le badge est redémarré plusieurs fois ; les interrupteurs des menus ne sont pas pressés (réglages
inchangés) ; les pages qui émettent d'elles-mêmes (porteuse, balise chaud-froid, échange de contacts, réglage radio)
émettent pendant la capture ; le mode admin est coupé à la fin du thème Admin. La carte SD attendue a les musiques
et les textes dans des dossiers et les vidéos à la racine de `VIDEOS`.

#### Tests à deux badges : `test_party_games.py`, `test_werewolf.py`, `test_smuggler.py`

Deux badges branchés en USB, `badge_menu` flashé, ports libres. Les deux badges sont redémarrés (sauf
`--no-reboot`), les boutons simulés, les traces des deux badges comparées ; captures dans un dossier daté. Le
déroulement de chaque test : [guide développeur § 9.3](guide_developpeur.md#93-tests-à-deux-badges).

| Script | Teste | Options propres |
|---|---|---|
| `test_party_games.py` | Tir à la corde et Assassin | `--ports P1 P2` (obligatoire), `--only tug,assassin`, `--out` (défaut `party_<date>`) |
| `test_werewolf.py` | Loup-garou : badge 1 meneur, badge 2 seul vrai joueur, des robots complètent | `--ports MENEUR JOUEUR` (obligatoire), `--without voleur,capitaine` (rôles retirés : `voyante`, `sorciere`, `chasseur`, `cupidon`, `petite-fille`, `capitaine`, `voleur`), `--out` (défaut `werewolf_<date>`) |
| `test_smuggler.py` | Contrebande : échange, cadeau, refus, annulation | `--ports INVITANT INVITÉ` (obligatoire), `--out` (défaut `smuggler_<date>`) |

Options communes : `--no-reboot`, `--verbose` / `-v`.

```bash
python tools/test_party_games.py --ports COM9 COM11 --only tug
python tools/test_werewolf.py --ports COM9 COM11 --without voleur
python tools/test_smuggler.py --ports COM9 COM11
```

Précautions :
- Assassin et Contrebande demandent des badges **collés** (signal très fort) : posez-les côte à côte. Le test de
  l'assassin affiche le RSSI mesuré, utile pour régler le seuil sur place ;
- `test_party_games.py` met le badge 1 en mode admin (pour jouer à 2), `test_werewolf.py` met le meneur en mode admin
  et règle Admin > Loup-garou (admin) sur « Test : robots » ; les deux remettent l'état d'origine à la fin. Si le
  script est interrompu, vérifiez ces réglages à la main ;
- `test_smuggler.py` **modifie la cale** des deux badges : à faire sur des badges de test.

#### `src/tests/host/run_tests.py` — tests sur PC, sans badge

Compile les modules C du firmware avec le compilateur du PC et vérifie leurs règles (jeux, décodeurs radio, vCard,
loup-garou, sonneries, livres-jeux...), puis les convertisseurs Python. Utile avant de fabriquer un firmware, ou pour
vérifier les fichiers de `docs/sd/` (les tests `rtttl` et `gamebook` les relisent).

| Option | Rôle |
|---|---|
| `noms...` | tests à lancer (défaut : tous) : `gfx`, `ir`, `games`, `puzzles`, `score`, `crypto`, `ookdec`, `vcard`, `werewolf`, `rsvp`, `gamebook`, `rtttl`, `screen`, `party_games`, `smuggler`, `image2epi`, `video2epaper`, `audio2wav` |
| `--cc gcc` | compilateur C (défaut : `$CC`, sinon gcc, sinon clang) |
| `--keep` | garde les dossiers temporaires |
| `--verbose`, `-v` | affiche aussi la sortie des tests réussis |

```bash
python src/tests/host/run_tests.py
python src/tests/host/run_tests.py rtttl gamebook -v
```

Sans compilateur C, les tests C sont SKIP ; sans ffmpeg, `video2epaper` et `audio2wav` aussi.

### 2.2 Préparer les médias

Le format de chaque dossier de la carte : [§ 6](#6-la-carte-sd).

#### `src/images/image2epi.py` — images → `.epi`

N'importe quelle image (JPEG, PNG...) devient une image 200 × 200 en 4 gris pour `IMAGES/`. Prérequis : Pillow.

| Option | Rôle |
|---|---|
| `images...` | fichiers source (plusieurs permis) |
| `-o`, `--output-dir` | dossier de sortie (défaut : dossier courant) ; un `.epi` par image, même nom |
| `--fit` | image entière avec des bandes blanches (défaut : recadrée au centre) |
| `--bw` | noir et blanc pur (1 bit par pixel) |
| `--contrast P` | pourcentage de pixels sombres / clairs écrêtés pour étirer le contraste (défaut 1) |
| `--equalize` | égalise l'histogramme à la place (images ternes) |
| `--preview` | écrit aussi un aperçu `.png` à côté de chaque `.epi` |

```bash
python src/images/image2epi.py photos/*.jpg -o IMAGES --preview
```

#### `src/video/video2epaper.py` — vidéo → `.epv`

La vidéo devient 200 × 200 en noir et blanc, avec le son pour le buzzer. Prérequis : ffmpeg, numpy, Pillow.

| Option | Rôle |
|---|---|
| `video` | fichier source (tout ce que lit ffmpeg) |
| `-o`, `--output` | fichier de sortie (défaut `VIDEO.EPV`) : à copier dans `VIDEOS/` |
| `--fps 10\|20\|30` | images par seconde (défaut 10 : le meilleur contraste) |
| `--fit` | vidéo entière avec des bandes (défaut : recadrée) |
| `--dither bayer\|fs\|threshold` | tramage (défaut `bayer` : stable d'une image à l'autre, moins de traces ; `fs` plus fin sur les images fixes) |
| `--gamma G` | < 1 éclaircit, > 1 assombrit (défaut 1) |
| `--contrast C` | > 1 augmente le contraste (défaut 1,2) |
| `--start S`, `--duration D` | extrait : début et durée en secondes |
| `--preview fichier.gif` | aperçu animé |
| `--no-audio` | sans le son |

```bash
python src/video/video2epaper.py film.mp4 --start 60 --duration 30 -o VIDEOS/extrait.epv
```

#### `src/audio/audio2wav.py` — sons → `.wav` pour le buzzer

Convertit en WAV 8 bits mono 16 kHz (environ 1 Mo par minute), filtré et compressé pour être audible sur le buzzer.
Prérequis : ffmpeg.

| Option | Rôle |
|---|---|
| `entrées...` | fichiers, jokers (`"musiques/*.mp3"`, les guillemets comptent sous Windows) ou dossiers (leurs fichiers audio, sans les sous-dossiers) |
| `-o`, `--output-dir` | dossier de sortie (défaut : dossier courant) |
| `--rate R` | fréquence d'échantillonnage (défaut 16000) |
| `--no-filter` | garde toute la bande et la dynamique (moins audible sur le buzzer) |
| `--start S`, `--duration D` | extrait |

```bash
python src/audio/audio2wav.py "albums/*.mp3" -o MUSIQUE/Films
```

Un fichier qui ne se convertit pas n'arrête pas les autres ; la liste des échecs est affichée à la fin.

#### `tools/rtttl_sort.py` (et `tools/rtttl_lib.py`) — trier une collection de sonneries

Range une grande collection de sonneries (milliers de fichiers `.txt`, `.rtttl`, `.rtx`, `.bas`) pour
`SONNERIES/` : lecture comme le badge, doublons retirés (même mélodie, même transposée), sonneries invalides
écartées, un fichier par sonnerie nommé d'après son meilleur titre, classement par catégorie. Bibliothèque standard
seulement. `rtttl_lib.py` n'est pas un script : c'est le lecteur de sonneries (RTTTL et PICAXE) utilisé par
`rtttl_sort.py`, identique à celui du badge.

| Option | Rôle |
|---|---|
| `src` | dossier source (lu seulement) |
| `dst` | nouveau dossier, **qui ne doit pas exister** |
| `--categories fichier` | fichier `chemin#ligne<TAB>D\|S\|F\|A` (Dessins animés, Génériques de séries, Musiques de films, Autre) ; sans lui, tout va dans `Autre` |
| `--list fichier` | écrit les groupes de doublons (un par ligne : leurs noms de fichiers), pour préparer les catégories, puis s'arrête |
| `--split N` | une catégorie de plus de N sonneries est découpée par initiale (défaut 500) |

```bash
python tools/rtttl_sort.py F:/RTTTL_origine --list titres.tsv
python tools/rtttl_sort.py F:/RTTTL_origine F:/SONNERIES --categories categories.tsv
```

Produit : le dossier `dst` et `dst/rapport.tsv` (chaque fichier écrit, ses copies dans la source, les erreurs, les
fichiers vides). Format RTTTL et PICAXE : [sonneries.md](sonneries.md).

#### `tools/gamebook_check.py` — vérifier un livre-jeu

Lit un livre comme le badge, puis signale les erreurs (section absente ou en double, choix vers une section absente,
trop de sections, de choix ou d'objets) et les remarques (sections inaccessibles, sans fin possible, jet de dé non
couvert, objet testé jamais donné, texte trop long, caractère absent des polices). Bibliothèque standard ; lit
`src/gfx/gfx_fonts.c` du dépôt.

| Option | Rôle |
|---|---|
| `livres...` | fichiers `.txt` |
| `--pages` | taille et nombre de pages (8 lignes) de chaque section |
| `--play` | jouer le livre dans le terminal (`q` : quitter) |
| `--c FICHIER` | écrit le livre comme livre intégré au firmware (développeurs, un seul livre) |

```bash
python tools/gamebook_check.py LIVRES/mon_livre.txt --pages
```

Code de sortie 1 en cas d'erreur. Écrire un livre : [livres_jeux.md](livres_jeux.md).

### 2.3 Pendant l'événement

#### `tools/badge_remote.py` — l'écran du badge sur le PC

Affiche l'écran du badge en grand, mis à jour à chaque dessin, et le pilote au clavier ou à la souris ; journal du
badge, captures PNG. Prérequis : pyserial, Tkinter. Mode d'emploi : [guide utilisateur § 6](guide_utilisateur.md#6-piloter-le-badge-depuis-un-ordinateur).

| Option | Rôle |
|---|---|
| `--port`, `-p` | port série (défaut : le premier badge ; sinon la liste « Badge : » de la fenêtre) |
| `--zoom`, `-z` | zoom de l'écran, 2 à 4 (défaut : d'après la taille de l'écran du PC) |
| `--snapshot`, `-s` `fichier.png` | enregistre l'écran actuel et quitte, sans fenêtre |

Clavier (la fenêtre a le focus) : flèche haut / `y` = flanc gauche, bas / `x` = flanc droit, gauche / `a` = aile
gauche, droite / Entrée / `b` = aile droite ; Maj + touche = appui long ; F5 : redemander l'écran ; F12 : capture
(`badge_AAAAMMJJ_HHMMSS.png` dans le dossier courant). Boutons « Capture », « Rafraîchir », « Diagnostic » (`!`),
cases « Journal », « Mode clavier (saisie de texte) » et « Mode admin ».

- **Mode clavier** : le texte tapé va dans l'éditeur ouvert sur le badge (nom, carte de visite, réponses, annonces),
  y compris les lettres accentuées é è ê à â ç ô î ù û ë ï É È À Ç ; Entrée : valider, Échap : annuler,
  Retour arrière : effacer. Les flèches restent les boutons. Bien plus rapide que les 4 boutons pour écrire les
  annonces.
- **Mode admin** : active / désactive le mode admin sans la séquence secrète (discret devant le public).

```bash
python tools/badge_remote.py --port COM9 --zoom 3
python tools/badge_remote.py --snapshot ecran.png
```

#### `tools/contacts_export.py` — exporter les cartes de visite reçues

Envoie `k` au badge et récupère les cartes reçues (Social > Contacts) dans un fichier vCard à importer dans un
téléphone ou un carnet d'adresses. Prérequis : pyserial, port libre.

| Option | Rôle |
|---|---|
| `--port COM9` | port série |
| `-o`, `--output` | fichier de sortie (défaut `contacts.vcf`, écrasé s'il existe) |

```bash
python tools/contacts_export.py -o mes_contacts.vcf
```

Les compétences des cartes reçues partent dans la ligne `CATEGORIES`.

#### `tools/battery_log.py` — test d'autonomie de la batterie

Un badge calibré automatiquement (Admin > Batterie (auto)) envoie aussi son estimation : colonne `percent_est` du CSV,
« ~72 % (auto) » à l'écran. Un premier test a montré que l'ADC reste presque plat (2530) pendant 13 h puis chute
pendant les 3 dernières heures (autonomie ~16 h avec les balises toutes les 2 s) : la tension ne donne pas le reste.
Le calibrage automatique mesure donc l'autonomie (minutes sur batterie depuis une charge complète, enregistrées toutes
les 10 min, jusqu'à ce que le badge s'éteigne seul ; validée au démarrage qui suit une vraie coupure, pas un
redémarrage logiciel, après 60 min au moins) ; l'estimation est 100 % moins le temps écoulé depuis la dernière charge
complète, 10 % au plus quand l'ADC est tombé de 100 pas sous son niveau au débranchement. Éteindre le badge à
l'interrupteur pendant la décharge fausserait l'autonomie.

Le badge testé tourne sur sa batterie, loin de l'USB, avec Réglages > Batterie par radio : oui (ses balises portent sa
batterie toutes les 2 s). Un autre badge branché au PC les entend et écrit `battery: <id> <nom> raw <ADC> mv <mV>
usb <0|1> rssi <dBm>` ; le script garde un relevé par badge toutes les `--interval` secondes dans un CSV (heure,
minutes écoulées, id, nom, ADC brut, mV, mV calibrés, USB, RSSI). Prérequis : `pyserial`.

| Option | Rôle |
|---|---|
| `--port COMx` | port du badge qui écoute (trouvé tout seul) |
| `--name NOM` | seulement ce badge (son nom ou le début de son id) |
| `--interval S` | secondes entre deux relevés d'un badge (défaut 60) |
| `--cal raw:mV,raw:mV` | deux points de calibration du badge testé : convertit l'ADC brut en mV |
| `-o FICHIER.csv` | fichier (défaut `battery_<date>.csv`, complété s'il existe) |

```bash
python tools/battery_log.py --port COM11 --name Tristan --interval 300
```

Un badge non calibré envoie 0 mV : la courbe de l'ADC brut reste exploitable (et `--cal` la convertit après une
calibration). Le badge qui n'est plus entendu a épuisé sa batterie : le dernier relevé donne l'autonomie.

#### `tools/score_check.py` — vérifier les scores et faire un classement

Les jeux Morpion, Puissance 4, Simon, Réflexes et Snake affichent en fin de partie (ou en appui long sur le jeu dans
le menu, pour le record) un QR code signé : `HIP26:<jeu>:<score>:<id du badge>:<nom>:<signature>`. Scannez-les avec
un téléphone, collez les textes dans un fichier, une ligne par QR code, puis lancez le script : il refuse les scores
modifiés à la main et classe chaque jeu (meilleur score de chaque badge ; un QR code scanné deux fois compte une
fois). Bibliothèque standard.

| Option | Rôle |
|---|---|
| `entrées...` | textes de QR codes, ou fichiers (une ligne par texte) ; `-` ou rien : l'entrée standard |
| `--key HEX` | la clé de 128 bits (32 chiffres hexadécimaux) ; défaut : variable `BADGE_SCORE_KEY` |
| `--make-key` | génère une nouvelle clé et sa table masquée à recopier dans `src/menu/score_code.c` (développeurs : un nouveau firmware est nécessaire) |

```bash
set BADGE_SCORE_KEY=0123...      (Windows ; export BADGE_SCORE_KEY=... sous Linux / macOS)
python tools/score_check.py scores.txt
```

Produit : le classement par jeu, les lignes `INVALID` ; code de sortie 1 s'il y a un texte invalide. La clé n'est
pas dans le dépôt : elle est gardée par les organisateurs. Les casse-têtes n'ont pas de QR code.

### 2.4 Contenu du CTF et des jeux (générateurs)

Ces scripts réécrivent des fichiers source du firmware : le résultat ne compte sur les badges qu'après une
recompilation et un flash ([§ 7](#7-flasher-et-mettre-à-jour-le-firmware)).

| Script | Rôle | Options | Produit |
|---|---|---|---|
| `tools/crypto_ctf_make.py` | les 13 défis crypto (Jeux solo > Défis crypto) : textes en clair, chiffrés par le script, vérifiés à la largeur de l'écran | sans option : affiche la table C ; `--update` : la remplace dans `src/menu/crypto_ctf.c` ; `--answers` : affiche les réponses et le flag final | la table C, ou les solutions |
| `tools/skills_icons.py` | pictogrammes 16 × 16 des 20 compétences, dessinés en ASCII dans le script | `--preview icones.png` (Pillow) | réécrit `src/menu/skills_icons.h` à chaque lancement |
| `tools/smuggler_icons.py` | icônes 32 × 32 des marchandises de la contrebande | `--png planche.png` (planche, au lieu d'écrire le C) ; `--check` (code 1 si le C n'est pas à jour) | réécrit les icônes de `src/menu/smuggler_goods.c` |
| `tools/werewolf_icons.py` | illustrations 32 × 32 des cartes du loup-garou | `--png planche.png` ; `--check` | réécrit les icônes de `src/menu/werewolf_cards.c` |

Précautions :
- **`crypto_ctf_make.py` est le fichier des solutions** (spoilers) : `--answers` affiche toutes les réponses et le
  flag ; ne pas le projeter, ne pas le lancer devant les participants ;
- `skills_icons.py` réécrit son fichier même sans changement : sans intérêt hors développement ;
- `--check` et `--png` n'écrivent rien dans les sources.

### 2.5 Flipper Zero et radio

#### `tools/flipper/*.sub` — les télécommandes prêtes

Deux fichiers Princeton 24 bits (te = 400 µs, préréglage `FuriHalSubGhzPresetOok650Async`, 433,92 MHz) à copier
dans le dossier `subghz` de la carte SD du Flipper, puis Sub-GHz > Saved > le fichier ; chaque bouton du Flipper
envoie une commande :

| Bouton du Flipper | `SecSea_general.sub` (`C1 6A 01`) | `SecSea_talk.sub` (`C1 6A 11`) |
|---|---|---|
| OK | la cigale chante (0x01) | talk : vert (0x11) |
| Haut | mode muet (0x02) | talk : orange, 5 min (0x12) |
| Bas | fin du mode muet (0x04 → 0x03) | talk : rouge énervé (0x14) |
| Droite | — | talk : rouge, fini (0x1F → 0x13) |
| Gauche | — | talk : éteint (0x18 → 0x10) |

Maintenez le bouton une seconde. Les commandes : [§ 5](#5-télécommande-mode-muet-sommeil-et-mode-démo).

Les mêmes commandes en **RAW** (`tools/flipper/raw/`, un fichier par commande, 10 trames Princeton, 0,5 s) : pour un
Flipper qui n'a pas le protocole Princeton, ou pour rejouer exactement le même signal. Sub-GHz > Saved > le fichier >
Send ; si une cigale ne réagit pas, renvoyez-le (son récepteur alterne entre l'OOK et le réseau).

| Fichier | Code | Effet |
|---|---|---|
| `SecSea_raw_chante.sub` | `C1 6A 01` | la cigale chante |
| `SecSea_raw_muet.sub` | `C1 6A 02` | mode muet |
| `SecSea_raw_fin_muet.sub` | `C1 6A 04` | fin du mode muet |
| `SecSea_raw_talk_vert.sub` | `C1 6A 11` | talk : vert |
| `SecSea_raw_talk_orange.sub` | `C1 6A 12` | talk : orange, 5 min |
| `SecSea_raw_talk_rouge_enerve.sub` | `C1 6A 14` | talk : rouge énervé |
| `SecSea_raw_talk_rouge_fini.sub` | `C1 6A 1F` | talk : rouge, fini |
| `SecSea_raw_talk_eteint.sub` | `C1 6A 18` | talk : éteint |

Régénérés par `python tools/ook_sub.py princeton 0xC16A02 -o SecSea_raw_muet.sub` (`--repeats N` pour un signal
plus long).

Quatre fichiers RAW (paquets GFSK du réseau des badges, `NET_LEDS`, générés par `flipper_net_sub.py leds`) pilotent
les LEDs des cigales à portée, sans relais (TTL 0) : pour les **essais de portée**. Même dossier, Sub-GHz > Saved >
le fichier > Send (Envoyer) :

| Fichier | Effet |
|---|---|
| `SecSea_leds_vert.sub` | LEDs vertes (50 %) |
| `SecSea_leds_rouge.sub` | LEDs rouges (50 %) |
| `SecSea_leds_retablir.sub` | retour à l'animation habituelle du badge |
| `SecSea_leds_vert_rouge_boucle.sub` | **vert, rouge, vert... toutes les secondes, pendant 2 min 30** (150 ordres) |

Essai de portée avec la boucle : lancez-la, éloignez-vous avec le Flipper ; tant qu'une cigale alterne vert / rouge
chaque seconde, elle reçoit ; si elle saute des couleurs, elle est en limite de portée. Rejouez le fichier pour
continuer, puis `SecSea_leds_retablir.sub`. Pour une autre durée, période ou couleur :

```bash
python tools/flipper_net_sub.py leds vert rouge --period 1 --count 600 --ttl 0 -o boucle_10min.sub
python tools/flipper_net_sub.py leds bleu --level 100 --ttl 0 --send    # envoyé tout de suite par l'USB
```

Une cigale en mode économie (batterie faible) garde ses LEDs éteintes.

#### `tools/ook_sub.py` — fichiers `.sub` OOK (télécommandes, sondes météo)

Écrit des fichiers Sub-GHz RAW (préréglage `FuriHalSubGhzPresetOok650Async`) avec les chronogrammes que décode le
badge : pour piloter les badges avec une commande non prévue par les deux fichiers ci-dessus, ou tester le Décodeur
433 MHz, la Station météo et la Chasse 433 MHz. Bibliothèque standard.

| Sous-commande | Arguments et options |
|---|---|
| `princeton CODE` | code 24 bits (`0xC16A30`...) ; `--te µs` (défaut 400) ; `--repeats N` (défaut 10 trames) ; `-o fichier` (défaut `princeton_<CODE>.sub`) |
| `came CODE`, `nice CODE` | `--bits 12\|24` (défaut 12) ; `--repeats N` (défaut 8) ; `-o fichier` (défaut `came_<CODE>.sub`, `nice_<CODE>.sub`) |
| `weather` | `--temp °C` (obligatoire), `--hum %` (défaut 50), `--channel N` (défaut 1), `--id N` (défaut 0x5A ; 14 bits pour Acurite), `--battery-low`, `--protocol all\|nexus\|thermopro_tx4\|gt_wt02\|infactory\|lacrosse_tx141thbv2\|acurite_592txr` (défaut `all` : un fichier `ook_<protocole>.sub` par sonde), `--repeats N`, `-o dossier` (défaut : dossier courant) |
| `check FICHIERS...` | vérifie la forme de fichiers RAW (pas les fichiers « Key » comme `tools/flipper/*.sub`) |
| `selftest` | génère tout dans un dossier temporaire et le vérifie |

```bash
python tools/ook_sub.py princeton 0xC16A30 -o choeur_frere_jacques.sub    # lance le morceau 0 du choeur
python tools/ook_sub.py weather --temp 21.5 --hum 45 --channel 2 -o meteo
```

Un canal hors de la plage d'une sonde la fait sauter ; une température hors de sa plage est écrite avec un
avertissement (le badge la refusera).

#### `tools/flipper_net_sub.py` — paquets du réseau des cigales

Écrit des fichiers RAW avec un préréglage GFSK personnalisé : le Flipper émet alors de vrais paquets du réseau des
badges. Affiche aussi le préréglage « SecSea » qui permet au Flipper d'enregistrer les paquets d'un badge.
Bibliothèque standard.

| Argument / option | Rôle |
|---|---|
| `command CMD` | une commande à distance (`NET_COMMAND`), par exemple `0x02` |
| `ping` | un ping (les badges l'écrivent sur leur port série) |
| `raw TYPE OCTETS...` | un paquet quelconque : le type, puis les octets de données en hexadécimal |
| `leds COULEUR...` | les **LEDs des cigales** (`NET_LEDS`, comme Admin > LEDs des cigales) : une couleur (`vert`, `rouge`, `orange`, `jaune`, `cyan`, `bleu`, `violet`, `rose`, `blanc`, ou `RRGGBB`), ou plusieurs en boucle, une toutes les `--period` s ; `eteint` : rétablir l'animation du badge |
| `pirates [N]` | un **réseau de N cigales proches** (6 par défaut, 16 au plus) aux noms de pirates (Rackham, Barbossa, AnneBony, Surcouf, La Buse...), chacune avec un score, des compétences et un niveau tirés au hasard, qui envoient leurs balises (`NET_BEACON`) `--repeats` fois : pour tester la réception et le décodage des balises (Social > Radar, ou `!` sur la console du badge) |
| `preset` | affiche les lignes à ajouter au fichier `subghz/assets/setting_user` de la carte SD du Flipper |
| `--id N` | identifiant de l'émetteur, 4 octets (défaut `0x5EC5EA26`) |
| `--repeats N` | paquets dans le fichier (défaut 8) |
| `--gap MS` | millisecondes entre deux paquets (défaut 250) |
| `--ttl N` | `command`, `leds` : sauts du relais par les cigales, 0 à 4 (défaut 2, 0 : pas de relais, pour les essais de portée) |
| `--period S` | `leds` : secondes entre deux couleurs (défaut 1) |
| `--count N` | `leds` : ordres dans le fichier (défaut 1 ; 150 = 2 min 30 à 1 s) |
| `--level N` | `leds` : luminosité, 1 à 100 % (défaut 50) |
| `-o`, `--output` | fichier de sortie (défaut `secsea_<type>.sub`) |
| `--power DBM` | puissance du Flipper : 10 (défaut, comme les badges), 7, 5, 0, -10, -15, -20, -30 |
| `--send` | copie le fichier sur le Flipper branché en USB et l'envoie (sa console série, comme `flipper_weather.py`) |
| `--port COMx` | avec `--send` : le port du Flipper (trouvé tout seul) |

```bash
python tools/flipper_net_sub.py command 0x02 -o muet_gfsk.sub
python tools/flipper_net_sub.py pirates 8 --repeats 6 --power -10 --send
python tools/flipper_net_sub.py preset
python tools/flipper_net_sub.py leds vert rouge --period 1 --count 150 --ttl 0 -o leds_boucle.sub
```

Précautions :
- `pirates` : près du Flipper (balises reçues au-dessus de -80 dBm, 3 fois en 10 s), les badges comptent chaque
  pirate comme une **rencontre** (+10 points, gardée en mémoire). À -10 dBm, les balises arrivent vers -85 dBm à un
  mètre : listées, sans rencontre ;
- **une commande `0x05` (mise en sommeil) envoyée ainsi endort réellement les badges à portée** : contrairement au
  code Princeton, le paquet réseau est obéi. Ne générez pas ce fichier hors d'un essai contrôlé ;
- le nonce de la commande est tiré une fois par fichier : rejouer le même fichier dans les 10 s n'a pas d'effet,
  au-delà la commande est exécutée à nouveau ;
- envoyer un fichier depuis le PC par la ligne de commande du Flipper et enregistrer les paquets d'un badge :
  [guide développeur § 6.19](guide_developpeur.md#619-flipper-zero).

#### `tools/flipper_weather.py` — la météo sur tous les badges, par un Flipper

Récupère les prévisions d'une ville sur [Open-Meteo](https://open-meteo.com) (gratuit, sans clé), en fait une
**annonce** des badges (l'heure, un texte court, un QR code avec les jours suivants), puis l'envoie avec un Flipper Zero
branché en USB : les fichiers `.sub` sont copiés sur sa carte SD par sa console série, puis émis (`subghz
tx_from_file`). Tous les badges à portée l'affichent comme une annonce (Social > Annonces la garde).
Prérequis : `pyserial`, Internet ; le Flipper sur son écran principal (pas d'application Sub-GHz ouverte), pas
utilisé par qFlipper.

| Argument / option | Rôle |
|---|---|
| `VILLE` | la ville (`"La Ciotat"`, `"Marseille"`...) |
| `--lang fr\|en` | langue du texte de l'annonce (défaut `fr`) |
| `--days N` | jours dans le QR code, à partir de demain (défaut 4) |
| `--rounds N` | nombre d'envois de chaque partie (défaut 4) |
| `--port COMx` | port du Flipper (trouvé tout seul) |
| `-o FICHIER.sub` | nom des fichiers écrits (défaut `build/flipper/secsea_meteo_N.sub`) |
| `--no-send` | écrit seulement les fichiers (à envoyer à la main : Sub-GHz > Saved) |
| `--dry-run` | affiche seulement les prévisions et le texte |
| `--id N` | identifiant de l'émetteur (défaut `0x5EC5EA27`) |
| `--ttl N` | sauts du relais par les cigales, 0 à 4 (défaut 2 ; 0 : ancien format, pour des badges d'un firmware plus ancien) |

```bash
python tools/flipper_weather.py "La Ciotat"
python tools/flipper_weather.py Marseille --lang en --port COM10
python tools/flipper_weather.py "La Ciotat" --dry-run
```

Exemple d'annonce : « Météo La Ciotat : 23C, couvert, vent 13 km/h. Demain 21-26C, bruine, pluie 8 %. », QR code
« dim 21-26C bruine / lun 21-26C bruine / mar 21-27C bruine ». Le symbole ° n'existe pas dans les polices du badge.

Précautions :
- l'annonce fait 3 paquets ; le Flipper émet mal les longs fichiers RAW (les longs paquets se perdent), d'où un petit
  fichier par partie, chacun envoyé plusieurs fois : compter une vingtaine de secondes ;
- elle s'affiche sur **tous** les badges à portée (comme une annonce des organisateurs) : à utiliser avec mesure.

### 2.6 Aides au développement

| Script | Rôle | Options |
|---|---|---|
| `src/image2epaper.py` | convertit une image de 2 ou 4 couleurs (ou une animation) en tableaux C pour le firmware ; appelé par CMake à la compilation, rarement à la main. Pillow | `image` ; `-o`, `--output` (défaut : sortie standard) ; `-b`, `--back-color 00\|01\|10\|11` (couleur de remplissage si la largeur n'est pas un multiple de 8, défaut `11` = blanc) |
| `src/gfx/gen_fonts.py` | génère `gfx_fonts.c` (polices 14, 18 et 26 pixels, Aileron fournie avec Pillow, accents français composés) sur la sortie standard | `--preview image.png` : planche des glyphes |

```bash
python src/gfx/gen_fonts.py --preview polices.png > src/gfx/gfx_fonts.c
```

Le fichier `gfx_fonts.c` est dans le dépôt : ne le régénérez que pour changer les polices.


## 3. La console série USB

### 3.1 Se connecter

N'importe quel terminal série, 115 200 bauds (la vitesse n'a pas d'importance en USB), 8N1, sans contrôle de flux :

```bash
python -m serial.tools.miniterm COM9 115200        # fourni avec pyserial
picocom -b 115200 /dev/ttyACM0                      # Linux
screen /dev/cu.usbmodem1101 115200                  # macOS
```

PuTTY (Windows, type « Serial ») convient aussi. Chaque caractère est une commande, **sans Entrée** ; les retours
à la ligne et les caractères inconnus sont ignorés. Désactivez l'écho local. Le badge répond par des lignes de texte.

### 3.2 Les commandes

| Touche | Effet |
|---|---|
| `a` `b` `x` `y` | appui court : aile gauche, aile droite, flanc droit, flanc gauche |
| `A` `B` `X` `Y` | appui long sur le même bouton |
| `!` | diagnostic : OLED et IR, réseau des cigales (nom, score, rencontres, balises émises / reçues, voisins et leur dBm), flags du CTF, compteurs du réseau (`net: sent, received, dropped`), état du réseau et de la radio (`net state:`), écoute des télécommandes (`remote state:`), télécommande activée / mode muet / mode admin (`remote:`), `version:`, radio (version du CC1101, quartz utilisé et mesuré), batterie (mV, %, valeur ADC, ou « not calibrated ») et points de calibration usine |
| `?` | état du son : ouvert, échantillons joués, volume, position de la musique |
| `i` | test infrarouge : décode une trame NEC fabriquée puis l'émet (environ 68 ms) |
| `o` | état du récepteur OOK : actif, impulsions, trames, RSSI, MARCSTATE, GDO0 |
| `p` | durées du dernier signal reçu par le décodeur OOK |
| `O` | trace des tentatives de décodage OOK : marche / arrêt |
| `k` | export des cartes de visite reçues en vCard (utilisé par `contacts_export.py`) |
| `V` | trace de chaque paquet réseau émis et reçu : marche / arrêt |
| `r` | registres du CC1101 et PATABLE |
| `M` | envoie le « Radio : message » (lisible par `subghz chat 433920000 0` sur un Flipper) |
| `P` | ping à +10 dBm : les badges qui l'entendent écrivent `net: ping #n from <id>, rssi ...` |
| `L` | mode *loopback* (les paquets émis reviennent comme d'un badge jumeau, pour tester seul) : marche / arrêt |
| `W` | batterie faible simulée (mode économie : muet, télécommande coupée, alerte, icône) : marche / arrêt |
| `R` | redémarrage immédiat du badge |
| `[` / `]` | envoi de l'écran à chaque changement : marche / arrêt (lignes `@FB ...`) |
| `s` | envoi de l'écran une fois |
| `U` | vérification des textes : trace `uicheck: ...` pour chaque texte coupé, trop large ou sous le pied de page : marche / arrêt |
| Ctrl+A (0x01) puis `A` / `a` | mode admin : marche / arrêt (réponse `admin: on` / `admin: off`) |
| Ctrl+B (0x02) puis un caractère | tape ce caractère dans l'éditeur de texte ouvert (`\r` : terminé, Échap : annuler, `\b` : effacer ; 0x80 à 0x8F : les 16 lettres accentuées de l'éditeur) |

Les touches marchent aussi sur la page SOMMEIL ([§ 5.3](#53-mise-en-sommeil-et-réveil-manuel)). Quelques terminaux
interceptent Ctrl+A (picocom, screen) : pour le mode admin, préférez la case de `badge_remote.py`.

Exemples :
- ouvrir le menu Admin sans la séquence : Ctrl+A puis `A` ;
- la séquence secrète au clavier, depuis la liste des thèmes : `yyxxyxyx` ;
- vérifier un badge : `!`, puis `P` sur un badge et lire la ligne `net: ping` sur un autre.

L'écran est envoyé sous la forme `@FB <BW|4G|WHITE|BLACK> [<plan lsb en base64> [<plan msb en base64>]]` :
`badge_remote.py` le dessine. Le protocole complet : [guide développeur § 8](guide_developpeur.md#8-le-protocole-usb-série).

### 3.3 Les lignes utiles du journal

| Ligne | Sens |
|---|---|
| `version: 1.0.0 (5d95da4 2026-09-30)` | version, commit et date du firmware (au démarrage et avec `!`) ; un `+` après le commit : sources modifiées |
| `ui: <titre>` | une nouvelle page s'affiche (le nom de l'application à son ouverture) |
| `notify: <texte>` | une notification en bas de l'écran |
| `admin: on` / `admin: off`, `admin: sending command 0x02`, `admin: badge type Staff` | mode admin, commande radio envoyée, type du badge |
| `remote: command 0x02 from <id \| Princeton \| this badge>`, `remote: muted` | commande à distance exécutée, mode muet |
| `sleep: requested, rebooting`, `sleep: on (...)`, `sleep: unlock 3/5 0/5`, `sleep: off, rebooting` | mise en sommeil et déblocage |
| `tune: crystal ... Hz, noise ... dBm (remotes above ... dBm), frequency offset a -> b (n packets, mean m)` | résultat du réglage radio |
| `browser: /MUSIQUE, 2 dir(s), 5 file(s): ...` | contenu d'un dossier ouvert par un lecteur |
| `video: playing ...`, `video: mount failed ...`, `music: end at 182s of 182s`, `music: stopped at ...` | lecture des vidéos et des musiques |
| `image: <fichier> (2 bit(s) per pixel)`, `saver: on (...)`, `saver: off` | images, veille |
| `rtttl: /SONNERIES: 3 dir(s), 12 file(s), 40 tune(s)`, `rtttl: playing "..."`, `rtttl: <fichier> line 7: error 5 (...) at column 12` | sonneries, erreurs de format |
| `gamebook: 3 books (2 on the SD card)`, `gamebook: section 12 missing` | livres-jeux |
| `battery: point 1 set, ADC raw ... = ... mV`, `store: factory settings saved (ok)` | calibration de la batterie |
| `store: saved (ok)`, `store: initialized` | réglages écrits en flash ; flash réinitialisée (premier démarrage, nouveau format) |
| `net: ping #n from <id>, rssi ...` | ping reçu |
| `achievement: <nom> (+XP, level n)` | succès obtenu |
| `announce: sending ...`, `announce: received ...`, `vote: ...`, `hotcold: ...`, `infection: ...`, `reset: <quoi>`, `demo: ...` | fonctions admin |

D'autres préfixes existent (`social:`, `contacts:`, `chorus:`, `party:`, `werewolf:`, `smuggler:`, `pirate:`...) :
voir le [guide développeur § 8](guide_developpeur.md#8-le-protocole-usb-série).


## 4. Le menu Admin

### 4.1 L'ouvrir et le quitter

- **Séquence secrète** : dans le menu principal (la liste des thèmes, pas dans un thème), tapez sur les flancs
  **gauche, gauche, droit, droit, gauche, droit, gauche, droit** en moins de 8 secondes. « Mode admin activé »
  s'affiche et le thème **Admin** apparaît en dernier, déjà sélectionné.
- **Par l'USB** : la case « Mode admin » de `badge_remote.py`, ou Ctrl+A puis `A` dans un terminal.
- Le mode admin est **gardé après extinction** et après une mise à jour du firmware.
- Pour le quitter : dernière entrée « Quitter le mode admin », ou Ctrl+A puis `a`.

Le mode admin permet aussi de lancer l'Assassin à 2 joueurs (pour les essais).

### 4.2 Les entrées

Dans l'ordre du menu :

| Entrée | Rôle | Boutons |
|---|---|---|
| **Commandes radio** | envoie une commande à tous les badges autour : Muet (conférence), Fin du mode muet, Cigale : chanter, Talk : éteint / vert / orange (5 min) / rouge (fini) / rouge énervé, Mise en sommeil. Ce badge l'exécute aussi (sauf le sommeil) | flancs : choisir ; D : envoyer ; G : retour |
| **LEDs des cigales** | couleur (9 couleurs ou R, G, B de 0 à 255) et mode (Fixe, Clignotant, Fondu, durées de 50 ms à 5 s) imposés aux LEDs des cigales autour, jusqu'à « Rétablir leurs LEDs » ou leur redémarrage | voir le [guide utilisateur § 4.8](guide_utilisateur.md#48-le-mode-admin-organisateurs) |
| **Annonces (admin)** | 6 annonces (heure, texte de 56 caractères, QR code : lien, texte, téléphone, SMS, e-mail, Wi-Fi, position GPS), gardées en flash ; aperçu ; envoi 3 fois à toutes les cigales | flancs : choisir ; D : ouvrir / modifier ; G : retour |
| **Vote (admin)** | ouvre une question prédéfinie, compte les votes (un par badge, le dernier compte) et affiche l'histogramme | D : ouvrir le vote, puis D : fermer |
| **Choeur : lancer** | lance « Frère Jacques » (4 voix) ou « L'Ode à la joie » (3 voix) ; ce badge chante la première voix | D : lancer / arrêter |
| **Balise chaud-froid** | ce badge émet une balise par seconde à +10 dBm ; les autres le cherchent avec Social > Chaud - froid. L'échelle (« Brûlant dès -74 dBm » par défaut, réglable par pas de 5 dB) est gardée et envoyée avec la balise | D : émettre / arrêter ; flancs : échelle ; G : quitter |
| **Virus : patient zéro** | infecte ce badge pour lancer l'épidémie | D : infecter ; flanc gauche : guérir ce badge |
| **Contrebande (admin)** | ajoute une marchandise au choix à la cale de ce badge ([contrebande.md](contrebande.md)) | flancs : choisir ; D : ajouter |
| **Loup-garou (admin)** | parties de moins de 8 joueurs : « 8 joueurs minimum » (défaut), « Petites parties (4+) », « Test : robots » ; gardé en flash ([loup_garou.md](loup_garou.md)) | flancs : choisir ; D : valider |
| **Remise à zéro** | efface une partie de la progression de ce badge (voir § 4.4) | D, puis D long : confirmer ; G : non |
| **Batterie (calibration)** | calibre la mesure de la batterie (voir § 4.3) | |
| **Radio pirate** | émet une mélodie, une tonalité de 1 kHz ou un WAV de la carte SD en FM sur 433 MHz ([radio_pirate.md](radio_pirate.md)) ; faible puissance, essais courts | |
| **Mode démo** | le badge présente ses fonctions en boucle (voir § 5.4) | D : lancer |
| **Type du badge** | Participant, Orateur ou Staff, affiché dans le bandeau du badge nominatif | flancs : choisir ; D : enregistrer |
| **Quitter le mode admin** | cache le thème Admin | |

### 4.3 Calibrer la batterie

Sans calibration, le badge n'affiche aucun niveau de batterie (« non calibrée ») : il ne montre jamais une valeur
fausse. Une fois par badge, avec un multimètre :

1. Badge branché en USB (en charge), Admin > Batterie (calibration). La page montre la valeur ADC (rafraîchie toutes
   les 2 s), la mesure, et les points 1 et 2.
2. Mesurez la tension aux bornes de la batterie. Sur la ligne « Multimètre », réglez cette tension avec les ailes
   (G : −, D : +, par 10 mV ; maintenues : de plus en plus vite ; de 2,50 à 4,50 V).
3. Flanc droit jusqu'à « > Enregistrer le point », aile droite : « Il faut un 2e point ».
4. Débranchez, laissez la tension baisser quelques minutes (au moins 0,2 V d'écart, 150 pas d'ADC), mesurez,
   réglez et enregistrez : « Point enregistré », le niveau s'affiche.

Un nouveau point remplace le plus proche. « > Effacer » demande une confirmation (D encore). Sur la ligne
« Multimètre », l'aile gauche sert à régler : quittez par un appui long sur l'aile gauche, ou depuis une autre ligne.
Les points sont des **réglages usine** : gardés par la Remise à zéro (même « Tout ») et par les mises à jour.
Vérification par la console : `!` affiche `battery: factory points ...`.

### 4.4 Remise à zéro

À faire avant l'événement sur des badges qui ont servi aux essais. Chaque ligne demande une confirmation par un
**appui long sur l'aile droite** :

| Ligne | Efface |
|---|---|
| Scores sociaux | score et rencontres du réseau des cigales |
| Records des jeux | records des jeux et des casse-têtes |
| Défis CTF et crypto | flags du CTF, défis crypto résolus |
| Contacts reçus | cartes de visite reçues (pas votre carte) |
| Virus | état du virus (en forme) |
| Succès et niveau | succès et leurs compteurs |
| Contrebande | la cale (nouvelles marchandises à la prochaine ouverture) |
| Tout | tout ce qui précède, plus la progression du livre-jeu |
| Annonces d'origine | les 6 annonces admin reprennent leurs textes d'origine (pas compris dans « Tout ») |

Ne sont jamais effacés : le nom, la carte de visite, les réglages (veille, volume, télécommande, mode muet, mode
admin, type du badge, réglage radio) et la calibration de la batterie.

### 4.5 Préparer un lot de badges (liste de contrôle)

1. Flasher la même version partout ([§ 7](#7-flasher-et-mettre-à-jour-le-firmware)) ; vérifier Réglages > Infos.
2. Laisser le réglage radio du premier démarrage se faire **près d'autres badges allumés**.
3. Calibrer la batterie (§ 4.3), si un multimètre est disponible.
4. `python tools/badge_selftest.py --port ...` sur chaque badge (quelques minutes).
5. Admin > Type du badge (Orateur, Staff...), Admin > Remise à zéro > Tout, puis Quitter le mode admin.
6. Carte SD préparée et validée par `badge_media_test.py` (§ 6).


## 5. Télécommande, mode muet, sommeil et mode démo

### 5.1 Les commandes à distance

Une commande arrive de deux façons :
- **d'un badge admin** (Admin > Commandes radio) : le badge l'envoie d'abord comme une télécommande Princeton
  (12 trames, ~0,6 s, pour les badges de talk qui n'écoutent qu'en OOK), puis 5 paquets du réseau des cigales en 2 s,
  à +10 dBm ;
- **d'une télécommande 433 MHz**, un Flipper Zero par exemple : un code Princeton 24 bits `0xC16Axx`, où `xx` est
  la commande. Le code doit être reçu deux fois : maintenez le bouton une seconde.

Une commande reçue par les deux voies, ou répétée, n'est exécutée qu'une fois. Le badge affiche la commande en bas
de l'écran et écrit `remote: command 0x.. from ...`.

| Commande | Effet |
|---|---|
| `0x01` | la cigale chante 6 s (rien en mode muet) |
| `0x02` | mode muet |
| `0x03` | fin du mode muet |
| `0x10` à `0x14` | lumières du badge de talk, si sa page est ouverte : éteint, vert, orange (5 min), rouge (fini), rouge énervé |
| `0x30` + n | lance le morceau n du choeur (`0x30` Frère Jacques, `0x31` L'Ode à la joie) |
| `0x04`, `0x18`, `0x1F` (Princeton seulement) | boutons du Flipper : `0x03`, `0x10`, `0x13` (voir § 2.5) |

Le badge n'obéit pas si Réglages > Télécommande est à « non ». Autres commandes depuis un Flipper : un fichier
généré par `ook_sub.py princeton 0xC16Axx` (§ 2.5), ou Sub-GHz > Add Manually > Princeton_433 puis modifier la ligne
`Key:` du fichier enregistré. N'utilisez pas `subghz tx` en ligne de commande : il remplace les 4 derniers bits
du code par 6.

Comment le badge écoute : il écoute le réseau des cigales en permanence ; quand il entend un émetteur qui n'est pas
un badge (au-dessus du bruit mesuré + 15 dB, ou −90 dBm sans réglage radio), il passe un instant à l'écoute des
télécommandes. Il ouvre aussi une écoute toutes les 10 s pour une télécommande faible. D'où l'intérêt du réglage
radio et de maintenir le bouton.

### 5.2 Le mode muet

- Coupe le son (buzzer, cigale, sonneries, choeur) et les LEDs. Les lecteurs continuent en silence.
- Activé par la commande `0x02`, désactivé par `0x03` ou par Réglages > Mode muet.
- **Gardé après extinction et après une mise à jour** : un badge qui reste muet a peut-être reçu la commande pendant
  un talk.
- Exception : la page du badge de talk garde ses LEDs, et l'état « STOP ! » fait chanter la cigale même en mode muet.

### 5.3 Mise en sommeil et réveil manuel

- Ordre : Admin > Commandes radio > Mise en sommeil, d'un badge admin. Il passe uniquement par le réseau des badges
  (pas en Princeton : un Flipper avec une simple télécommande ne peut pas endormir la conférence ; un fichier
  `flipper_net_sub.py command 0x05` le peut, voir § 2.5).
- Les badges qui le reçoivent enregistrent l'ordre et redémarrent : radio coupée, LEDs et son éteints, services
  arrêtés, page « SOMMEIL : le badge a été mis en sommeil par un admin... ». Ils ne répondent plus aux commandes
  radio, ni à un redémarrage, ni à une mise à jour du firmware : **l'état est gardé en flash**.
- Ne s'endorment pas : le badge admin qui envoie l'ordre, et un badge ouvert sur la page du badge de talk.
- **Réveil manuel** : **5 fois le flanc gauche, puis 5 fois le flanc droit**, avec moins de 5 s entre deux appuis
  (un autre bouton recommence la séquence). Le badge redémarre normalement. Par la console série : `yyyyyxxxxx`.
  Le journal suit la progression (`sleep: unlock 5/5 2/5`).

### 5.4 Le mode démo

Pour un stand : Admin > Mode démo, aile droite. Le badge enchaîne en boucle, LEDs en arc-en-ciel : badge nominatif
(8 s), succès (6 s), images de la carte SD (18 s, une toutes les 6 s), la première vidéo (20 s), compétences (5 s),
la première musique (10 s), démo de l'écran (15 s), programme (6 s), radar (6 s), livres-jeux (5 s), crédits (6 s),
infos (5 s). Les médias sautent leur étape sans carte SD ou sans fichier. La veille ne démarre pas ; **n'importe
quel bouton arrête la démo**. Elle n'est pas gardée après un redémarrage.

Conseils : badge branché en USB (la démo consomme), carte SD avec quelques images, une vidéo et une musique courtes
en tête de leurs dossiers (ordre alphabétique).


## 6. La carte SD

### 6.1 La carte

- Toute capacité (micro-SD, SDHC, SDXC), formatée en **FAT32** ou **exFAT**. Le badge ne formate jamais la carte
  et n'y écrit rien : elle peut être préparée sur un ordinateur et dupliquée.
- Noms de fichiers longs et accentués acceptés, **63 octets au plus** (en UTF-8, une lettre accentuée compte 2) :
  un nom plus long est ignoré. Les fichiers cachés, système et commençant par `.` (fichiers `._` de macOS) sont ignorés.
- Les extensions ne tiennent pas compte des majuscules (`.EPI` = `.epi`). Les listes sont triées par ordre alphabétique.

### 6.2 L'arborescence

```
carte SD
├── IMAGES/      images .epi (200×200, 4 gris)
├── VIDEOS/      vidéos .epv
├── MUSIQUE/     sons .wav, sous-dossiers permis
├── TEXTES/      textes .txt (lecture rapide)
├── SONNERIES/   sonneries .txt .rtttl .rtx .bas, sous-dossiers permis
├── RTTTL/       idem (au choix, ou les deux)
└── LIVRES/      livres-jeux .txt
```

| Dossier | Format | Limites et remarques | Préparer avec |
|---|---|---|---|
| `IMAGES` | `.epi` : 200 × 200, 1 ou 2 bits par pixel | 48 entrées par dossier (sous-dossiers + fichiers) dans la visionneuse ; 32 images proposées pour la veille ; 24 pour Envoyer une image | `image2epi.py` |
| `VIDEOS` | `.epv` : images 200 × 200 noir et blanc + son 8 bits | 48 entrées par dossier ; le mode démo et l'OLED prennent la première | `video2epaper.py` |
| `MUSIQUE` | `.wav` PCM 8, 16, 24 ou 32 bits, flottant 32 bits, mono ou stéréo, 4 à 192 kHz | 48 entrées par dossier ; blind test : 16 sous-dossiers et 64 morceaux au plus ; aussi la source des WAV de la radio pirate. Conseillé : 8 bits 16 kHz mono | `audio2wav.py` |
| `TEXTES` | `.txt` UTF-8 (avec ou sans BOM) ou Windows-1252 | 48 entrées par dossier ; la position de lecture est retenue | un éditeur de texte |
| `SONNERIES`, `RTTTL` | une sonnerie RTTTL par ligne (`#` : commentaire), ou commandes `tune` PICAXE (`.bas`) | 2048 caractères par ligne ; 40 sous-dossiers montrés par dossier ; fichiers en nombre quelconque, par pages de 32 ; 128 sonneries par page | `rtttl_sort.py`, [sonneries.md](sonneries.md) |
| `LIVRES` | livre-jeu `.txt` (sections `== n`, choix `-> n : ...`) | 16 livres ; 400 sections, 8 choix, 8 objets ; ~2 000 octets de texte par section | `gamebook_check.py`, [livres_jeux.md](livres_jeux.md) |

Un lecteur (Images, Vidéos, Musique, Lecture rapide) dont le dossier manque ou est vide affiche la **racine** de la
carte : une vidéo `VIDEO.EPV` (nom par défaut de `video2epaper.py`) copiée à la racine est donc lue aussi, mais
rangez plutôt chaque fichier dans son dossier. Au-delà de 48 entrées dans un dossier, une partie n'est pas
listée (les premières lues sur la carte sont gardées, puis triées) : faites des sous-dossiers.

### 6.3 Les exemples du dépôt

[`docs/sd/`](../sd) contient une carte d'exemple à copier à la racine :
- `docs/sd/SONNERIES/classique.txt` et `exemples.rtttl` : des sonneries du domaine public et des exemples du format ;
- `docs/sd/LIVRES/tresor_cigalon.txt` : « Le Trésor du capitaine Cigalon », le livre intégré au badge, modèle
  pour écrire le vôtre.

### 6.4 Préparer une carte pour l'événement

1. Formater en FAT32 (ou exFAT), créer les dossiers.
2. Convertir les médias (§ 2.2) ; vérifier les livres (`gamebook_check.py`) et les sonneries (le badge marque `(!)`
   les lignes invalides ; `rtttl_sort.py` les écarte).
3. Mettre en tête de liste (ordre alphabétique) les fichiers voulus pour le mode démo.
4. Insérer la carte dans un badge et lancer `python tools/badge_media_test.py` : il lit chaque vidéo et chaque
   musique et liste les échecs dans `media_report.txt`.
5. Dupliquer la carte.

> Respectez les droits d'auteur : utilisez des œuvres libres ou dont vous avez les droits.


## 7. Flasher et mettre à jour le firmware

### 7.1 Le fichier

Le firmware est un fichier `.uf2` : `badge_menu.uf2`, fourni par les organisateurs (releases du dépôt) ou compilé
(`build/src/menu/badge_menu.uf2`, voir le [guide développeur § 2](guide_developpeur.md#2-compiler-et-flasher)).

### 7.2 Par glisser-déposer (sans outil)

1. Interrupteur sur ON.
2. Maintenez le bouton de démarrage du RP2040 (« BOOTLOAD » / BOOTSEL) du badge, branchez le câble USB, relâchez
   après une ou deux secondes.
3. Un disque **RPI-RP2** apparaît : copiez-y le fichier `.uf2`.
4. Le badge redémarre tout seul sur le nouveau firmware (le disque disparaît).

### 7.3 Avec picotool

Badge allumé, branché et **port série libre** (fermez `badge_remote.py` et les terminaux) :

```bash
picotool load -f -x badge_menu.uf2
```

`-f` fait redémarrer le badge en mode flash, `-x` lance le firmware après l'écriture. Si picotool ne trouve pas le
badge, passez par le bouton (§ 7.2).

### 7.4 Après la mise à jour

- Vérifier la version : Réglages > Infos (« Version 1.0.0 (5d95da4) », date de compilation), ou `!` sur la console.
- Si la nouvelle version apporte le réglage radio, ou si les réglages ont été réinitialisés, la page Réglage radio
  s'ouvre seule au démarrage (environ 15 s) : laissez-la finir près d'autres badges allumés.

### 7.5 Ce qui survit à un flash

Les réglages sont dans les derniers secteurs de la flash, que le flash d'un firmware n'écrit pas :

| Données | Après un flash normal |
|---|---|
| Nom, scores, rencontres, records, flags, succès, cale, livre-jeu en cours, réglages (veille, volume, lampe, télécommande, **mode muet**, **mode admin**, type du badge, **sommeil**), réglage radio (quartz, bruit, correction de fréquence) | gardés, sauf si la nouvelle version change le format du stockage principal : tout est alors remis à zéro (`store: initialized`) et le réglage radio recommence |
| Cartes de visite (la vôtre et les reçues), annonces admin | gardées, sauf changement de format de ce second stockage (effacées) |
| Calibration de la batterie (réglage usine) | toujours gardée |
| Tout | effacé seulement par un effacement complet de la flash (par exemple `flash_nuke.uf2`) |

### 7.6 Les réglages usine

- **Réglage radio** (Réglages > Réglage radio) : quartz (26 ou 27 MHz), bruit de l'endroit (seuil d'écoute des
  télécommandes = bruit + 15 dB, entre −95 et −70 dBm), correction de fréquence d'après les autres badges entendus
  pendant 10 s (au moins 2 paquets, sinon pas de correction). Fait au premier démarrage ; à refaire sur le lieu de
  l'événement si les télécommandes ou les badges passent mal, près d'autres badges allumés. Un réglage interrompu
  recommence au démarrage suivant.
- **Calibration de la batterie** : § 4.3. Jamais effacée par le badge.


## 8. Langue du badge

Le badge parle **français** ou **anglais** (d'autres langues s'ajoutent sans toucher au code) : tout le détail est
dans [traduction](traduction.md).

| Action | Comment |
|---|---|
| Choisir la langue | Réglages > Langue (« Langue / Language ») : flancs pour choisir, D pour valider |
| Revenir à l'anglais depuis n'importe quelle langue | aile gauche (G) **5 fois** (retour au menu principal), puis aile gauche **maintenue 5 s** |
| Par le port série | `E` : anglais ; `N` : langue suivante (trace `i18n: language en (English)`) |
| Captures d'écran d'une langue | `python tools/badge_screens.py --lang en` : `docs/screens/en/` et `docs/en/screens.md`, puis le badge revient à sa langue |
| Ajouter / corriger une traduction | `python tools/i18n.py update`, traduire `src/menu/lang/<code>.po`, `python tools/i18n.py gen`, compiler |
| Vérifier | `python tools/i18n.py check` (textes non traduits, formats, caractères des polices) |

La langue est gardée dans la mémoire du badge (elle survit à une mise à jour du firmware et aux remises
à zéro du menu Admin). Les traces du port série ne sont pas traduites : les outils du PC
fonctionnent quelle que soit la langue.


## 9. Dépannage

| Problème | Solution |
|---|---|
| Le badge ne démarre pas | Interrupteur sur ON ; batterie chargée (brancher en USB). |
| Le badge n'apparaît pas en USB | Interrupteur sur ON ; câble de données (pas un câble de charge seule) ; essayer un autre port USB. |
| « cannot open COM9 », « used by another application », picotool ne trouve pas le badge | Le port est ouvert ailleurs : fermer `badge_remote.py`, le terminal, l'autre script. Un seul programme à la fois. |
| Un script trouve le mauvais badge | Plusieurs badges branchés : préciser `--port` (`--ports` pour les tests à deux badges). |
| « the badge does not answer: is the menu application (badge_menu) flashed? » | Le badge tourne une application de test ou un autre firmware : flasher `badge_menu.uf2`. |
| Rien ne s'affiche dans le terminal | Normal : le badge ne parle que sur événement. Taper `!`. Les lignes émises avant l'ouverture du port sont perdues. |
| Le badge affiche « SOMMEIL » | Mis en sommeil par un admin : 5 × flanc gauche puis 5 × flanc droit (§ 5.3). Ni un redémarrage ni un flash ne le réveillent. |
| Plus de son ni de LEDs | Mode muet (commande pendant un talk) : Réglages > Mode muet, ou commande `0x03`. |
| Le thème Admin est apparu tout seul | La séquence des flancs a été tapée par hasard : Admin > Quitter le mode admin. |
| Le thème Admin a disparu après un test | Les scripts de test coupent le mode admin à la fin : le réactiver (§ 4.1). |
| Le badge n'obéit pas au Flipper | Réglages > Télécommande : oui ; fichier `.sub` (pas `subghz tx`) ; code `0xC16Axx` ; bouton maintenu une seconde ; badge proche. `O` sur la console montre les tentatives de décodage. |
| Les badges s'entendent mal, les télécommandes passent mal | Réglages > Réglage radio sur place, près d'autres badges allumés. `!` : comparer « crystal used » et « measured ». |
| Les autres cigales ne sont plus entendues | Normal tant qu'une page occupe la radio : Décodeur 433 MHz, Station météo, Chasse 433 MHz, Badge de talk, échange de contacts, radio pirate. |
| Pas de niveau de batterie | Batterie non calibrée : § 4.3. |
| « Carte SD absente », `video: mount failed` | Carte bien enfoncée, FAT32 ou exFAT. |
| Un fichier n'apparaît pas | Bon dossier et bonne extension ; nom de 63 octets au plus ; pas plus de 48 entrées dans le dossier ; fichier non caché. |
| Une sonnerie est marquée `(!)` | Aile droite : ligne, colonne et raison ; la console écrit `rtttl: ... error ...` ([sonneries.md](sonneries.md)). |
| Un livre-jeu n'apparaît pas ou s'arrête | `python tools/gamebook_check.py LIVRES/livre.txt` ; 16 livres au plus. |
| Une vidéo ou une musique s'arrête avant la fin | `badge_media_test.py` pour la retrouver ; reconvertir avec `video2epaper.py` / `audio2wav.py`. |
| Son trop faible | Médias > Volume ; WAV converti par `audio2wav.py` sans `--no-filter`. |
| `score_check.py` déclare tous les scores invalides | Mauvaise clé, ou clé d'une autre version du firmware (`--make-key` change la clé). |
| Assassin « Trop loin ou absente », contrebande sans cigale « à portée de main » | Coller les badges. Les seuils de RSSI sont réglés à la compilation (développeurs). |
| `test_werewolf.py` interrompu, parties à robots | Admin > Loup-garou (admin) : remettre « 8 joueurs minimum ». |
| L'écran garde des traces | Normal après des rafraîchissements rapides : il se nettoie au prochain rafraîchissement complet. |
| Le badge ne répond plus | Bouton RESET, ou éteindre / rallumer ; `R` sur la console. |

Pour les problèmes de développement (compilation, radio, paquets perdus) : [guide développeur § 11](guide_developpeur.md#11-dépannage-du-développement).
