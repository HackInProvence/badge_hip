# Brainstorm : ce que la cigale pourrait faire avec un module ESP32

Document de réflexion, **rien n'est développé**. Le module (Bluetooth BLE + Wi-Fi), son branchement et ses API
sont décrits dans [etude_module_esp32.md](etude_module_esp32.md). Ici : les usages, pour choisir lesquels faire.

Légende : effort ★ (une journée, avec les API du module en place) à ★★★ (gros projet) ;
🔑 demande une clé d'API (souvent gratuite, parfois payante) ; 📱 demande l'application du téléphone ;
🌐 demande le Wi-Fi.

Le principe commun : l'ESP32 interroge un service, garde quelques valeurs et envoie au badge une page « widget »
(titre, quelques lignes, une jauge ou une icône, une couleur de LEDs). Le badge l'affiche dans **Badge > Écran du
module**, en notification, ou sur l'écran de veille. Le Wi-Fi n'est allumé que le temps de la requête.


## 1. La vie de tous les jours

| # | Idée | Source possible | Effort |
|---|---|---|---|
| 1 | **Météo** : maintenant, les prochaines heures, « pluie dans 20 min » ; LEDs bleues s'il va pleuvoir | Open-Meteo (sans clé), Météo-France (API publique) | ★ 🌐 |
| 2 | **Trajet vers le bureau** : « Bureau : 23 min (+8 min de bouchons) », l'heure de départ conseillée, LEDs vertes / orange / rouges selon le trafic, une notification « partez maintenant » | Google Routes API, TomTom Routing, HERE (trafic en temps réel) 🔑 | ★★ 🌐 |
| 3 | **Prochains passages** du bus, du tram, du métro, du train à l'arrêt choisi ; retards et perturbations | Navitia, IDFM PRIM, SNCF, GTFS-RT des réseaux locaux 🔑 | ★★ 🌐 |
| 4 | **Vélos et trottinettes** en libre-service à la station la plus proche | GBFS (Vélib', Le vélo, ...) | ★ 🌐 |
| 5 | **Agenda** : le prochain rendez-vous, compte à rebours, rappel 10 min avant (LEDs, son) | un lien ICS, Google Calendar, CalDAV | ★★ 🌐 |
| 6 | **Horloge exacte** et réveil : l'heure NTP, alarme avec une sonnerie RTTTL | NTP | ★ 🌐 |
| 7 | **Qualité de l'air, pollens, UV** : une jauge, LEDs rouges en cas de pic | Open-Meteo Air Quality, Atmo France 🔑 | ★ 🌐 |
| 8 | **Vigilance et alertes** : orages, canicule, crues, alertes FR-Alert relayées | Météo-France vigilance, Vigicrues | ★★ 🌐 |
| 9 | **Marées, lever / coucher du soleil, phase de la Lune** (La Ciotat oblige) | calcul local, SHOM 🔑 | ★ 🌐 |
| 10 | **Liste de courses / tâches** partagée, cochée avec les boutons | Todoist, Google Tasks, un fichier sur un serveur 🔑 | ★★ 🌐 |
| 11 | **Notifications du téléphone** sur le badge (appel, SMS, messagerie) : pratique pendant un talk, téléphone rangé | iOS : ANCS (l'ESP32 le lit directement en BLE), Android : l'application 📱 | ★★ |
| 12 | **Retrouver son téléphone / son badge** : faire sonner l'un depuis l'autre, distance approximative par le RSSI | BLE 📱 | ★ |


## 2. Pendant une conférence

| # | Idée | Source possible | Effort |
|---|---|---|---|
| 13 | **Programme à jour** : les fichiers de `CONFERENCES/` téléchargés depuis le site de l'événement, changements de salle et d'horaire notifiés | un JSON / iCal publié par les organisateurs | ★★ 🌐 |
| 14 | **« Mon programme »** : marquer des talks sur le badge, rappel 5 min avant avec la salle | local + horloge NTP | ★★ |
| 15 | **Annonces des organisateurs** au-delà de la portée radio (autres bâtiments, la soirée) | un flux web de l'événement | ★ 🌐 |
| 16 | **Votes et questions** au talk depuis le badge, résultats en direct sur l'écran de la salle | un petit serveur de l'événement | ★★ 🌐 |
| 17 | **Tableau des scores du CTF** : son rang, le premier, « nouveau flag validé » | API CTFd | ★ 🌐 🔑 |
| 18 | **Réseau des cigales relayé** : les messages passent d'une salle à l'autre (et entre événements) par Internet, quelques badges-passerelles suffisent | un serveur relais | ★★★ 🌐 |
| 19 | **Échange de contacts vers le téléphone** : les cartes reçues par radio deviennent des contacts du téléphone (vCard) | 📱 | ★ |
| 20 | **Plan de l'événement** : « vous êtes ici » approché par le Wi-Fi (bornes connues) ou les cigales fixes des stands | positionnement Wi-Fi / RSSI | ★★★ |
| 21 | **Badge d'orateur connecté** : le chronomètre du Badge de talk piloté depuis le téléphone, le temps restant poussé par la régie | 📱 / serveur | ★ |
| 22 | **Stands** : « vous êtes passé à 12 stands sur 30 », tampon numérique par BLE au lieu du papier | BLE des stands | ★★ |


## 3. Hacking pédagogique (dans les règles)

| # | Idée | Source possible | Effort |
|---|---|---|---|
| 23 | **Scanner Wi-Fi** : réseaux visibles, canaux encombrés, chiffrement (WPA2 / WPA3 / ouvert) : un atelier sur la sécurité des réseaux | balayage passif de l'ESP32 | ★ |
| 24 | **Scanner BLE** : les appareils autour, leurs annonces ; **détecteur de traceurs** (AirTag, SmartTag, Tile) qui vous suivent | balayage passif | ★★ |
| 25 | **Atelier BLE** : le badge expose un service GATT volontairement faible, à attaquer (CTF), sans risque pour personne | ESP32 | ★★ |
| 26 | **Avions au-dessus** : vol, altitude, compagnie | OpenSky Network, ADS-B Exchange | ★ 🌐 |
| 27 | **Satellites et ISS** : prochain passage visible, durée | Celestrak + calcul, open-notify | ★★ 🌐 |
| 28 | **Veille sécurité** : la dernière CVE critique, le CERT-FR, les bulletins ANSSI | flux RSS / API NVD | ★ 🌐 |
| 29 | **Météo de l'espace** : aurores, éruptions solaires (pour les radioamateurs) | NOAA SWPC | ★ 🌐 |


## 4. Écran, image et son

| # | Idée | Source possible | Effort |
|---|---|---|---|
| 30 | **Écran agrandi** sur le téléphone, captures d'écran du badge en un geste | miroir de l'écran 📱 | ★ |
| 31 | **Photo vers l'e-Paper** : le téléphone recadre et trame la photo en 200 × 200, envoyée à la carte SD | 📱 | ★★ |
| 32 | **Morceau en cours** (Spotify, Deezer, la radio) : titre, artiste, pochette tramée | API Spotify 🔑 | ★★ 🌐 |
| 33 | **Cadre e-Paper** : une image du jour (NASA APOD, une BD, un dessin d'XKCD) | flux publics | ★ 🌐 |
| 34 | **Citation, blague, mot du jour, fait historique** | API publiques | ★ 🌐 |
| 35 | **Podcasts et radios web** : non (pas de son de qualité sur le buzzer), mais les **titres** en cours d'une webradio | métadonnées ICY | ★ 🌐 |


## 5. Maison et objets connectés

| # | Idée | Source possible | Effort |
|---|---|---|---|
| 36 | **Home Assistant** : température des pièces, porte ouverte, sonnette (notification + LEDs), boutons du badge comme interrupteurs | API Home Assistant, MQTT 🔑 | ★★ 🌐 |
| 37 | **Consommation électrique** du jour, prix de l'électricité (jours Tempo rouges en LEDs rouges) | Linky (via Home Assistant), API Tempo d'EDF | ★★ 🌐 |
| 38 | **Imprimante 3D** : avancement, fin d'impression notifiée | OctoPrint, Bambu (MQTT local) | ★ 🌐 |
| 39 | **Serveurs et CI** : état d'un service, « build cassé » en LEDs rouges, alertes de supervision | GitHub Actions, GitLab, Uptime Kuma, Grafana | ★ 🌐 🔑 |
| 40 | **Matter / Thread** (avec un ESP32-C6) : le badge comme télécommande d'objets Matter | ESP32-C6 | ★★★ |


## 6. Social et jeux

| # | Idée | Source possible | Effort |
|---|---|---|---|
| 41 | **Mentions sur les réseaux** : les messages au hashtag de l'événement (Mastodon, Bluesky), une notification quand on vous mentionne | API Mastodon, Bluesky (AT Protocol) | ★★ 🌐 |
| 42 | **Classements en ligne** des jeux du badge (2048, Snake, Simon) entre tous les badges, même chez soi | un petit serveur | ★★ 🌐 |
| 43 | **Jeux multi à distance** : Puissance 4, Tic-tac-toe, Duel avec un badge à l'autre bout du monde | un serveur relais | ★★★ 🌐 |
| 44 | **Chasse au trésor géante** : indices débloqués par l'heure, le lieu (Wi-Fi), les cigales croisées | serveur de jeu | ★★★ 🌐 |
| 45 | **Assistant** : une question choisie dans une liste (ou dictée au téléphone), la réponse courte d'un modèle de langue affichée en défilement | API d'un modèle de langue 🔑 | ★★ 🌐 📱 |
| 46 | **Traduction** d'un message de cigale reçu dans la langue du badge | API de traduction 🔑 | ★★ 🌐 |


## 7. Appareil photo (module avec caméra et flash)

Le module caméra est décrit dans [etude_module_esp32.md](etude_module_esp32.md) § 7. 📷 = demande la caméra.

| # | Idée | Effort |
|---|---|---|
| 53 | **Appareil photo e-Paper** : viseur tramé sur l'écran, photo sur la carte, à revoir dans Médias > Images | ★★ 📷 |
| 54 | **Photomaton de badge** : une photo de soi devient le badge nominatif (portrait tramé + nom) | ★★ 📷 |
| 55 | **Lecteur de QR codes** : scanner les QR du programme, des stands, des scores des autres cigales, une carte de contact | ★★ 📷 |
| 56 | **Échange de contacts par QR** : la cigale affiche sa carte en QR, l'autre la photographie | ★ 📷 |
| 57 | **Scanner de documents** : une diapo, une affiche, un plan de salle, recadrés et gardés sur la carte | ★★ 📷 |
| 58 | **Lecture de texte** (OCR, par une API ou localement) : le titre d'une diapo ou d'une affiche devient une note | ★★ 📷 🌐 |
| 59 | **Filtres e-Paper** : trames (points, lignes, ASCII art), 4 niveaux de gris, contours : un rendu « gravure » | ★ 📷 |
| 60 | **Time-lapse et stop-motion** : une image toutes les N secondes, rejouée en vidéo `.epv` sur le badge | ★★ 📷 |
| 61 | **Détecteur de mouvement** : la cigale garde le stand, LEDs et son quand quelqu'un passe | ★★ 📷 |
| 62 | **Chasse au trésor photo** : retrouver un lieu ou un objet à partir d'une image tramée | ★ 📷 |
| 63 | **Jeu de couleurs** : trouver un objet de la couleur demandée (la caméra mesure la teinte), LEDs de la même couleur | ★★ 📷 |
| 64 | **Webcam Wi-Fi** de la cigale pour un live du stand, avec un voyant allumé | ★★ 📷 🌐 |

## 8. Idées farfelues 🤪

| # | Idée | Effort |
|---|---|---|
| 47 | La cigale **chante plus fort quand il fait chaud** dehors (la météo réelle règle le chant) | ★ 🌐 |
| 48 | **Humeur du badge** selon la bourse, la météo et le nombre de cigales croisées | ★ 🌐 |
| 49 | **Pomodoro collectif** : toutes les cigales d'une équipe en pause en même temps, même à distance | ★★ 🌐 |
| 50 | **Compte à rebours** de la prochaine SecSea, de l'apéro, des vacances | ★ |
| 51 | **Tamagotchi connecté** : la cigale a faim quand il pleut, s'ennuie si on ne croise personne | ★★ 🌐 |
| 52 | **Cigale-boussole** vers une autre cigale sur Internet (sa position partagée) | ★★ 🌐 📱 |


## Pistes pour le tri

- **Avec la caméra** : appareil photo (53), photomaton (54), lecteur de QR codes (55), dans cet ordre.
- **Premier lot** (peu d'effort, montre tout le chemin : Wi-Fi, widget, LEDs, notification) : météo (1), horloge
  (6), notifications du téléphone (11), écran agrandi (30), scanner Wi-Fi (23).
- **Le plus utile au quotidien** : trajet (2), transports (3), agenda (5), Home Assistant (36).
- **Le plus utile pendant une conférence** : programme à jour (13), mon programme (14), annonces (15), CTF (17).
- **À cadrer avant** : tout ce qui touche à la vie privée (positions, scanners BLE, contacts) : traitement local,
  rien n'est envoyé sans le dire, données effacées à la demande ; les clés d'API restent dans l'ESP32.
- Les services payants (Google Routes, certaines API de trafic) : prévoir une alternative gratuite ou une clé
  fournie par l'utilisateur.
