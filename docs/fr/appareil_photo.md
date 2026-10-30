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
2. Repérer son port (`pio device list` : « USB-SERIAL CH340 ») et le mettre dans `upload_port` et `monitor_port` de
   `esp32cam/platformio.ini`. Sans cela, PlatformIO prend le premier port venu, souvent un badge.
3. `cd esp32cam`, puis `pio run -t upload` (PlatformIO : `pip install platformio` ; si `pio` n'est pas trouvé, il est
   dans `%USERPROFILE%\.platformio\penv\Scripts` sous Windows, `~/.platformio/penv/bin` ailleurs).
4. Quand PlatformIO affiche « Connecting... » : maintenir le bouton **FLASH** du flasheur (IO0 à la masse) et appuyer
   sur le bouton **RST** de l'ESP32-CAM, puis relâcher. Le flasheur ne peut pas redémarrer l'ESP32-CAM tout seul :
   sa broche RST n'est pas reliée.
5. À la fin, appuyer sur **RST** de l'ESP32-CAM : le firmware démarre. `pio device monitor` montre son texte de boot
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

### La netteté (mise au point)

L'OV2640 de l'ESP32-CAM est à **mise au point fixe** : pas d'autofocus, rien à régler par le programme. La netteté
se règle **à la main**, en vissant ou dévissant l'objectif :

