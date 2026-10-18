# Étude : un module ESP32 pour la cigale

Document d'étude, pour discussion dans l'équipe. **Rien n'est développé** : ce sont des choix techniques proposés,
avec leurs limites sur le matériel actuel. Les usages imaginés sont dans [idees_esp32.md](idees_esp32.md).

Objectifs :
- **Bluetooth** : piloter le badge depuis une application Android / iOS : afficher son écran en grand, le
  télécommander, recevoir ses notifications (une cigale détectée, un message reçu...).
- **Internet** (Wi-Fi) : le badge devient l'écran d'un service web (météo, trajet, programme...) que l'ESP32
  interroge.
- **Des API** pour que l'ESP32 accède à l'écran, aux boutons, aux LEDs, au son, au réseau des cigales et à la carte SD
  (échange de fichiers).


## 1. Le matériel

### Quel ESP32

| Puce | Radio | Atouts | Limites |
|---|---|---|---|
| **ESP32-C3** (recommandé) | Wi-Fi 4, BLE 5 | petit (cartes « SuperMini » 22 × 18 mm, XIAO), peu cher (2-5 €), consomme peu, RISC-V, Arduino et ESP-IDF | 400 Ko de RAM, pas de Bluetooth classique |
| ESP32-C6 | Wi-Fi 6, BLE 5, Thread / Zigbee | mêmes formats (XIAO), Matter possible | plus récent, moins d'exemples |
| ESP32-S3 | Wi-Fi 4, BLE 5 | PSRAM (8 Mo), USB, plus de calcul (rendu d'images, petits modèles d'IA) | plus gros, consomme plus |
| ESP32 classique | Wi-Fi 4, BLE 4.2, Bluetooth classique | SPP (port série Bluetooth) | plus gros, plus gourmand |

Le C3 suffit : le rôle de l'ESP32 est une passerelle (BLE et Wi-Fi vers une liaison série), pas du calcul.
Le S3 se justifie seulement pour rendre des images côté module (cartes, photos tramées).

### Le branchement : le port J2

Le port droit **J2** (2 × 6) porte l'**UART0** du RP2040, inutilisé par le firmware (la console est sur l'USB :
`pico_enable_stdio_uart(badge_menu 0)`) :

