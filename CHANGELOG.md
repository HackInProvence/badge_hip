# Historique des versions

Le numéro de version est dans le fichier [VERSION](VERSION) ; la marche à suivre pour une nouvelle version est dans le
[guide développeur](docs/fr/guide_developpeur.md#faire-une-nouvelle-version).

Format : `MAJEUR.MINEUR.CORRECTIF`.
- **CORRECTIF** : des corrections, rien de nouveau.
- **MINEUR** : des fonctions nouvelles, compatibles avec les badges de la version précédente.
- **MAJEUR** : incompatible : le réseau radio des cigales change, ou les réglages enregistrés sont remis à zéro.


## Non publiée

Rien pour l'instant.


## 1.3.0 — 2026-11-19

Depuis `v_1.1.0` (la 1.2.0 n'a pas été publiée : son contenu est ici).

**Nouveautés**
- **Appareil photo** (Badge) : un module ESP32-CAM branché sur un port d'extension (SAO) : viseur sur l'écran,
  photos (JPEG et image pour le badge), vidéos, réglages ; le badge trouve tout seul le port du module. Le
  fonctionnement, le branchement et le dépannage : [docs/fr/appareil_photo.md](docs/fr/appareil_photo.md). Un petit
  appareil photo à côté de la cigale au démarrage.
- **Télécommande** (Badge) : avec le module ESP32-CAM, une page web pilote le badge depuis un téléphone (son écran en
  direct, ses 4 boutons, un champ de texte). Point d'accès Wi-Fi du module par défaut (QR code, portail captif), ou
  un réseau enregistré dans le module (un code à 6 chiffres protège le pilotage) ; page `/admin` : les écrans de
  toutes les cigales du réseau.
- **Mise à jour du module ESP32-CAM depuis la carte SD du badge** (`ESP32/ESP32CAM.BIN`) ; le paquet de distribution
  le contient.
- **Thème Flipper Zero** (Réglages > Thème) : un clin d'œil au Flipper Zero : les menus dans son style, et pour page
  d'accueil les animations de son dauphin (selon le grade du badge, avec leurs bulles) ; la veille montre le dauphin
  endormi. Les animations sont sur la carte SD (`FLIPPER/`, © Flipper Devices Inc., GPL v3), converties par
  `tools/flipper_anims.py`.
- **Médias** : appui long sur B sur un fichier (la liste ou l'image affichée) : le renommer ou l'effacer ; une photo
  de l'appareil va avec son JPEG.
- **`tools/badge_studio.py`** : la télécommande et le gestionnaire de fichiers de la carte SD dans une seule fenêtre,
  comme qFlipper (onglets Écran et Fichiers, sur une seule connexion USB) ; le badge par l'USB ou par le Wi-Fi de son
  module ESP32-CAM (son point d'accès, les modules trouvés sur le réseau). `badge_remote.py` et `sd_files.py`
  marchent toujours seuls.
- `tools/make_dist.py` : le paquet d'une version (firmware, carte SD, outils, documentation HTML, sommes de contrôle).

**Améliorations**
- La liaison avec le module est un service partagé (appareil photo, télécommande) ; protocole 2 (le badge reste
  compatible avec un module en protocole 1 pour l'appareil photo).
- `sd_files.py` : les erreurs courantes de la carte en clair (pas de carte, le badge ne répond pas).
- La documentation : l'appareil photo (fonctionnement, branchement sur les ports SAO de la carte V1.2, dépannage), les
  ports SAO dans l'électronique ; le site web montre les nouveautés et garde le firmware 1.1.0 en secours.

**Essayé sur le matériel** : l'appareil photo (viseur, photo, vidéo) sur une carte V1.2 (avant le passage au service partagé) ; le
thème Flipper (les animations depuis la carte SD, les boutons, la veille) ; `badge_studio.py` par l'USB (l'écran, la
copie de 1 Mo sur la carte pendant que l'écran est envoyé, un badge sans carte).

**Pas encore essayé sur le matériel** : la télécommande web (point d'accès, réseau, page admin), la mise à jour du
module depuis la carte SD (le module doit d'abord être flashé une fois avec l'ESP Flasher : nouvelle table des
partitions), l'appareil photo depuis le passage au service partagé, `badge_studio.py` par le Wi-Fi d'un vrai module.
En cas de problème, le firmware 1.1.0 reste disponible.


## 1.1.0 — 2026-10-20

Depuis `v_1.0.0` :

**Nouveautés**
- **Badge conférence** : le programme de n'importe quel événement depuis la carte SD (`CONFERENCES/`), avec 10
  événements français en exemple.
- **Mini-bolides** : course de petites voitures de 2 à 8 badges, 4 circuits choisis par le créateur de la partie
  (facile à expert). Les LEDs donnent la place, puis le gagnant (vert clignotant) et les autres (rouge en fondu).
- **Détecteur de cigales** (Social) : chaque cigale captée est signalée (son, LEDs, message).
- **Radar des cigales** : la cigale suivie répond environ 4 fois par seconde (on trouve la direction en tournant sur
  soi) ; Y fait clignoter ses yeux en vert, X la fait chanter (« Une cigale te cherche ! »).
- **Identifiant sonore** (Badge) : le nom du badge joué en FSK sur le buzzer.
- **Mon grade** : 10 grades de pirate gagnés avec l'expérience, avec leur portrait.
- **Chœur** : 6 nouvelles chansons à plusieurs voix.
- **Carte SD par l'USB** : `tools/sd_files.py`, une fenêtre pour envoyer, récupérer, renommer, effacer des fichiers ;
  les dates des fichiers sont conservées.
- **Écran** : réglage du nettoyage (discret, complet, désactivé).

**Améliorations**
- Les boutons portent leur nom imprimé sur la carte : **Y A B X** (de gauche à droite), partout.
- Les jeux ne se quittent plus par erreur : appui long sur A.
- Les textes trop longs défilent lentement au lieu d'être coupés.
- Partout où l'on choisit un fichier, tout le dossier est accessible, par pages.
- L'écran ne clignote plus pendant l'utilisation (le rafraîchissement complet attend 3 s sans changement).
- Réseau : écoute avant d'émettre, période des balises adaptée à l'occupation, 32 voisines, débit réglable pour les
  essais de portée (10 kbit/s par défaut : la meilleure portée).
- Radar des cigales : échelle chaud / froid plus large ; radio pirate : meilleure portée.
- Radio : fréquence du quartz du CC1101 par défaut à 26 MHz (`CC1101_fXOSC`) : environ 1 m de portée en plus.
- Annonces : 20 emplacements, le programme de SecSea 2026 par défaut.
- `badge_remote.py` : écran redimensionnable, choix du badge, débit radio.
- Documentation : guides mis à jour, site web (`tools/docs_site.py`), étude et idées pour un module ESP32,
  électronique du badge.

**Corrections**
- Calibration de la batterie : le point 2 ne pouvait jamais être enregistré.
- Loup-garou : la description du rôle s'ouvrait toute seule ; dessins du villageois, du voleur et de Cupidon refaits.
- Le record d'un jeu (appui long sur B dans le menu) se ferme de nouveau avec n'importe quel bouton.

**Non vérifié** : le réseau entre un badge 1.0.0 et un badge 1.1.0 (le modem radio a changé de réglages) : mettre
à jour tous les badges en 1.1.0.

Firmware : `badge_menu-1.1.0.uf2` (tag `v_1.1.0`).


## 1.0.0 — 2026-10-04

Première version distribuée (tag `v_1.0.0`).
