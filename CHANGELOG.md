# Historique des versions

Le numéro de version est dans le fichier [VERSION](VERSION) ; la marche à suivre pour une nouvelle version est dans le
[guide développeur](docs/fr/guide_developpeur.md#faire-une-nouvelle-version).

Format : `MAJEUR.MINEUR.CORRECTIF`.
- **CORRECTIF** : des corrections, rien de nouveau.
- **MINEUR** : des fonctions nouvelles, compatibles avec les badges de la version précédente.
- **MAJEUR** : incompatible : le réseau radio des cigales change, ou les réglages enregistrés sont remis à zéro.


## 1.1.0 — en préparation (pas encore de tag)

Depuis `v_1.0.0` :

**Nouveautés**
- **Badge conférence** : le programme de n'importe quel événement depuis la carte SD (`CONFERENCES/`), avec 10
  événements français en exemple.
- **Mini-bolides** : course de petites voitures de 2 à 8 badges, 4 circuits choisis par le créateur de la partie
  (facile à expert). Les LEDs donnent la place, puis le gagnant (vert clignotant) et les autres (rouge en fondu).
- **Détecteur de cigales** (Social) : chaque cigale captée est signalée (son, LEDs, message).
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
- Annonces : 20 emplacements, le programme de SecSea 2026 par défaut.
- `badge_remote.py` : écran redimensionnable, choix du badge, débit radio.
- Documentation : guides mis à jour, site web (`tools/docs_site.py`), étude et idées pour un module ESP32,
  électronique du badge.

**Corrections**
- Calibration de la batterie : le point 2 ne pouvait jamais être enregistré.
- Loup-garou : la description du rôle s'ouvrait toute seule ; dessins du villageois, du voleur et de Cupidon refaits.
- Le record d'un jeu (appui long sur B dans le menu) se ferme de nouveau avec n'importe quel bouton.

**À vérifier avant de publier** : le réseau entre un badge 1.0.0 et un badge de cette version (le modem radio a
changé de réglages).


## 1.0.0 — 2026-10-04

Première version distribuée (tag `v_1.0.0`).
