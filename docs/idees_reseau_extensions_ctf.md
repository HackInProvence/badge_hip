# Idées pour la cigale : réseau social, extensions et CTF

Document de réflexion, pour discussion dans l'équipe.
Rien de tout ça n'est implémenté : ce sont des propositions, avec leur faisabilité sur le matériel actuel
(RP2040, CC1101 433 MHz, écran e-Paper 200×200, buzzer, 2 LEDs WS2812, 4 boutons, carte SD en V1.1, ports J2/J3).


## 1. Réseau social de proximité entre cigales

### Principe

Chaque cigale émet régulièrement une courte balise radio.
Quand deux cigales se trouvent **très près l'une de l'autre** (quelques dizaines de centimètres) pendant quelques secondes,
elles « se rencontrent » : chacune enregistre l'autre et gagne des **points de sociabilisation**.

La proximité se mesure avec le **RSSI** (puissance reçue) du CC1101.
Pour qu'il discrimine bien les distances courtes, les balises sont émises à **faible puissance** (-20 à -10 dBm) :
à cette puissance, seul un badge très proche reçoit un signal fort.
Le RSSI est bruité (orientation, corps humain, réflexions), d'où la règle de « quelques secondes » :
on exige plusieurs balises au-dessus du seuil dans une fenêtre de temps.

### Contraintes

- **Réglementation (bande ISM 433,05-434,79 MHz, ETSI EN 300 220)** : 10 mW p.a.r. maximum et un rapport cyclique
  d'émission limité (10 % sur cette sous-bande). Une balise de ~20 octets à 9,99 kbit/s dure ~25 ms :
  une toutes les 2 s fait ~1,3 %, largement dans les clous.
- **Collisions** : avec 200 badges dans une salle, les balises se chevauchent parfois.
  Un délai aléatoire (±500 ms) entre les balises suffit, une balise perdue n'est pas grave.
- **Batterie** : le CC1101 en réception continue consomme ~15 mA.
  Le mode **Wake-On-Radio** du CC1101 (réception par intermittence, réveil sur préambule) réduit ça fortement.
- **Quartz** : les badges n'ont pas tous le même quartz (26 ou 27 MHz, voir le menu « Infos »).
  Il faut détecter le quartz au démarrage (déjà fait par le menu) pour que tous les badges soient sur la même fréquence.

### Identité

- Identifiant : les 8 octets de l'identifiant unique de la flash (`pico_get_unique_board_id`), ou un hash tronqué sur 4 octets.
- Pseudo : saisi via l'USB (ou choisi dans une liste avec les boutons), stocké en flash, affiché sur l'écran.

### Format des paquets (proposition)

