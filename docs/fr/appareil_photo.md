# Appareil photo : le module ESP32-CAM

*English version: [camera.md](../en/camera.md).*

Un **ESP32-CAM** (AI-Thinker : ESP32, caméra OV2640 2 Mpx, 4 Mo de PSRAM, LED de flash) branché sur le port **J2**
fait de la cigale un appareil photo : **Badge > Appareil photo**. L'e-Paper sert de viseur, les photos et les vidéos
vont sur la carte SD du badge.

- **Le firmware de l'ESP32** : [esp32cam/](../../esp32cam) (PlatformIO, Arduino).
- **La liaison** : [src/shared/camlink/](../../src/shared/camlink) (commune aux deux firmwares, testée sur le PC).
- **La page du badge** : [src/menu/camera.c](../../src/menu/camera.c).


## 1. Le matériel

### Les broches de l'ESP32-CAM

Deux rangées de 8 broches ; les noms sont imprimés au dos de la carte (à vérifier sur la vôtre) :

| Rangée 1 | Rangée 2 |
|---|---|
| 5V | 3V3 |
| GND | IO16 (PSRAM : ne pas utiliser) |
| IO12 | IO0 (à la masse au démarrage : mode programmation) |
| IO13 | GND |
| IO15 | VCC (sortie 3,3 V ou 5 V selon le pont : ne pas alimenter par là) |
| IO14 | U0R (IO3 : réception de l'UART0) |
| IO2 | U0T (IO1 : émission de l'UART0) |
| IO4 (LED de flash) | GND |

L'UART0 (U0R, U0T) sert à la fois à **programmer** l'ESP32 et à **parler au badge**.

### Programmer l'ESP32-CAM

L'ESP32-CAM n'a pas de port USB : il se programme par son UART0, avec un adaptateur USB-série (ici l'**ESP Flasher
v6.1**). Le schéma exact de l'ESP Flasher (sa rangée de broches, son bloc de 2 × 4) est à compléter : voir § 6.

Le principe, avec n'importe quel adaptateur USB-série **en 3,3 V** :

| Adaptateur | ESP32-CAM |
|---|---|
| 5V | 5V (l'ESP32-CAM a son régulateur 3,3 V ; la caméra et le Wi-Fi demandent du courant) |
| GND | GND |
| TX | U0R |
| RX | U0T |
| — | IO0 relié à GND **pendant le démarrage** (un fil ou un cavalier) |

1. Relier IO0 à GND, brancher l'adaptateur (ou appuyer sur le bouton RST de l'ESP32-CAM) : mode programmation.
2. `cd esp32cam`, puis `pio run -t upload --upload-port COMx` (PlatformIO : `pip install platformio`).
3. Retirer le fil IO0-GND, appuyer sur RST : le firmware démarre.

### Brancher l'ESP32-CAM sur la cigale (port J2)

| J2 (cigale) | Signal | ESP32-CAM |
|---|---|---|
| 1 | +3,3 V | **3V3** (pas 5V : le badge n'a pas de 5 V) |
| 11 ou 12 | GND | GND |
| 9 | UART0 TX (GPIO12) | U0R (IO3) |
| 7 | UART0 RX (GPIO13) | U0T (IO1) |

```
   Cigale, port J2 (2 x 6)                ESP32-CAM
   ┌─────────────────────┐
   │  1 +3,3V ●──────────┼──────────────── 3V3
   │  7 RX    ●──────────┼──────────────── U0T (IO1)
   │  9 TX    ●──────────┼──────────────── U0R (IO3)
   │ 11 GND   ●──────────┼──────────────── GND
   └─────────────────────┘
```

- **IO0 non relié** (il a une résistance de tirage sur la carte) : l'ESP32 démarre son firmware.
- **Le courant** : la caméra et la PSRAM tirent ~150-250 mA ; le régulateur 3,3 V du badge (TPS63000) donne ~1 A.
  Le Wi-Fi n'est pas utilisé. La **LED de flash** tire beaucoup : réglage « Flash » à « non » par défaut.
- **Le port J2** porte aussi la carte infrarouge (GPIO17, GPIO20) : les deux à la fois demandent un câble en Y ou
  une carte d'extension commune.
- Au démarrage, l'ESP32 écrit son texte de boot sur l'UART0 (115 200 bauds) : le badge l'ignore (trames COBS).


## 2. Sur le badge : Badge > Appareil photo

- **Recherche du module** : le badge envoie un bonjour chaque seconde ; le module répond (son nom et sa version).
- **Le viseur** : l'image de la caméra en plein écran, rafraîchie dès qu'elle arrive (rafraîchissement rapide de
  l'e-Paper, ~2-3 images par seconde). Une bande en bas : ce que font les boutons, ou le dernier message.
- **B** : une **photo**. Le module envoie le JPEG à la taille choisie, puis une version 200 × 200 en 4 niveaux de gris :
  - `PHOTOS/IMG_0001.JPG` : la photo (pour un ordinateur) ;
  - `IMAGES/PHOTOS/IMG_0001.EPI` : la même, à voir sur le badge (**Médias > Images**, dossier PHOTOS).
- **B long** : une **vidéo** (B long à nouveau : arrêt). 4 images par seconde, noir et blanc, sans son :
  `VIDEOS/PHOTOS/VID_0001.EPV`, à voir dans **Médias > Vidéos**.
- **X** : les **réglages** (gardés) ; Y / X choisir, B changer la valeur, A retour au viseur :

| Réglage | Valeurs |
|---|---|
| Taille des photos | 320x240, 640x480, 800x600 (défaut), 1024x768, 1280x1024, 1600x1200 |
| Qualité JPEG | maximale, haute (défaut), moyenne, basse |
| Luminosité, Contraste, Saturation | -2 à +2 |
| Effet | aucun, négatif, gris, rouge, vert, bleu, sépia |
| Miroir, Retourner | non / oui |
| Flash | non, pour la photo, toujours (lampe) |
| Trame du viseur | Atkinson (défaut), Floyd-Steinberg, Bayer, seuil |

- **A** : quitter (arrête la vidéo en cours).


## 3. La liaison (src/shared/camlink)

- UART à **921 600 bauds**, 8N1.
- Paquet : `[type][seq][longueur sur 2 octets][données][CRC-16]`, encodé en **COBS**, terminé par un octet 0.
- Les données du module (viseur, fichiers, vidéo) attendent l'**ACK** du badge avant le paquet suivant (renvoyées
  après 300 ms) : le badge peut être occupé par son écran ou sa carte SD sans rien perdre.
- Une image du viseur : 200 × 200 en 1 bit = 5 000 octets, en 5 paquets de 1 000.

| Type | Sens | Contenu |
|---|---|---|
| HELLO | les deux | version du protocole, nom du firmware |
| ACK | les deux | le numéro du paquet reçu |
| SETTINGS | badge → module | les réglages (clé, valeur) |
| PREVIEW | badge → module | viseur marche / arrêt |
| FRAME | module → badge | une partie d'une image du viseur |
| SHOT | badge → module | prendre une photo |
| FILE_OPEN / FILE_DATA / FILE_CLOSE | module → badge | un fichier de la photo (JPEG, puis .epi) |
| VIDEO | badge → module | vidéo marche / arrêt, images par seconde |
| VFRAME | module → badge | une partie d'une image de la vidéo |
| STATUS | module → badge | prêt, ou caméra absente |

Test sur le PC : `python src/tests/host/run_tests.py camlink` (aller-retour COBS, erreurs, reprise après le texte de
boot, tramages).


## 4. Côté ESP32 (esp32cam/)

- La caméra en **JPEG 320 × 240** pour le viseur : décodée en gris, le carré central réduit à 200 × 200, contraste
  étiré, tramé en 1 bit (la trame choisie), envoyé.
- La **photo** : la taille passe à celle des réglages, le flash s'allume si demandé, 3 images jetées (l'exposition
  s'adapte), le JPEG est envoyé ; puis le viseur repasse en 320 × 240 et son image en 4 gris (Floyd-Steinberg) part
  en `.epi`.
- La **vidéo** : une image par période (4 par seconde), tramée en Bayer (stable d'une image à l'autre : moins de
  rémanence sur l'e-Paper).
- Rien d'autre sur l'UART0 (`CORE_DEBUG_LEVEL=0`).


## 5. Essais

1. Programmer l'ESP32-CAM (§ 1), le brancher sur J2.
2. Badge > Appareil photo : « Module trouvé », puis le viseur.
3. Une photo (B) : le message « Photo IMG_0001 » ; Médias > Images > PHOTOS.
4. Une vidéo (B long, quelques secondes, B long) : Médias > Vidéos > PHOTOS.
5. Les traces du badge (port série USB) : `camera: module ...`, `camera: PHOTOS/IMG_0001.JPG open`,
   `camera: photo 1 saved`, `camera: video saved, N frames`.


## 6. À compléter

- Le brochage de l'**ESP Flasher v6.1** (la rangée de broches et le bloc de 2 × 4) : schéma de branchement exact,
  et si le flasheur pilote IO0 et le reset tout seul.
- Les mesures : images par seconde du viseur, courant pendant une photo avec le flash, durée d'envoi d'une photo.