1. Retirer le film de protection transparent sur l'objectif, s'il y est encore (une cause fréquente d'image floue).
2. L'objectif est souvent bloqué par un point de colle : le tenir avec une pince fine par sa bague crantée et tourner
   doucement pour casser la colle (sans tirer sur la nappe du capteur).
3. Tourner par petits pas en regardant le viseur du badge : dans le sens inverse des aiguilles d'une montre, la
   netteté vient plus près (portraits, objets à 20-50 cm) ; dans le sens des aiguilles, plus loin.
4. Une fois net, un point de vernis à ongles ou de colle le bloque, si besoin.

Le viseur est petit (200 × 200, 1 bit) : vérifier sur une photo JPEG (dossier `PHOTOS`, sur l'ordinateur). Avec peu
de lumière, la pose est longue et la photo floue de bougé : éclairer, ou le réglage « Flash ».


## 2. Sur le badge : Badge > Appareil photo

- **Recherche du module** : le badge envoie un bonjour chaque seconde ; le module répond (son nom et sa version).
- **Le viseur** : l'image de la caméra en plein écran, rafraîchie dès qu'elle arrive (rafraîchissement rapide de
  l'e-Paper, ~2-3 images par seconde). Une bande en bas : ce que font les boutons, ou le dernier message.
- **B** : une **photo**. Le module envoie le JPEG à la taille choisie, puis une version 200 × 200 en 4 niveaux de gris :
  - `PHOTOS/IMG_0001.JPG` : la photo (pour un ordinateur) ;
  - `IMAGES/PHOTOS/IMG_0001.EPI` : la même, à voir sur le badge (**Médias > Images**, dossier PHOTOS).
    Un appui long sur B, dans la liste ou sur la photo affichée : la renommer ou l'effacer (avec son JPEG).
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


## 3. La télécommande web : Badge > Télécommande

Le module ESP32-CAM sert une page web qui pilote le badge depuis un téléphone (ou un ordinateur) : l'**écran du
badge** en direct (les 4 gris compris), ses **4 boutons Y A B X** (appui long : maintenir le bouton) et un **champ de
texte** pour ses éditeurs (nom, messages...). La page n'envoie que les boutons et le texte : ni carte SD, ni
redémarrage, ni mode admin, contrairement au port USB.

**Démarrer** : Badge > Télécommande > « Télécommande : oui ». Elle reste active en arrière-plan (A revient aux menus,
on navigue partout depuis le téléphone) jusqu'à « Télécommande : non » ou le redémarrage du badge.

**Le point d'accès (par défaut)** : le module crée son propre Wi-Fi « Cigale-<nom> », protégé par un mot de passe
tiré au hasard à chaque démarrage. « Se connecter » affiche un **QR code Wi-Fi** : le téléphone le scanne, rejoint le
réseau et la page s'ouvre toute seule (portail captif) ; sinon : `http://192.168.4.1`. Ce mot de passe suffit : qui
le lit sur l'écran du badge a déjà le badge en main.

**Un réseau** : sur la page (depuis le point d'accès), « Wi-Fi du module » : le nom (SSID) et le mot de passe d'un
réseau, et une clé admin facultative ; le module les garde. Puis sur le badge : « Wi-Fi : réseau ». « Se connecter »
affiche l'adresse du module sur ce réseau (et son QR code) et un **code à 6 chiffres** (nouveau à chaque démarrage)
que la page demande avant de piloter le badge : les autres personnes du réseau ne peuvent pas le piloter. Le nom
`http://cigale-<nom>.local` marche aussi sur la plupart des ordinateurs. Si le réseau ne répond pas (15 s), le module
revient à son point d'accès (« Réseau introuvable »), pour corriger les réglages.

**La page admin** (`/admin` sur n'importe quel module du réseau) : l'écran de **toutes les cigales du réseau** côte à
côte, en lecture seule. Les modules se signalent entre eux (UDP, port 4210) ; la page demande la clé admin, la même
sur tous les modules (réglée dans « Wi-Fi du module »).

**Mettre à jour le module** depuis la carte SD du badge : copier `esp32cam/.pio/build/esp32cam/firmware.bin` (ou le
fichier du paquet de distribution) sous le nom **`ESP32/ESP32CAM.BIN`** sur la carte, puis Badge > Télécommande >
« Mettre à jour le module » > B. Le badge l'envoie par la liaison (~1 Mo, environ une minute), le module le vérifie
puis redémarre dessus ; son nouveau nom de version s'affiche. Il faut que le module ait déjà un firmware avec la
télécommande (protocole 2) : la première fois, le flasher avec l'ESP Flasher (§ 1).

- **Le courant** : le Wi-Fi tire des pointes de ~300 mA ; la batterie du badge se vide plus vite, arrêter la
  télécommande quand elle ne sert pas.
- Le badge n'envoie son écran que lorsqu'une page le regarde.


## 4. La liaison (src/shared/camlink)

- UART à **921 600 bauds**, 8N1.
- Paquet : `[type][seq][longueur sur 2 octets][données][CRC-16]`, encodé en **COBS**, terminé par un octet 0.
- Les données du module (viseur, fichiers, vidéo) attendent l'**ACK** du badge avant le paquet suivant (renvoyées
  après 300 ms) : le badge peut être occupé par son écran ou sa carte SD sans rien perdre.
- Une image du viseur : 200 × 200 en 1 bit = 5 000 octets, en 5 paquets de 1 000.
- Les données du badge (son écran, la mise à jour du module) attendent de même l'ACK du module.
- Côté badge, la liaison est un service partagé par l'appareil photo et la télécommande (`src/menu/camport.c`) ; sans
  la page de l'appareil photo, son récepteur est petit (96 octets : les touches, l'état du Wi-Fi) pour la RAM.

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
| STATUS | module → badge | prêt, caméra absente, résultat de la mise à jour |
| WEB | badge → module | télécommande marche / arrêt, point d'accès ou réseau, son nom, son mot de passe, le code |
| WEB_STATE | module → badge | pages ouvertes, état du Wi-Fi, adresse IP, réseau enregistré |
| SCREEN | badge → module | une partie de l'écran du badge (1 plan, ou 2 pour les 4 gris) |
| KEY | module → badge | un bouton de la page, ou un caractère tapé |
| OTA_BEGIN / OTA_DATA / OTA_END | badge → module | un nouveau firmware du module, depuis la carte SD |

Test sur le PC : `python src/tests/host/run_tests.py camlink` (aller-retour COBS, erreurs, reprise après le texte de
boot, tramages).


## 5. Côté ESP32 (esp32cam/)

- La caméra en **JPEG 320 × 240** pour le viseur : décodée en gris, le carré central réduit à 200 × 200, contraste
  étiré, tramé en 1 bit (la trame choisie), envoyé.
- La **photo** : la taille passe à celle des réglages, le flash s'allume si demandé, 3 images jetées (l'exposition
  s'adapte), le JPEG est envoyé ; puis le viseur repasse en 320 × 240 et son image en 4 gris (Floyd-Steinberg) part
  en `.epi`.
- La **vidéo** : une image par période (4 par seconde), tramée en Bayer (stable d'une image à l'autre : moins de
  rémanence sur l'e-Paper).
- La **télécommande** : le Wi-Fi (point d'accès avec portail captif, ou réseau enregistré dans la flash :
  `Preferences` « webremote »), un serveur web dans sa propre tâche (`esp_http_server`) : `/` la page, `/screen`,
  `/key`, `/type`, `/info`, `/config`, `/admin`, `/peers.json` ; les pages dans `esp32cam/src/web_pages.h`.
- La **mise à jour** : la bibliothèque `Update`, dans l'autre partition OTA (`min_spiffs.csv` : deux applications de
  1,9 Mo).
- Rien d'autre sur l'UART0 (`CORE_DEBUG_LEVEL=0`).


## 6. Essais

1. Programmer l'ESP32-CAM (§ 1), le brancher sur un port SAO (4 fils).
2. Badge > Appareil photo : « Module trouvé », puis le viseur.
3. Une photo (B) : le message « Photo IMG_0001 » ; Médias > Images > PHOTOS.
4. Une vidéo (B long, quelques secondes, B long) : Médias > Vidéos > PHOTOS.
5. Les traces du badge (port série USB) : `camera: module ...`, `camera: PHOTOS/IMG_0001.JPG open`,
   `camera: photo 1 saved`, `camera: video saved, N frames`.


## 7. À compléter

- Les GPIO réellement présents sur chaque port SAO de la carte V1.2 (le schéma de la V1.2 n'est pas dans le dépôt) :
  la recherche automatique couvre les cas possibles ; à confirmer pour la documentation.
- Le courant pendant une photo avec le flash.

**Essayé sur une cigale V1.2 (2026-10-19)** : le module trouvé sur **UART0 GPIO16/17** (broches 3 et 4 du port
SAO) ; le viseur ; une photo 800 × 600 en **2,1 s** du bouton à l'enregistrement (JPEG 17 Ko et son .epi) ; une vidéo
de **~3,5 images par seconde** (32 images en 9 s).