Configuration radio : le preset GFSK 9,99 kbit/s déjà utilisé (compatible Flipper Zero, ce qui permet de l'observer),
longueur variable, CRC matériel du CC1101.

| Octets | Champ | Description |
|---|---|---|
| 1 | magic | `0xC1` (« CIgale ») |
| 1 | version / type | 4 bits de version, 4 bits de type (balise, poignée de main, message, station) |
| 4 | id | identifiant de l'émetteur |
| 1 | seq | compteur, pour ignorer les doublons |
| 1 | puissance | puissance d'émission utilisée, pour corriger le RSSI |
| 2 | score | score actuel (affichage chez les autres, classement) |
| n | données | selon le type : pseudo, preuve de rencontre, message... |

### Déroulement d'une rencontre

1. **Balise** (toutes les ~2 s, faible puissance) : `id`, `seq`, `score`.
2. **Détection** : le badge B reçoit au moins 3 balises de A au-dessus du seuil RSSI (ex. -40 dBm) en 10 s.
3. **Poignée de main** : B envoie à A une demande contenant un nombre aléatoire ; A répond avec son pseudo
   et une preuve (voir ci-dessous) ; B fait de même.
4. **Enregistrement** : chacun stocke l'autre (id, pseudo, date) en flash, les LEDs clignotent, le buzzer joue un chant de cigale,
   l'écran affiche « Nouvelle rencontre : pseudo (+10 points) ».

### Points

- +10 pour une nouvelle cigale rencontrée, +1 pour une cigale déjà connue (une fois par heure au plus).
- Bonus « essaim » : +5 si 3 cigales ou plus sont proches en même temps.
- Bonus « rare » : badges staff ou orateurs (type spécial), qui valent plus.
- Bonus de série : rencontres sur plusieurs jours de l'événement.

### Triche et sécurité

Toute clé présente dans le firmware peut être extraite (la flash se lit avec picotool) : **c'est un choix assumé**,
et même un challenge CTF (voir plus bas). Protections raisonnables :

- une rencontre n'est valide que si **les deux** badges l'ont enregistrée (preuve croisée : chacun signe l'id de l'autre
  avec un HMAC sur une clé de l'événement) ;
- une station de base (un badge relié à un PC, sur le modèle de `radio_source.c`) collecte les scores
  et peut détecter les incohérences (1000 rencontres en une minute, rencontres non réciproques).

### Stockage

Un secteur de flash (4 Ko) réservé à la fin de la flash de 16 Mo : ~400 rencontres de 10 octets.
Écriture par ajout (journal), pour ne pas effacer le secteur à chaque rencontre.

### Affichage et classement

- Menu « Cigales » : score, nombre de rencontres, dernières rencontres, cigales proches en ce moment (avec leur RSSI).
- Station de base : un badge en réception relié en USB à un PC, qui affiche un classement sur un écran de la salle.

### Étapes de réalisation suggérées

1. Balise + réception + affichage des cigales proches avec leur RSSI (valider le seuil de proximité sur de vrais badges).
2. Stockage des rencontres en flash et points.
3. Poignée de main et preuves croisées.
4. Station de base et classement.
5. Wake-On-Radio pour la batterie.


## 2. Usages des ports d'extension

Brochage relevé sur le schéma v1.0 :

| J3 (2×6) | | J2 (2×6) | |
|---|---|---|---|
| 1-2 : +3,3 V EXT | | 1 : +3,3 V MAIN | 2 : GPIO20 |
| 3 : I2C1 SCL | 4 : I2C1 SDA | 3 : I2C0 SCL | 4 : GPIO21 |
| 5 : SPI0 SCK | 6 : SPI0 MISO | 5 : I2C0 SDA | 6 : SPI0 CS1 |
| 7 : écran RST | 8 : écran BUSY | 7 : UART0 RX | 8 : SPI0 CS2 |
| 9 : SPI0 MOSI | 10 : écran D/C | 9 : UART0 TX | 10 : SPI0 CS3 |
| 11-12 : GND | | 11-12 : GND | |

Attention : sur la V1.1, **CS1 et GPIO21 sont utilisés par la carte SD** (sélection et détection) : il reste CS2 et CS3.

J3 reprend les signaux de l'écran : on peut y brancher **un autre écran** (par exemple un e-Paper plus grand)
ou des périphériques SPI. J2 offre un UART, un deuxième bus I2C, deux chip-selects SPI et deux GPIO libres.

### Idées de modules

- **Capteurs I2C** (quelques euros) : température/humidité (BME280), accéléromètre (secouer la cigale pour changer d'écran,
  détecter une chute), capteur de lumière (adapter les LEDs).
- **Infrarouge** (GPIO20/21) : télécommande universelle, communication entre badges par IR (plus directionnelle que la radio :
  « se regarder » pour se rencontrer), TV-B-Gone.
- **NFC/RFID** (PN532 en I2C ou RC522 en SPI sur CS2) : lire des badges d'accès, échanger des contacts en posant deux cigales
  l'une sur l'autre (proximité parfaite pour le réseau social).
- **LoRa** (SX1276 en SPI sur CS2/CS3) : messages à longue portée entre stands, chasse au trésor sur tout le site.
- **GPS** (UART) : chasse au trésor géolocalisée, horodatage.
- **Écran supplémentaire** : petit OLED I2C pour des infos rapides (l'e-Paper est lent), ou e-Paper plus grand sur J3.
- **Outil de hacking matériel** : le badge devient un adaptateur USB-UART/I2C/SPI (via l'USB CDC) pour les ateliers,
  ou un analyseur logique simple avec les PIO du RP2040 (GPIO20/21).
- **Carte cible CTF** : une petite carte avec son propre microcontrôleur à attaquer (UART de debug, EEPROM I2C à lire,
  glitch d'alimentation) branchée sur J2.
- **Standard de connecteur** : les ports ne sont pas compatibles SAO (Shitty Add-On, le standard des badges de conférence,
  2×3 broches : 3,3 V, GND, I2C, 2 GPIO). Un adaptateur J2 → SAO ouvrirait l'accès aux nombreux add-ons existants.


## 3. Challenges de type CTF

Chaque challenge exploite une capacité du badge. Les flags au format `SECSEA{...}` sont validés par un site ou la station de base.

### Débutant

- **Code Konami** : une séquence de boutons (↑↑↓↓ avec les flancs, ailes...) débloque un menu caché qui affiche un flag.
- **Morse sur les LEDs ou le buzzer** : un menu « Cigale bavarde » clignote ou chante un flag en morse.
- **Port série** : le badge affiche un message d'accueil sur l'USB ; une commande cachée (`help` ne la montre pas) donne un flag.
- **Carte SD** : un fichier caché, ou un flag dans une seule image d'une vidéo (image subliminale à 10 i/s).

### Intermédiaire

- **Rémanence de l'écran** : le flag est affiché une fraction de seconde en rafraîchissement rapide puis effacé partiellement ;
  le ghosting le laisse deviner (exploite le mode multiframe).
- **Deuxième plan mémoire** : en mode noir et blanc, l'écran n'affiche que la RAM « B/W » ; le flag est écrit dans la RAM « RED »,
  visible seulement en mode 4 niveaux de gris (menu caché ou en modifiant le firmware).
- **Spectrogramme** : un WAV de la carte SD cache un flag visible dans son spectrogramme (Audacity).
- **Radio - écoute** : le badge émet périodiquement un message sur une autre fréquence ou dans une autre modulation ;
  on le trouve avec l'analyseur de fréquence du Flipper puis on le décode (Flipper, SDR, ou un autre badge).
- **Radio - rejeu** : une « porte » (station de base) s'ouvre quand elle reçoit le bon paquet ; on capture celui d'un badge
  staff avec un Flipper et on le rejoue. Variante plus dure : code tournant (rolling code) à comprendre.
- **Quartz** : le challenge du bug trouvé pendant ce projet ! Un badge « mal calibré » émet le flag décalé de 16 MHz.

### Avancé

- **Dump du firmware** : `picotool save` puis rétro-ingénierie (Ghidra, ARM Thumb) pour retrouver une clé XORée ou un flag chiffré.
- **Clé du réseau social** : extraire la clé HMAC du firmware pour forger des rencontres (et comprendre pourquoi la station
  de base les détecte quand même).
- **Debug SWD** : les pastilles SWD du badge permettent d'attacher un debugger (openocd + une Pico en picoprobe)
  et de lire un secret qui n'existe qu'en RAM.
- **Attaque temporelle** : un mot de passe demandé sur l'USB est comparé caractère par caractère ; le temps de réponse,
  mesurable depuis le PC, révèle les bons caractères un par un.
- **Challenge social** : un flag découpé en morceaux, chaque morceau n'étant donné que par un badge d'un type (staff, orateur...) :
  il faut les rencontrer (réseau social) pour reconstituer le flag.

### Organisation

- Un menu « CTF » sur le badge : liste des challenges, saisie des flags avec les boutons, score local.
- Les challenges débutants dans le firmware d'origine, les avancés nécessitent d'outiller (Flipper, SDR, picotool, debugger) :
  bonne occasion d'ateliers.
- Attention aux challenges radio : rester dans la bande ISM et les limites de puissance/rapport cyclique,
  **pas de brouillage** (illégal et nuisible aux autres).
