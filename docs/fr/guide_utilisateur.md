# Badge SecSea — guide utilisateur

Ce guide explique comment utiliser le badge SecSea de Hack In Provence : allumer, naviguer dans les menus,
profiter des médias, des jeux, de la radio et des extensions, et préparer une carte SD.
Pour modifier le logiciel du badge, voir le [guide développeur](guide_developpeur.md).

*English version: [user guide](../en/user_guide.md).*


## 1. Le badge en bref

- Un écran e-Paper (encre électronique) de 200 × 200 pixels, en noir et blanc ou 4 niveaux de gris.
  L'image reste affichée même sans courant.
- 4 boutons, 2 LEDs de couleur, un buzzer.
- Une radio 433 MHz (CC1101) compatible avec le Flipper Zero.
- Un lecteur de carte micro-SD pour les vidéos, musiques, textes et images.
- Deux ports d'extension (infrarouge, écran OLED...).
- Une batterie rechargeable par USB-C.


## 2. Démarrer

1. Mettez l'interrupteur sur **ON**. Sur **OFF**, le badge ne démarre pas, même branché en USB
   (seule la charge fonctionne : LED rouge pendant la charge, verte quand la batterie est pleine).
2. Le menu principal s'affiche après quelques secondes.

L'écran e-Paper est lent (environ 0,3 s pour changer l'image) et clignote parfois en noir et blanc :
c'est normal, il se « nettoie » régulièrement pour éviter les traces des images précédentes.


## 3. Les boutons

Le badge est vu de face, la tête de la cigale en haut.

| Bouton | Dans les menus | Rôle habituel |
|---|---|---|
| **Aile droite** (D) | valider | OK, pause, lancer |
| **Aile gauche** (G) | retour | annuler, quitter |
| **Flanc gauche** | monter | précédent, moins |
| **Flanc droit** | descendre | suivant, plus |

Le bas de chaque écran rappelle les boutons utiles, par exemple « Flancs : choix  D : OK  G : retour ».
Dans le menu principal, l'aile gauche coupe aussi le son de la cigale et les LEDs.


## 4. Les menus

Le menu principal regroupe les fonctions par thème :

| Thème | Contenu |
|---|---|
| **Médias** | Images, Vidéos, Musique, Lecture rapide, Volume |
| **Jeux** | Morpion, Puissance 4, Simon, Réflexes, Snake, Blind test, CTF |
| **Badge** | Son de la cigale, animations des LEDs, démo de l'écran, écran OLED |
| **Radio & IR** | Réseau des cigales, message radio, porteuse radio, infrarouge |
| **Réglages** | Veille de l'écran, Infos, Crédits |


### 4.1 Médias

