# Appareil photo : le module ESP32-CAM

*English version: [camera.md](../en/camera.md).*

Un **ESP32-CAM** (AI-Thinker : ESP32, caméra OV2640 2 Mpx, 4 Mo de PSRAM, LED de flash) branché sur un port
d'extension (SAO) fait de la cigale un appareil photo : **Badge > Appareil photo**. L'e-Paper sert de viseur, les photos et les vidéos
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

### Programmer l'ESP32-CAM avec l'ESP Flasher (Rev6)

L'ESP32-CAM n'a pas de port USB : il se programme par son UART0. L'**ESP Flasher Rev6** (KSEC / April Brother) est un
adaptateur USB-C → série **CH340**, en **3,3 V** (régulateur MIC5219, ~400 mA), avec un bouton RESET, un bouton FLASH
(GPIO0) et un circuit de démarrage automatique sur ses broches RST et GPIO0.

**Par la rangée de broches** (les noms sont imprimés à côté de chaque broche) :

| ESP Flasher | ESP32-CAM | |
|---|---|---|
| VCC | **3V3** | le flasheur donne du 3,3 V : surtout pas sur la broche 5V de l'ESP32-CAM |
| GND | GND | |
| TXD | U0R (IO3) | l'émission de l'un sur la réception de l'autre |
| RXD | U0T (IO1) | |
| GPIO0 | IO0 | le mode programmation, piloté par le flasheur (ou son bouton FLASH) |
| RST | — | non relié : l'ESP32-CAM n'a pas de broche de reset, seulement son bouton RST |
| CH_EN | — | non relié |

```
   ESP Flasher (rangée)            ESP32-CAM
   VCC   ●───────────────────────── 3V3
   GND   ●───────────────────────── GND
   TXD   ●───────────────────────── U0R (IO3)
   RXD   ●───────────────────────── U0T (IO1)
   GPIO0 ●───────────────────────── IO0
   RST   ●  (non relié)
   CH_EN ●  (non relié)
```

**Par le bloc de 2 × 4** : c'est le support d'un module ESP-01 (ESP8266). Brochage habituel de l'ESP-01, vu de
dessus, l'antenne vers le haut :

```
   ┌───────┬───────┬───────┬───────┐
   │  GND  │ GPIO2 │ GPIO0 │  RXD  │      RXD, TXD : ceux du module ESP-01 (le flasheur y envoie / en reçoit)
   ├───────┼───────┼───────┼───────┤
   │  TXD  │ CH_PD │  RST  │  VCC  │
   └───────┴───────┴───────┴───────┘
```

| Bloc 2 × 4 | ESP32-CAM |
|---|---|
| VCC | 3V3 |
| GND | GND |
| RXD (la réception du module) | U0R (IO3) |
| TXD (l'émission du module) | U0T (IO1) |
| GPIO0 | IO0 |
| GPIO2, CH_PD, RST | non reliés |

Le fabricant ne publie pas le brochage du bloc : **vérifiez-le au multimètre** (continuité entre chaque broche du
bloc et la broche de même nom de la rangée) avant de brancher. En cas de doute, utilisez la rangée.

**Programmer** :

1. Brancher le flasheur en USB : il apparaît comme un port série (CH340 : pilote inclus dans Windows 10/11, macOS,
   Linux).
2. `cd esp32cam`, puis `pio run -t upload --upload-port COMx` (PlatformIO : `pip install platformio`).
3. Quand PlatformIO affiche « Connecting... » : maintenir le bouton **FLASH** du flasheur (IO0 à la masse) et appuyer
   sur le bouton **RST** de l'ESP32-CAM, puis relâcher. Le flasheur ne peut pas redémarrer l'ESP32-CAM tout seul :
   sa broche RST n'est pas reliée.
4. À la fin, appuyer sur **RST** de l'ESP32-CAM : le firmware démarre. `pio device monitor` montre son texte de boot
   puis des trames binaires (la liaison avec le badge).

Le 3,3 V du flasheur (~400 mA) suffit pour programmer et essayer la caméra sur la table, **sans le flash** (la LED de
flash tire trop).

### Brancher l'ESP32-CAM sur la cigale (port d'extension SAO)

Les ports d'extension de la cigale sont au format **SAO** (2 × 3 broches, avec un détrompeur) :

```
   Port SAO (vu de face, le détrompeur en haut)
   ┌─────────┬─────────┐
   │ 1  3,3V │ 2   GND │
   ├─────────┼─────────┤
   │ 3   SDA │ 4   SCL │
   ├─────────┼─────────┤
   │ 5 GPIO1 │ 6 GPIO2 │
   └─────────┴─────────┘
```

Le badge n'a pas besoin de savoir où l'ESP32-CAM est branché : pendant la recherche, il essaie chaque liaison série
possible des ports (une par seconde, affichée à l'écran) et garde celle qui répond :

| Liaison | Badge → ESP32-CAM U0R | ESP32-CAM U0T → badge | Sur le port |
|---|---|---|---|
| UART0 GPIO16/17 | GPIO16 (SDA de l'I2C0) | GPIO17 (SCL de l'I2C0) | broches 3 et 4 du port qui porte l'I2C0 |
| UART1 GPIO20/21 | GPIO20 | GPIO21 | broches 5 et 6, si ce sont ces GPIO |
| UART0 GPIO12/13 | GPIO12 | GPIO13 | l'ancien port 2 × 6 de la carte V1.1 |

Le câblage (4 fils) :

| Port SAO | ESP32-CAM |
|---|---|
| 1 (3,3 V) | **3V3** (pas 5V : le badge n'a pas de 5 V) |
| 2 (GND) | GND |
| 3 (SDA) ou 5 (GPIO1) : l'émission du badge | U0R (IO3) |
| 4 (SCL) ou 6 (GPIO2) : la réception du badge | U0T (IO1) |

- **IO0 non relié** (il a une résistance de tirage sur la carte) : l'ESP32 démarre son firmware.
- **Le courant** : la caméra et la PSRAM tirent ~150-250 mA ; le régulateur 3,3 V du badge (TPS63000) donne ~1 A.
  Le Wi-Fi n'est pas utilisé. La **LED de flash** tire beaucoup : réglage « Flash » à « non » par défaut.
- La carte infrarouge et l'écran OLED ne doivent pas être branchés sur le même port.
- Au démarrage, l'ESP32 écrit son texte de boot sur l'UART0 (115 200 bauds) : le badge l'ignore (trames COBS).
- La trace du badge (port série USB) dit où le module a été trouvé : `camera: module esp32cam 0.1, protocol 1, on
  UART0 GPIO16/17`.


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

1. Programmer l'ESP32-CAM (§ 1), le brancher sur un port SAO (4 fils).
2. Badge > Appareil photo : « Module trouvé », puis le viseur.
3. Une photo (B) : le message « Photo IMG_0001 » ; Médias > Images > PHOTOS.
4. Une vidéo (B long, quelques secondes, B long) : Médias > Vidéos > PHOTOS.
5. Les traces du badge (port série USB) : `camera: module ...`, `camera: PHOTOS/IMG_0001.JPG open`,
   `camera: photo 1 saved`, `camera: video saved, N frames`.


## 6. À compléter

- Les GPIO réellement présents sur chaque port SAO de la carte V1.2 (le schéma de la V1.2 n'est pas dans le dépôt) :
  la recherche automatique couvre les cas possibles ; à confirmer pour la documentation.
- Les mesures : images par seconde du viseur, courant pendant une photo avec le flash, durée d'envoi d'une photo.
