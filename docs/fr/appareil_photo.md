# Appareil photo : le module ESP32-CAM

*English version: [camera.md](../en/camera.md).*

Un **ESP32-CAM** (AI-Thinker : ESP32, caméra OV2640 2 Mpx, 4 Mo de PSRAM, LED de flash) branché sur un port
d'extension (SAO) fait de la cigale un appareil photo : **Badge > Appareil photo**. L'e-Paper sert de viseur, les photos et les vidéos
vont sur la carte SD du badge.

- **Le firmware de l'ESP32** : [esp32cam/](../../esp32cam) (PlatformIO, Arduino).
- **La liaison** : [src/shared/camlink/](../../src/shared/camlink) (commune aux deux firmwares, testée sur le PC).
- **La page du badge** : [src/menu/camera.c](../../src/menu/camera.c).

Sommaire : [1. Comment ça marche](#1-comment-ça-marche) · [2. Le matériel](#2-le-matériel) (programmer le module,
[le brancher sur la cigale](#brancher-lesp32-cam-sur-la-cigale-ports-sao-de-la-carte-v12)) ·
[3. Sur le badge](#3-sur-le-badge--badge--appareil-photo) · [4. La télécommande web](#4-la-télécommande-web--badge--télécommande) ·
[5. La liaison](#5-la-liaison-srcsharedcamlink) · [6. Côté ESP32](#6-côté-esp32-esp32cam) · [7. Essais](#7-essais) ·
[8. Dépannage](#8-dépannage) · [9. À compléter](#9-à-compléter)


## 1. Comment ça marche

Le badge ne sait pas lire une caméra : c'est l'ESP32 du module qui fait tout le travail d'image. Le badge lui donne
les ordres, affiche ce qu'il reçoit et l'écrit sur sa carte SD. Les deux parlent par une **liaison série** (2 fils,
921 600 bauds) :

```
  ┌──────────────────── ESP32-CAM ────────────────────┐                ┌──────────────── badge (RP2040) ───────────────┐
  │ caméra OV2640 ──JPEG──► ESP32 (240 MHz, 4 Mo PSRAM)│   liaison      │ camport.c (le service de la liaison)          │
  │                         · décode, réduit à 200×200 │   série        │   ├─ camera.c : la page Appareil photo        │
  │ LED de flash (IO4) ◄──  · trame en 1 bit / 4 gris  │ ◄────────────► │   │   ├─► écran e-Paper (le viseur)           │
  │                         · garde le JPEG de la photo│  U0T → RX      │   │   └─► carte SD : PHOTOS, IMAGES, VIDEOS   │
  │ Wi-Fi ◄──► téléphone    · serveur web (télécommande│  U0R ← TX      │   └─ webremote.c : la Télécommande           │
  └────────────────────────────────────────────────────┘ + 3,3 V, masse └───────────────────────────────────────────────┘
```

- **La recherche** : à l'ouverture de la page, le badge essaie chaque liaison possible de ses ports d'extension, une
  par seconde, en envoyant un bonjour (HELLO) ; le module répond avec son nom et sa version, le badge garde cette
  liaison. Rien à configurer : le module peut être branché sur l'un ou l'autre port (§ 2).
- **Le viseur** : le badge demande le viseur (PREVIEW) ; le module prend des images JPEG 320 × 240, les décode, garde
  le carré central réduit à 200 × 200, étire le contraste et les trame en noir et blanc (5 000 octets), envoyés en 5
  paquets. Le badge affiche chaque image en rafraîchissement rapide de l'e-Paper : **2 à 3 images par seconde**.
- **La photo** (B) : le module passe à la taille des réglages, allume le flash si demandé, jette 3 images (le temps
  que l'exposition s'adapte), envoie le **JPEG** (le badge l'écrit dans `PHOTOS/`), puis une version 200 × 200 en
  **4 gris** (`.epi`, dans `IMAGES/PHOTOS/`) pour la voir sur le badge. Environ **2 s** du bouton à l'enregistrement.
- **La vidéo** (B long) : le module envoie une image tramée par période (Bayer : stable d'une image à l'autre, moins de
  rémanence) ; le badge les ajoute au fichier `.epv` (`VIDEOS/PHOTOS/`) en les affichant. **~3,5 images par
  seconde**, sans son.
- **Rien ne se perd** : chaque paquet de données attend l'accusé de réception de l'autre côté (renvoyé après 300 ms) ;
  le badge peut être occupé par son écran ou sa carte SD pendant ce temps.
- **La télécommande** (§ 4) passe par la même liaison : le badge envoie son écran au module, le module lui renvoie les
  boutons appuyés sur la page web.


## 2. Le matériel

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

### Brancher l'ESP32-CAM sur la cigale (ports SAO de la carte V1.2)

La carte V1.2 a deux ports d'extension **SAO** (2 × 3 broches au pas de 2,54 mm), d'après son schéma
([hardware/badge_v1.2.pdf](../../hardware/badge_v1.2.pdf)) :

| Broche | SAO0 | SAO1 |
|---|---|---|
| 1 | **masse** | **masse** |
| 2 | **+3,3 V** | **+3,3 V** |
| 3 | I2C0 SCL (GPIO17) | I2C1 SCL (GPIO3) |
| 4 | I2C0 SDA (GPIO16) | I2C1 SDA (GPIO2) |
| 5 | GPIO19 | UART0 RX (GPIO13) : la réception du badge |
| 6 | GPIO20 | UART0 TX (GPIO12) : l'émission du badge |

```
   Port SAO (numéros du schéma)       la broche 1 est repérée sur la carte (pastille carrée ou marque)
   ┌─────────┬─────────┐
   │ 1 masse │ 2 3,3 V │
   ├─────────┼─────────┤
   │ 3   SCL │ 4   SDA │
   ├─────────┼─────────┤
   │ 5       │ 6       │
   └─────────┴─────────┘
```

> **Attention à l'alimentation** : sur cette carte, la broche 1 est la **masse** et la broche 2 le **+3,3 V** (le
> contraire du standard SAO habituel). Avant de brancher, vérifier au multimètre : continuité entre la broche 1 et
> la masse du badge (le blindage de la prise USB-C, par exemple), 3,3 V entre les broches 2 et 1 badge allumé. Une
> inversion détruit l'ESP32-CAM.

**Deux façons de brancher le module**, au choix ; le badge trouve tout seul celle qui est câblée (4 fils dans les deux
cas) :

| ESP32-CAM | Sur SAO1 (conseillé) | Sur SAO0 |
|---|---|---|
| **3V3** (surtout pas 5V : le badge n'a pas de 5 V) | broche 2 | broche 2 |
| **GND** | broche 1 | broche 1 |
| **U0R** (IO3, la réception du module) | broche 6 (UART0 TX, GPIO12) | broche 4 (SDA, GPIO16) |
| **U0T** (IO1, l'émission du module) | broche 5 (UART0 RX, GPIO13) | broche 3 (SCL, GPIO17) |
| Trace du badge | `on UART0 GPIO12/13` | `on UART0 GPIO16/17` |

```
   SAO1 du badge                     ESP32-CAM
   1 masse  ●────────────────────── GND
   2 3,3 V  ●────────────────────── 3V3
   5 RX     ●────────────────────── U0T (IO1)    l'émission de l'un sur la réception de l'autre
   6 TX     ●────────────────────── U0R (IO3)
   3, 4     (libres)                IO0 : non relié
```

- **SAO1** : ses broches 5 et 6 sont l'UART0 du RP2040, faites pour une liaison série ; SAO0 reste libre pour la carte
  infrarouge (GPIO17, GPIO20). Les broches 3 et 4 de SAO1 (I2C1) servent à l'écran OLED : pas d'OLED en même temps
  sur ce port.
- **SAO0** : le badge détourne les broches de l'I2C0 en liaison série (GPIO16 en émission, GPIO17 en réception) ;
  c'est le branchement essayé en premier. Pas de carte infrarouge sur ce port en même temps.
- **IO0 non relié** (il a une résistance de tirage sur la carte) : l'ESP32 démarre son firmware.
- **Le courant** : la caméra et la PSRAM tirent ~150-250 mA, le Wi-Fi de la télécommande des pointes de ~300 mA ; le
  régulateur 3,3 V du badge (TPS63000) donne ~1 A. La **LED de flash** tire beaucoup : réglage « Flash » à « non » par
  défaut.
- Au démarrage, l'ESP32 écrit son texte de boot sur l'UART0 (115 200 bauds) : le badge l'ignore (trames COBS).
- La trace du badge (port série USB) dit où le module a été trouvé : `camera: module esp32cam 0.2, protocol 2, on
  UART0 GPIO12/13`.
- **Cartes V1.0 / V1.1** (connecteurs 2 × 6 J2, J3 : [electronique.md § 10](electronique.md#cartes-v10-v11--j2-j3)) :
  U0R sur J2 broche 9 (UART0 TX, GPIO12), U0T sur J2 broche 7 (UART0 RX, GPIO13), 3V3 sur J2 broche 1, GND sur J2
  broche 11.

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


## 3. Sur le badge : Badge > Appareil photo

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


## 4. La télécommande web : Badge > Télécommande

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
télécommande (protocole 2) : la première fois, le flasher avec l'ESP Flasher (§ 2).

- **Le courant** : le Wi-Fi tire des pointes de ~300 mA ; la batterie du badge se vide plus vite, arrêter la
  télécommande quand elle ne sert pas.
- Le badge n'envoie son écran que lorsqu'une page le regarde.


## 5. La liaison (src/shared/camlink)

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


## 6. Côté ESP32 (esp32cam/)

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


## 7. Essais

1. Programmer l'ESP32-CAM (§ 2), le brancher sur un port SAO (4 fils, § 2).
2. Badge > Appareil photo : « Module trouvé », puis le viseur.
3. Une photo (B) : le message « Photo IMG_0001 » ; Médias > Images > PHOTOS.
4. Une vidéo (B long, quelques secondes, B long) : Médias > Vidéos > PHOTOS.
5. Les traces du badge (port série USB) : `camera: module ...`, `camera: PHOTOS/IMG_0001.JPG open`,
   `camera: photo 1 saved`, `camera: video saved, N frames`.


## 8. Dépannage

| Ce qu'on voit | Pourquoi, et quoi faire |
|---|---|
| « Recherche du module » sans fin | Pas de réponse sur aucune liaison : U0R et U0T inversés (les croiser : l'émission de l'un sur la réception de l'autre), un fil sur la mauvaise broche, le module sans firmware (le programmer, § 2), IO0 relié à la masse (mode programmation). La trace du badge liste les liaisons essayées. |
| Le module trouvé, puis perdu ; le module redémarre | Alimentation trop faible : fils longs ou fins, batterie basse, le flash. Brancher le badge en USB pour essayer, raccourcir les fils, « Flash : non ». |
| « Caméra absente » | La nappe de la caméra mal enfoncée dans son connecteur (le volet noir doit être fermé), ou une caméra défectueuse. |
| Image floue | Le film de protection sur l'objectif, ou la mise au point (§ 2, « La netteté »). |
| Image très sombre ou très claire | Peu de lumière (pose longue) ; les réglages Luminosité, Contraste ; le flash. |
| Viseur lent | Normal : 2 à 3 images par seconde, la limite du rafraîchissement rapide de l'e-Paper. |
| Photo non enregistrée | Pas de carte SD, ou carte pleine ou protégée : Médias doit pouvoir la lire. |
| La télécommande ne s'ouvre pas | Voir § 4 : le module doit avoir le firmware du protocole 2 (`esp32cam 0.2` dans la trace). |


## 9. À compléter

- Le courant pendant une photo avec le flash.
- Le branchement sur SAO1 (UART0 GPIO12/13) n'a pas encore été essayé sur une carte V1.2.

**Essayé sur une cigale V1.2 (2026-10-19)** : le module trouvé sur **UART0 GPIO16/17** (broches 3 et 4 de SAO0) ; le
viseur ; une photo 800 × 600 en **2,1 s** du bouton à l'enregistrement (JPEG 17 Ko et son .epi) ; une vidéo de **~3,5
images par seconde** (32 images en 9 s).