Les médias sont lus sur la carte SD (voir [§ 7](#7-préparer-la-carte-sd)).
Chaque lecteur affiche la liste des fichiers ; les sous-dossiers sont précédés de « > ».

**Images** (dossier `IMAGES`)
- Flancs : image précédente / suivante.
- Aile droite : choisir cette image comme image de veille.
- Aile gauche : retour à la liste.

**Vidéos** (dossier `VIDEOS`)
- Aile droite : pause / reprise ; flancs : volume ; aile gauche : arrêt.
- Le son sort par le buzzer : il est reconnaissable, pas hi-fi.

**Musique** (dossier `MUSIQUE`)
- Aile droite : pause ; aile gauche : stop ; flanc gauche : moins fort ; flanc droit : plus fort.

**Lecture rapide** (dossier `TEXTES`)
- Les mots s'affichent un par un au même endroit, les yeux ne bougent plus : on lit beaucoup plus vite
  (méthode PVSR, voir [pvsr.md](../pvsr.md)).
- Aile droite : pause / reprise ; aile gauche : quitter.
- Flancs (appui court) : vitesse −/+ (de 100 à 900 mots par minute, 250 au départ).
- Flancs (appui long) : recul / avance d'environ 10 secondes de lecture.
- La position est retenue : un texte reprend là où vous l'avez quitté.

**Volume** : chaque appui sur l'aile droite augmente le volume (0 à 8, puis retour à 0) et joue un petit carillon.


### 4.2 Jeux

Dans tous les jeux (sauf Simon), **l'aile gauche quitte le jeu**.

| Jeu | Comment jouer |
|---|---|
| **Morpion** | Flancs : choisir la case ; aile droite : jouer. Vous avez les croix, la cigale les ronds. Chacun commence à son tour. La cigale se trompe de temps en temps... |
| **Puissance 4** | Flancs : choisir la colonne ; aile droite : lâcher le pion. Alignez 4 pions pleins avant les anneaux de la cigale. |
| **Simon** | Chaque bouton est une zone de l'écran, à sa place sur le badge (flancs en haut, ailes en bas), avec sa couleur et sa note. Répétez la séquence, qui s'allonge à chaque tour. Ici l'aile gauche est une zone du jeu : pour quitter, attendez la fin de la partie puis aile gauche. |
| **Réflexes** | Quand les LEDs s'allument en vert (avec un bip), appuyez vite sur l'aile droite ou un flanc. 5 essais, la moyenne compte. Trop tôt : l'essai recommence. |
| **Snake** | Aile droite : départ / pause. Flanc gauche : tourner à gauche ; flanc droit : tourner à droite (par rapport à la direction du serpent). |
| **Blind test** | Choisissez un dossier de musiques : les morceaux passent dans un ordre aléatoire, sans répétition. Aile droite : afficher le titre ; flanc droit : morceau suivant ; flanc gauche : pause. |
| **CTF** | Un défi : entrer un code célèbre des jeux vidéo avec les 4 boutons pour obtenir un « flag ». |

Les records sont gardés même après extinction : meilleur score de Simon et Snake, meilleure moyenne de Réflexes,
nombre de victoires contre la cigale au Morpion et au Puissance 4.

**Score en QR code** : à la fin d'une partie, un appui sur un flanc affiche un QR code qui contient le jeu, le score,
le nom de votre cigale et une signature : les organisateurs peuvent le scanner pour un classement, et un score
modifié à la main est refusé. N'importe quel bouton referme le QR code.
Dans le menu Jeux, un **appui long sur l'aile droite** sur le nom d'un jeu affiche de la même façon le QR code de son record.

En bas de l'écran, les boutons sont indiqués dans l'ordre où ils sont sur le badge : G (aile gauche) à gauche,
D (aile droite) à droite.


### 4.3 Badge

- **Cigale** : active ou coupe le chant de la cigale.
- **LEDs** : change l'animation (arc-en-ciel, respiration, battement, clignotement, vert fixe, éteintes).
- **Démo écran** : montre les possibilités de l'écran (noir et blanc, 4 gris, animation rapide).
- **Écran OLED** : démos sur un petit écran OLED branché sur le port gauche (étoiles, cube 3D, cigale, texte, vidéo).


### 4.4 Radio & IR

**Réseau des cigales**
- Les badges allumés s'envoient discrètement des signaux radio.
- Approchez votre badge tout près d'un autre (quelques centimètres) pendant quelques secondes : vous gagnez
  **10 points** pour une nouvelle rencontre, **1 point** pour une cigale déjà rencontrée (une fois par heure au plus).
- Aile droite : choisir le **nom de votre cigale** (8 caractères).
  - Flancs : changer la lettre.
  - Aile droite (appui court) : lettre suivante ; aile gauche : lettre précédente.
  - Aile droite (appui long) : enregistrer.
  - Aile gauche sur la première lettre : annuler.
- Flanc droit : activer / couper les signaux.

**Radio : message** envoie un message que l'on peut lire avec l'application « SubGHz chat » d'un Flipper Zero (433,92 MHz).

**Radio : porteuse** émet un signal continu pendant 30 s (visible avec l'analyseur de fréquence d'un Flipper Zero).

**Infrarouge** (module sur le port droit)
- « Enregistrer un signal » : visez le récepteur avec une télécommande et appuyez sur une touche.
- Les 4 emplacements rejouent les signaux enregistrés, comme une télécommande.


### 4.5 Réglages

- **Veille de l'écran** :
  - le délai avant la veille (1, 2, 5, 10 ou 30 minutes, ou désactivée) ;
  - l'image affichée pendant la veille (la SecSea par défaut, ou une image du dossier `IMAGES`) ;
  - « Aperçu » pour l'essayer.

  Pendant la veille, n'importe quel bouton réveille le badge.
- **Infos** : version de la radio, quartz, carte SD, batterie. Aile droite : les crédits.
- **Crédits** : les personnes et associations derrière le badge.


## 5. Batterie

- La batterie se recharge par le port USB-C, même interrupteur sur OFF.
  LED rouge : en charge ; LED verte : chargée.
- Le niveau de batterie n'est affiché (icône en haut à droite du menu, et dans Infos)
  que si le badge a été calibré : sinon « non calibrée » (voir le guide développeur).


## 6. Piloter le badge depuis un ordinateur

Branché en USB, le badge apparaît comme un port série. L'application `tools/badge_remote.py` affiche l'écran du badge
en grand sur l'ordinateur et le pilote au clavier (flèches = boutons).

```bash
pip install pyserial
python tools/badge_remote.py
```

Voir le [guide développeur](guide_developpeur.md#8-le-protocole-usb-série) pour les touches du port série.


## 7. Préparer la carte SD

### 7.1 La carte

- Toute capacité fonctionne (micro-SD, SDHC, SDXC).
- Formatée en **FAT32** ou **exFAT**, comme le fait un ordinateur ou un appareil photo.
  Le badge ne formate jamais la carte et n'y écrit rien.
- Les noms de fichiers peuvent être longs et contenir des accents.

### 7.2 Les dossiers

```
carte SD
├── IMAGES/     images .epi (200×200, 4 gris)
├── VIDEOS/     vidéos .epv
├── MUSIQUE/    sons .wav (sous-dossiers possibles, pratiques pour le blind test)
└── TEXTES/     textes .txt pour la lecture rapide
```

### 7.3 Convertir ses fichiers

Les scripts sont dans le dépôt du projet. Ils demandent Python 3 et, pour le son et la vidéo,
[ffmpeg](https://ffmpeg.org/) installé et accessible dans le PATH.

```bash
pip install pillow numpy
```

**Images** : n'importe quelle photo (JPEG, PNG...) devient une image 200 × 200 en 4 gris.

```bash
python src/images/image2epi.py photo1.jpg photo2.png -o IMAGES
python src/images/image2epi.py affiche.jpg --fit -o IMAGES        # image entière, avec des bandes blanches
python src/images/image2epi.py paysage.jpg --equalize --preview    # plus de contraste, et un aperçu .png
```

Options utiles :
- `--fit` : l'image n'est pas recadrée ;
- `--contrast 3` : contraste plus fort ;
- `--equalize` : pour les images ternes ;
- `--bw` : noir et blanc pur ;
- `--preview` : écrit un aperçu en .png.

**Vidéos** : la vidéo est réduite à 200 × 200, en noir et blanc, 10 images par seconde, avec le son pour le buzzer.

```bash
python src/video/video2epaper.py film.mp4 -o VIDEOS/film.epv
python src/video/video2epaper.py film.mp4 --fit --start 60 --duration 30 -o VIDEOS/extrait.epv
```

Options utiles :
- `--fit` : l'image n'est pas recadrée ;
- `--fps 10|20|30` : 10 donne le meilleur contraste ;
- `--no-audio` : sans le son ;
- `--preview apercu.gif` : écrit un aperçu animé.

**Sons** : MP3, OGG, FLAC... deviennent des WAV adaptés au buzzer (8 bits, 16 kHz, son compressé pour être audible).

```bash
python src/audio/audio2wav.py "musiques/*.mp3" -o MUSIQUE/Films
python src/audio/audio2wav.py dossier_complet -o MUSIQUE
```

**Textes** : n'importe quel fichier `.txt` (UTF-8 ou Windows/Latin-1) dans `TEXTES`.

> Respectez les droits d'auteur : utilisez des œuvres libres de droits ou dont vous avez les droits.


## 8. En cas de problème

| Problème | Solution |
|---|---|
| Le badge ne s'allume pas | Interrupteur sur ON ? Batterie chargée (brancher en USB) ? |
| Le badge n'est pas vu par l'ordinateur | Câble USB de données (pas un câble de charge seule) ; interrupteur sur ON. |
| « Carte SD absente » | Carte bien enfoncée ? Formatée en FAT32 ou exFAT ? |
| Un fichier n'apparaît pas | Bonne extension (`.epi`, `.epv`, `.wav`, `.txt`) et bon dossier ? Nom de moins de 64 caractères ? |
| Pas de son | Volume à 0 ? (Médias > Volume). Le buzzer est discret : collez l'oreille. |
| Le message radio n'arrive pas au Flipper | Le Flipper doit être sur 433,92 MHz dans « SubGHz chat ». Dans Infos, le quartz utilisé doit être 26 ou 27 MHz. |
| L'écran garde des traces | Normal après de nombreux rafraîchissements rapides : il se nettoie au prochain rafraîchissement complet. |
| Le badge ne répond plus | Bouton RESET, ou éteindre / rallumer. |