| J2 | Signal | Vers l'ESP32 |
|---|---|---|
| 1 | +3,3 V MAIN | 3V3 |
| 11, 12 | GND | GND |
| 9 | UART0 TX (GPIO12) | RX |
| 7 | UART0 RX (GPIO13) | TX |
| 2 | GPIO20 (aujourd'hui : réception IR) | option : ligne « réveil / données prêtes » |

- **Conflit avec l'infrarouge** : la carte IR est aussi sur J2 (GPIO20 et GPIO17). Soit une carte d'extension qui
  porte l'ESP32 **et** l'IR (le plus simple pour les utilisateurs), soit un connecteur traversant.
- **Alimentation** : le 3,3 V vient du TPS63000 (buck-boost, ~1 A sous 3,3 V depuis la batterie). Les pointes du
  Wi-Fi d'un C3 (~350 mA en émission) passent, mais **à valider à l'oscilloscope** (chute de tension pendant un
  rafraîchissement de l'écran + émission CC1101 + Wi-Fi). Prévoir 100-220 µF près du module.
- **Couper le module** : aucun GPIO libre ne commande son alimentation. L'ESP32 se met lui-même en sommeil profond
  sur ordre du badge (commande `SLEEP`) et se réveille par sa broche RX ou un timer ; sur une future carte V1.3,
  un transistor de coupure piloté par un GPIO serait mieux.
- **Pas de SAO** : J2 n'est pas au format SAO (voir [idees_reseau_extensions_ctf.md](idees_reseau_extensions_ctf.md)) ;
  une carte J2 → SAO est une autre piste, indépendante.

### Consommation (ordres de grandeur à mesurer)

| Mode de l'ESP32-C3 | Courant |
|---|---|
| Sommeil profond | ~5-50 µA (selon la carte : LED, régulateur) |
| BLE, connecté, au repos | ~10-30 mA (moyenne, selon l'intervalle de connexion) |
| Wi-Fi associé, au repos (modem sleep) | ~20-30 mA |
| Wi-Fi en échange | 80-100 mA, pointes ~350 mA |

Le badge seul consomme peu (e-Paper, radio en réception ~15 mA). Un module Wi-Fi allumé en permanence diviserait
l'autonomie : le mode normal doit être **BLE au repos, Wi-Fi à la demande** (une requête, puis coupure).
À mesurer avec Admin > Vider la batterie et `battauto` comme pour le badge seul.


## 2. La liaison série badge ↔ ESP32

### Transport

- UART **921 600 bauds**, 8N1 (le RP2040 et l'ESP32 le tiennent sans souci sur 5 cm). Une image de l'écran
  (200 × 200 × 1 bit = 5 000 octets) passe en ~55 ms.
- Des **trames** : `type (1) | séquence (1) | longueur (2) | données | CRC16`, délimitées par **COBS** (un 0 sépare les
  trames : resynchronisation immédiate après une erreur). Une trame fait au plus 1 Ko de données ; une image ou un
  fichier passe en plusieurs trames.
- Chaque requête reçoit une réponse `OK` / `ERREUR (code)` avec la même séquence ; les événements spontanés n'en
  attendent pas.
- Côté badge : une interruption UART remplit un tampon circulaire de **2 Ko** (la RAM est comptée : voir le
  `.map`), un service `esp_service()` dans la boucle principale décode les trames, comme `race_service()`.
- Au démarrage, le badge envoie `HELLO` (version du firmware, du protocole, nom, identifiant) ; sans réponse, le
  module est considéré absent et rien d'autre ne change.

### Les API

Les commandes viennent de l'ESP32 (et donc, à travers lui, du téléphone ou d'Internet). Le badge reste maître :
il peut refuser, et l'utilisateur voit toujours ce qui se passe.

| Domaine | Commandes (ESP32 → badge) | Événements (badge → ESP32) |
|---|---|---|
| **Système** | `HELLO`, `PING`, `INFO` (batterie, firmware, nom, langue, mode muet), `TIME` (l'heure NTP, pour l'horloge du badge), `SLEEP` (ordre au module) | `HELLO`, `BATTERY` (niveau, charge), `MUTE` |
| **Écran** | `SCREEN_GET` (l'image affichée), `SCREEN_PUSH` (une image 1 ou 2 bits, plein écran ou un rectangle), `WIDGET` (une page décrite : titre, lignes, jauge, QR code, pied de page ; le badge la dessine avec ses polices et ses traductions) | `SCREEN` (l'image après chaque rafraîchissement, si le miroir est demandé), `PAGE` (le titre de la page ouverte, comme les lignes `ui:`) |
| **Boutons** | `BUTTON` (appuyer A / B / X / Y, court ou long : la télécommande, les mêmes touches que `badge_remote.py`), `KEY` (un caractère pour l'éditeur de texte) | `BUTTON` (les appuis, quand une page de l'ESP32 est ouverte) |
| **LEDs et son** | `LEDS` (couleurs ou animation, pendant N secondes), `TONE`, `RTTTL` (une sonnerie) ; ignorés en mode muet | — |
| **Notifications** | `NOTIFY` (titre, texte, son, couleur des LEDs, durée) : le même bandeau que les messages reçus | `EVENT` : cigale détectée (la ligne `detect:` d'aujourd'hui), message reçu, rencontre, succès, annonce, batterie faible |
| **Réseau des cigales** | `NET_MESSAGE` (envoyer un message de cigale, comme le menu Messages), `NET_NEIGHBOURS` (la liste des voisines) | `NET_RX` (un message reçu) |
| **Carte SD** | `FILE_LIST`, `FILE_STAT`, `FILE_READ` (fichier, position, taille), `FILE_WRITE` (fichier, position, données ; à la fin : `FILE_CLOSE`), `FILE_DELETE`, `MKDIR` | `FILE_CHANGED` |

Choix importants :
- **`WIDGET` plutôt que des pixels** pour les pages d'information : 100 octets au lieu de 5 000, le texte reste
  net, traduit, et défile comme les autres pages. `SCREEN_PUSH` reste pour ce qui est vraiment graphique (une
  carte, un graphique, une photo tramée par le téléphone).
- **Un rafraîchissement d'e-Paper prend du temps** (un rafraîchissement partiel prend quelques centaines de ms, un complet plus d'une seconde) : le miroir de l'écran et
  les pages poussées sont limités à ~1 image par seconde, le badge regroupe les demandes.
- **La radio** : seulement les messages du réseau des cigales, avec ses règles (écoute avant d'émettre, rapport
  cyclique). Pas d'émission radio arbitraire commandée de l'extérieur.
- **La carte SD** est partagée avec l'écran sur le SPI0 : le badge sérialise les accès (aucun accès pendant un
  rafraîchissement). Débit attendu : ~50-80 Ko/s par la liaison série (une vidéo `.epv` de 1 Mo en ~20 s).

### Sécurité

- **Appairage BLE** avec un code à 6 chiffres affiché sur l'e-Paper (« passkey entry ») : impossible de piloter un
  badge sans l'avoir en main. Liaison chiffrée, appareils appairés retenus par l'ESP32 (2 ou 3 au plus).
- **Bac à sable sur la carte SD** : l'ESP32 n'écrit que dans `ESP32/` et dans les dossiers de contenu (`IMAGES`,
  `MUSIQUE`, `TEXTES`, `CONFERENCES`...), jamais à la racine par défaut ; la lecture peut être plus large. Les noms sont vérifiés (`..`, longueur).
- **Interrupteur côté badge** : Réglages > Module ESP32 : activé / désactivé, télécommande autorisée ou non, accès
  à la carte SD autorisé ou non (comme Réglages > Télécommande pour l'USB).
- Pas de mise à jour du firmware du badge par l'ESP32 (le RP2040 se flashe par l'USB) ; le firmware de l'ESP32 se
  met à jour par le Wi-Fi (OTA signé) ou par son USB.
- Les secrets (Wi-Fi, clés d'API) restent dans l'ESP32 (NVS chiffrée), jamais sur la carte SD ni à l'écran.


## 3. Côté badge (firmware RP2040)

- `src/esp/` : le pilote UART, les trames COBS, le décodage ; `src/menu/esp.c` : le service et ses pages.
- **Réglages > Module ESP32** : présent ou non, version, état du Bluetooth (appairé, connecté) et du Wi-Fi, les
  autorisations ; appairage (affiche le code).
- **Badge > Écran du module** : la page où l'ESP32 dessine (`WIDGET`, `SCREEN_PUSH`) et reçoit les boutons ; elle
  utilise `shared_buf` (deux images de 5 000 octets) puisqu'elle est seule à l'écran.
- Les **notifications** passent par le bandeau existant (`notify()` de `main.c`), les **événements** par un point
  unique (là où `detect:` et les messages sont tracés aujourd'hui).
- RAM : ~2,5 Ko permanents (tampon de réception + état), le reste dans `shared_buf`.
- `badge_remote.py` gagne un mode « par l'ESP32 » (la même interface, par BLE au lieu de l'USB), et chaque commande
  série ajoutée va dans `SERIAL_COMMANDS`.


## 4. Côté ESP32

- Firmware **Arduino** (ou ESP-IDF) avec NimBLE (léger) ; un dépôt ou un dossier `esp32/` à part.
- **BLE** : un service GATT principal compatible **Nordic UART Service** (les trames du §2 telles quelles) : les
  applications génériques (nRF Connect, Serial Bluetooth Terminal) et Web Bluetooth marchent dès le premier jour ;
  plus tard, des caractéristiques dédiées (notification d'événement, image de l'écran).
- **Wi-Fi** : réglage du réseau par le BLE depuis l'application (pas de point d'accès à configurer) ; client
  HTTPS pour les API ; une tâche par « widget » (météo, trajet...) avec sa période ; le module coupe le Wi-Fi entre
  deux mises à jour.
- **Les services web** sont décrits dans un fichier de configuration (URL, période, champs à extraire, mise en
  page du widget) envoyé par l'application : ajouter un service ne demande pas de reflasher.


## 5. L'application mobile

| Option | Android | iOS | Effort | Remarque |
|---|---|---|---|---|
| Applications génériques (nRF Connect, Serial Bluetooth Terminal) | oui | oui | nul | pour tester le protocole |
| **Page web Web Bluetooth** (publiée avec le site du badge) | Chrome | non (sauf navigateur Bluefy) | ★ | pas d'installation ; pas de notification en arrière-plan |
| **Flutter** (`flutter_blue_plus`) | oui | oui | ★★★ | une seule base de code, notifications en arrière-plan, publication sur les stores (compte Apple payant) |

Recommandation : d'abord la **page Web Bluetooth** (rapide, sert aussi d'outil de développement), puis une
application **Flutter** si l'usage se confirme.

