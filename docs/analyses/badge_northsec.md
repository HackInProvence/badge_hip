# Analyse : le badge NorthSec (nsec/nsec-badge)

Source : <https://github.com/nsec/nsec-badge> (branche `master` = badge **2025**, branches et tags des éditions 2016 à
2024), et le dépôt plus récent **`nsec/badge-2026`** de la même organisation. Analyse faite d'après le code (octobre
2026). Ce qui n'a pas pu être vérifié est signalé en fin de document.

---

## 1. Le badge 2025 en bref

| Élément | Détail |
|---|---|
| Processeur | ESP32-C3 (RISC-V 160 MHz, 4 Mo de flash) |
| LEDs | 18 LEDs adressables SK6812 : 12 pour les animations, 6 pour une « jauge d'accréditation » |
| Boutons | 5 sur le circuit, 3 utilisés par le firmware (haut, bas, OK) |
| Infrarouge | émetteur + récepteur 38 kHz (codage type NEC) : l'appairage entre badges |
| Écran | OLED SSD1306 128 × 32 (I2C), présent dans le code et le schéma, absent du README |
| Extensions | 2 connecteurs SAO v1.69bis (6 add-ons dessinés : pansement, bombe, sac de billets, diamant, machine à sous, add-on CTF) |
| Station d'accueil | connecteur à bord de carte : le badge devient esclave I2C quand il est posé sur un « dock » |
| Alimentation | USB-C ou 3 piles AAA |
| Radio | **aucune** utilisée (pas de Wi-Fi ni de BLE en 2025) |

## 2. Deux firmwares dans la même flash

- **Conférence** (partition `factory`) et **CTF** (partition `ota_0`). La commande série `firmware_select conf|ctf`
  bascule de l'un à l'autre et redémarre.
- L'image CTF était flashée à la table des admins pendant le CTF : les participants découvrent les défis plus tard.
- Pas de mise à jour par le réseau.

## 3. Le firmware « conférence »

### Boutons
- **Haut / bas** : change l'animation des LEDs ; on ne peut choisir que les animations jusqu'à son **niveau social**
  (chaque niveau en débloque une).
- **OK court** : l'OLED alterne entre le logo et les statistiques (« Social Level N / Animation N / Sponsor N »).
- **OK long** : lance un échange infrarouge.

### 118 animations de LEDs
Trois familles, à images clés avec des couleurs interpolées :
- 7 **battements de cœur** (rouge, vert, bleu, blanc, jaune, cyan, magenta) ;
- 79 **cycles de couleurs** (« dark wave synth », arc-en-ciel pastel, respirations de chaque couleur, drapeau des
  fiertés, étincelles...), sur toutes les LEDs ou une sur deux, avec un décalage par LED (effet scintillant) ;
- 32 **étoiles filantes** (8 palettes, 1 à 8 étoiles, vitesses croissantes).