Fonctions de l'application :
- **Écran agrandi** : le miroir de l'e-Paper, agrandi et lisible (avec le texte de la page si `WIDGET`).
- **Télécommande** : les 4 boutons (court / long) et un clavier pour l'éditeur de texte du badge.
- **Notifications** du téléphone pour les événements du badge (cigale détectée, message reçu, annonce...).
- **Fichiers** : envoyer une image (tramée par le téléphone en 200 × 200), une musique, un livre-jeu, le programme
  d'une conférence (`CONFERENCES/`) ; récupérer des fichiers.
- **Réglages** : nom, carte de contact, Wi-Fi, services web du module.


## 6. Le mode « écran Internet »

Deux façons de montrer un service web sur le badge :
1. **Widgets** (recommandé) : l'ESP32 extrait les valeurs (JSON) et envoie une page `WIDGET` (« Météo », « 18 °C,
   pluie à 16 h », une jauge, un pied de page). Le badge la dessine : net, traduit, peu de données.
2. **Pixels** : l'ESP32 (un S3 de préférence) rend une image 200 × 200 et l'envoie par `SCREEN_PUSH` : tout est
   possible (graphique, carte), au prix du débit et de la lisibilité.

Le badge peut aussi afficher un widget sur l'**écran de veille** : la météo ou le trajet visibles sans rien toucher, mis à jour toutes les 10-15 minutes pour la batterie.