### L'appairage infrarouge (le réseau social)
1. OK long : attente aléatoire de 2 à 3 s (pour ne pas parler en même temps), puis une demande de synchronisation.
2. Quand deux badges s'entendent, celui au plus petit identifiant reçoit, l'autre envoie.
3. Paquets de 5 octets : type, identifiant (3 octets de l'adresse MAC), somme de contrôle XOR. Délai de 10 s.
4. L'OLED montre chaque étape : « Looking for peer », « Sending data... », « Complete! », « Timed out ».
5. Seul un badge **jamais rencontré** fait monter le niveau, de moins en moins vite : **+5** jusqu'au niveau 20, +4
   jusqu'à 75, +3 jusqu'à 125, +2 ensuite, plafond **200**.
6. Pas d'authentification (seulement le XOR).

### Sponsors et stations d'accueil
- Posé sur un dock (détection par deux broches), le badge éteint son OLED et écoute en I2C :
  - `0x69 <n>` : le sponsor n (1 à 14) est obtenu, enregistré dans un masque de bits ;
  - `0x45 <n>` : impose l'animation n.
- La **jauge d'accréditation** (6 LEDs vertes) monte avec le nombre de sponsors (seuils 1, 3, 5, 8, 11, 14).
- Les stands des sponsors deviennent ainsi des étapes à visiter.

### Anti-triche
- Le niveau est enregistré avec une **somme de contrôle dépendant de l'adresse MAC** : modifier la mémoire à la main
  remet le niveau à 0 au démarrage.

### Ligne de commande (USB, invite `nsec>`)
- Toujours : `help`, `clear`, `reboot`, `firmware_select`.
- Dans le firmware CTF seulement : `quantum`, `calibrate`, `qkd-init`, `qkd`, `qkd2`, `codenames`, `unlocksafe`.

## 4. Les défis du CTF 2025 (mécanismes, sans les flags)

| Défi | Principe |
|---|---|
| `quantum` | un **simulateur quantique** jusqu'à 8 qubits (portes X, Y, Z, H, CNOT, mesure, empreinte MD5 du vecteur d'état) |
| `calibrate` | construire des états précis (|−⟩, |+⟩|+⟩, paire de Bell, GHZ, état « cluster ») ; chaque étape est validée par l'empreinte, la progression s'affiche sur les 6 LEDs de la jauge |
| `qkd-init`, `qkd` | un échange de clé **BB84** avec une station (dock ESP32-S3) par un bus filaire : le joueur garde les bits dont les bases concordent, puis déchiffre |
| `qkd2` | la même chose avec du **bruit** : correction d'erreurs **Cascade** à la main (parités des blocs) avant de déchiffrer |
| `unlocksafe` | posé sur la station « coffre-fort », le badge envoie la clé de `qkd2` : la station ouvre une **vraie serrure** pendant 10 s ; avant, elle fait clignoter un second flag en ASCII sur les 8 LEDs de l'add-on CTF, câblées dans le désordre avec un bit inversé (il faut le schéma de l'add-on pour le lire) |
| `codenames` | 20 questions de **culture radioamateur**, 25 réponses dont des leurres (comme le jeu Codenames) : les réponses forment une clé de 25 bits vérifiée par une station placée **au bar** |

## 5. Outils
- Scripts de flashage : un firmware, les deux firmwares (conférence + CTF), effacement de la mémoire, flashage des
  stations.
- Binaires précompilés, compilation PlatformIO (ESP-IDF), intégration continue GitHub.
- Fichiers KiCad du badge, des add-ons et des stations.

## 6. Les éditions précédentes

| Année | Matériel | Points forts |
|---|---|---|
| 2016 | nRF51 (BLE) + STM32, OLED | menu, programme de la conférence, animal virtuel à soigner |
| 2017 | idem, piles AAA | identité, badges proches en BLE, effets de LEDs ; CTF : 4 défis dont une **machine virtuelle** à rétro-concevoir, clavier USB, mise à jour DFU |
| 2018 | nRF52 + STM32, NeoPixels, batterie Li-ion | **3D en fil de fer** sur l'OLED, LEDs pilotées en BLE, programme, badges proches |
| 2019 | idem, écran couleur, flash externe, SAO | lampe torche, économiseur d'écran, aide au soudage, snake, démineur, **mode zombie**, balises BLE (« bar de la résistance », propagande), diaporamas, ligne de commande |
| 2021 | ESP32, écran 240 × 240, Wi-Fi, buzzer | un **jeu de rôle** complet (« North Sectoria », une vingtaine de personnages, coffres, code Konami) avec les flags cachés dans le jeu |
| 2023 | ATmega328 + 16 NeoPixels, appairage filaire ; partie CTF en ESP32 | LEDs de tous les badges pilotées par un **maillage BLE** depuis un écran tactile de contrôle |
| 2024 | ESP32-S3, 4 SAO | **appairage en chaîne** par câble (plusieurs badges en ligne, bonus si nombreux) ; 3 firmwares dont un chargé quand l'add-on CTF (puce crypto ATECC) est branché ; défis IR, Modbus, rétro-conception, Wi-Fi ; jeu de réflexes |

## 7. Le badge 2026 (nsec/badge-2026) : le plus proche du nôtre

- **ESP32-S3** avec un écran **e-paper 200 × 200 noir et blanc (SSD1681, le même que le nôtre)**, une puce **NFC**
  (ST25R3916), 18 LEDs, 6 boutons, un **capteur de lumière**.