## 7. Étapes proposées

1. **Prototype matériel** : une carte de développement ESP32-C3 câblée sur J2 ; mesurer la tension pendant les
   pointes, la consommation par mode.
2. **Liaison série** : trames COBS, `HELLO` / `PING`, un ESP32 qui fait écho ; test hôte du codage des trames
   (comme `fsk`).
3. **Pont BLE** (Nordic UART) : télécommande et miroir de l'écran depuis nRF Connect, puis depuis la page Web
   Bluetooth.
4. **Notifications** : les événements du badge vers le téléphone.
5. **Wi-Fi et widgets** : un premier service (la météo, sans clé d'API : Open-Meteo).
6. **Carte SD** : échange de fichiers, bac à sable.
7. **Application Flutter**, si les étapes précédentes sont concluantes ; **carte d'extension** (ESP32 + IR) pour la
   V1.3 du badge, avec un transistor de coupure de l'ESP32.

## 8. Risques et questions ouvertes

- L'**autonomie** : à mesurer ; le Wi-Fi permanent est exclu.
- La **tenue du 3,3 V** pendant les pointes cumulées (Wi-Fi + CC1101 + e-Paper).
- Le **port J2 partagé** avec l'IR : une carte unique ou un connecteur traversant.
- La **RAM du badge** : le service doit rester sous ~3 Ko permanents.
- **iOS** : pas de Web Bluetooth dans Safari ; les notifications en arrière-plan demandent une vraie application.
- **Vie privée** : un badge visible en BLE peut être suivi ; adresse BLE aléatoire (privacy BLE), annonce
  désactivable, pas de nom en clair dans l'annonce.