- **Quatre compteurs** de progression (0 à 255), enregistrés masqués par l'identifiant du badge avec un CRC :
  - **Social** : appairage **NFC** pair à pair, prouvé par HMAC-SHA256 (clé dérivée du firmware et de l'adresse MAC,
    défi avec un nombre aléatoire) ; résultat en couleur (vert : nouveau, jaune : déjà vu, rouge : échec) ; le
    profil (nom, pronoms, affiliation, contact, couleur) n'est partagé qu'avec l'accord de l'utilisateur ;
  - **Sponsor** : les stations lisent le badge en I2C (10 % de chances d'un bonus « chanceux ») ;
  - **Lumière** : le temps passé à la lumière, qui monte, tient 30 min au maximum, puis redescend ;
  - **Attraction** : non trouvé dans le code.
- Les LEDs montrent un compteur en barre de progression, un arc-en-ciel quand tout est au maximum.
- Un **portail Wi-Fi** sur son propre point d'accès pour modifier le profil et voir ses contacts ; le badge émule une
  **étiquette NFC** qui donne le Wi-Fi à un téléphone.
- Lecteur NFC (NDEF, MIFARE Classic).
- Animations de LEDs décrites en **JSON** (aurore, comète, lucioles, radar...).
- Un **simulateur sur PC** du firmware, un outil qui flashe tous les badges branchés, un éditeur d'animations en HTML.
- Un firmware « conférence seule » qui **refuse de compiler** avec le code du CTF (et un script qui le vérifie).
- Défis CTF : un **système industriel** (le badge en point d'accès + client MQTT, un mot de passe caché, une machine
  à états obscurcie à mener jusqu'à « ouvrir la vanne »), deux défis quantiques (VQE, QAOA) validés à une station,
  et un message qui ne sort que sur une broche série cachée.

## 8. Ce qui peut inspirer notre cigale

Notre badge a déjà un réseau social radio, des succès et niveaux, des jeux multi-badges, des défis crypto. Les idées
nouvelles :

1. **Deux firmwares** : un firmware « conférence » propre, puis le firmware « CTF » flashé ou débloqué plus tard, avec
   une garde à la compilation et un script qui vérifie qu'aucun défi ne fuit dans l'image « conférence ».
2. **Des stations physiques** (sponsors, bar, coffre-fort) qui donnent des points ou valident des défis : posées sur
   un connecteur, ou en radio 433 MHz pour nous. La station « coffre-fort » qui ouvre une vraie serrure marque les
   esprits ; celle du bar attire du monde au bar.
3. **Une progression infalsifiable** : le score et les succès avec une signature liée à l'identifiant du badge ;
   ceux qui la contournent ont résolu un défi caché.
4. **Des gains décroissants** par nouvelle rencontre (+5, +4, +3, +2) qui débloquent un contenu cosmétique
   (pour nous : cadres, titres, images de veille, animations de LEDs).
5. **Un appairage authentifié** avec un résultat clair (nouveau / déjà vu / échec) et un profil partagé seulement
   avec accord : notre échange de contacts pourrait ajouter la preuve HMAC.
6. **Une statistique liée à l'environnement** qui monte puis redescend : pour nous, le temps passé près des balises
   433 MHz des salles de talks (« assiduité »).
7. **Des défis « au schéma et à la sonde »** : des LEDs d'un add-on câblées dans le désordre, un message sur une
   broche série non documentée de nos connecteurs d'extension.
8. **Un parcours pédagogique** dans la ligne de commande : simulateur, puis échange de clé avec du bruit à corriger à
   la main ; en version radio 433 MHz pour nous (un échange de clé bruité entre deux cigales).
9. **Une grille de quiz qui forme une clé** (20 questions, 25 réponses dont des leurres), thème radio et 433 MHz,
   vérifiée par une station.
10. **Un « équipement industriel » à piloter** par radio (une station qui joue l'automate, une séquence à trouver).
11. **Outils** : un simulateur du firmware sur PC (nous avons déjà des tests sur PC), un flashage de tous les badges
    branchés à la fois, des contenus décrits par fichiers sur la carte SD.

## 9. Non vérifié
- Si l'OLED du badge 2025 était monté sur tous les badges (absent du README).
- Le rôle des 2 boutons inutilisés en 2025 (sans doute reset et démarrage).
- Le comportement des add-ons SAO (seuls les fichiers KiCad sont publiés) et le firmware réel des stations sponsors.
- Les éditions 2016 à 2024 ont été lues surtout par leurs README et la liste de leurs fichiers.
- Le compteur « Attraction » du badge 2026.
