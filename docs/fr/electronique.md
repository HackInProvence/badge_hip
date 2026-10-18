# L'électronique du badge

*English version: [electronics.md](../en/electronics.md).*

Ce document décrit la carte du badge (V1.1, projet KiCad dans [hardware/kicad](../../hardware/kicad), schéma PDF de
la V1.0 dans [hardware](../../hardware)) **par fonction** : à quoi sert chaque composant. Les repères (R24, U6...)
sont ceux du schéma ; les liaisons ont été relevées dans le fichier du circuit imprimé. Les GPIO sont ceux de
[src/pinouts.h](../../src/pinouts.h).

```
 USB-C ──► alimentation (charge TP4056, partage USB / batterie, abaisseur-élévateur 3,3 V) ──► tout le badge
                 │
 Batterie ──────►┘──► mesure (pont + LM321) ──► ADC
                                     ┌─► écran e-Paper (SSD1681) ─┐
 RP2040 + flash 16 Mo ── SPI0 ──► 74HC139 ┼─► carte micro-SD          ├─ ports J2 / J3
                     ── SPI1 ──► CC1101 ─► filtre ─► antenne 433 MHz
                     ── GPIO ──► 4 boutons, 2 LEDs WS2812B, buzzer, IR (J2), I2C (J3)
```


## 1. Alimentation

### Entrée USB-C (J1)

- **J1** : prise USB-C (USB 2.0) : l'alimentation 5 V (VBUS) et les données (le port série du badge, le flashage).
- **R5, R6** (5,1 kΩ) : tirent CC1 et CC2 à la masse : le badge se présente comme un appareil alimenté (« sink »),
  le chargeur lui donne 5 V.
- **R1, R2** (27,4 Ω) : résistances série de D+ et D− vers le RP2040 (adaptation de la ligne USB, valeur du guide
  de conception du RP2040).

### Charge de la batterie (U5 TP4056)

- **H1** : connecteur de la batterie Li-ion / LiPo (JST PH 2,0 mm, 2 broches).
- **U5 TP4056** : chargeur linéaire Li-ion (4,2 V). Il est alimenté par VBUS à travers **R20** (0,1 Ω) et découplé
  par **C37** (10 µF) ; sa sortie BAT va à la batterie (**C38**, 10 µF).
- **R21** (1,2 kΩ) sur PROG : fixe le **courant de charge à ~1 A** (I = 1200 / R_PROG en kΩ, datasheet du TP4056).
  La batterie doit le supporter (capacité de 1 000 mAh ou plus conseillée).
- **D6** (rouge) avec **R19** (2,4 kΩ) sur CHRG : **en charge** ; **D7** (verte) avec **R27** sur STDBY :
  **chargée**.

### Partage USB / batterie (D2, Q3)

Le badge fonctionne sur USB ou sur batterie, sans couper la charge :
- **D2** (Schottky MBR0530) : avec l'USB, le 5 V alimente le badge (la ligne « système », VSYS) à travers elle.
- **Q3** (SI2301, MOSFET P) entre la batterie et VSYS, sa grille sur VBUS : **sans USB**, **R26** (100 kΩ) tire la
  grille à la masse, Q3 conduit et la batterie alimente le badge ; **avec l'USB**, la grille est à 5 V, Q3 est
  bloqué : la batterie ne fait que se charger.
- **C39** (47 µF) : réservoir de VSYS.

### Régulateur 3,3 V (U6 TPS63000) et interrupteur

- **U6 TPS63000** : convertisseur abaisseur-élévateur (« buck-boost ») : 3,3 V stables que la batterie soit à 4,2 V
  ou à 3 V (jusqu'à ~1 A sous 3,3 V). Inductance **L7** (2,2 µH), sorties **C40, C41** (47 µF), **C28** (4,7 µF).
- **R28** (1 MΩ) / **R29** (178 kΩ) : pont de retour qui fixe la sortie à 0,5 × (1 + 1000 / 178) ≈ **3,31 V**.
- **R30** (100 Ω) et **C43** : la broche PS/SYNC reliée (filtrée) à l'entrée : d'après la datasheet du TPS63000, le
  **mode économie est désactivé** (fréquence fixe : moins d'ondulation, mais un peu plus de consommation au repos ;
  une piste pour l'autonomie d'une future carte).
- **SW7** (interrupteur à glissière) et **R31** (100 kΩ) : R31 tire EN vers VSYS (marche) ; sur OFF, SW7 met EN à
  la masse : **le régulateur est coupé, le badge éteint, même branché en USB** ; la charge continue (le TP4056 est
  avant le régulateur).

### Mesure de la batterie (U7 LM321)

- **R24** (100 kΩ) / **R25** (200 kΩ) : pont diviseur : 2/3 de la tension de la batterie.
- **U7 LM321** : amplificateur monté en suiveur (sa sortie sur son entrée −), alimenté par la batterie, vers
  **ADC_VBAT** (GPIO29, ADC3 du RP2040). Points de test **TP8** (ADC_VBAT), **TP9** (3,3 V), **TP10** (masse).
- **Limite connue** : le LM321 n'est pas « rail-à-rail » : sa sortie ne dépasse pas ~V_bat − 1,5 V alors que
  l'entrée vaut 2/3 V_bat : il sature, d'où la calibration logicielle (guide développeur § 6.9). Pour une prochaine
  carte : un amplificateur rail-à-rail (MCP6001...) ou le pont directement sur l'ADC.


## 2. Le microcontrôleur (U1 RP2040)

- **U1 RP2040** : deux cœurs Cortex-M0+ à 133 MHz, 264 Ko de RAM. Découplage : **C1–C7, C12, C14, C15, C19, C20,
  C42, C44** (100 nF / 1 µF) près de chaque broche d'alimentation.
- **Régulateur interne 1,1 V** du cœur : **C13** (1 µF), **C8, C9** (100 nF) sur VREG_VOUT.
- **X1** (quartz 12 MHz, ABM8-272-T3), **C10, C11** (15 pF), **R16** (1 kΩ, limite le courant dans le quartz) :
  l'horloge du RP2040 et de l'USB.
- **U3 W25Q128JV** : la mémoire flash de 16 Mo (QSPI) : le firmware, les réglages, les contacts, les annonces.
  **R4** (10 kΩ) tire QSPI_SS au 3,3 V.
- **SW6 BOOTSEL** (avec **R3**, 1 kΩ) : met QSPI_SS à la masse au démarrage : le RP2040 démarre en mode flashage
  (disque « RPI-RP2 »).
- **SW1 RESET** : met RUN à la masse (redémarrage) ; **R7** (100 kΩ) tire RUN au 3,3 V.
- **Débogage SWD** : points de test **TP2** (SWDIO), **TP3** (SWCLK), tirés par **R22, R23** (10 kΩ) ; **TP1**
  (3,3 V), **TP4** (masse), **TP5** (RUN).


## 3. Le bus SPI0 partagé : écran, carte SD, extensions (U2 74HC139)

L'écran, la carte SD et deux emplacements des extensions partagent le bus SPI0 (SCK GPIO6, MOSI GPIO7, MISO GPIO4).
- **U2 74HC139** (un des deux décodeurs 2 vers 4) : son entrée de validation est le chip-select matériel du SPI0
  (GPIO5) ; ses deux adresses viennent de **GPIO18** et **GPIO19**. Il choisit l'un des quatre : **Y0 l'écran**,
  **Y1 la carte SD**, **Y2 et Y3 le port J2** (CS2, CS3). Trois périphériques avec une seule broche de sélection.
- **R32, R33, R34** (4,7 kΩ) : tirages de MOSI, SCK et MISO au 3,3 V (la carte SD les demande en mode SPI).


## 4. L'écran e-Paper (J4)

- **J4** : connecteur FPC 24 broches de l'écran **e-Paper 1,54" 200 × 200** (contrôleur **SSD1681**), sur SPI0 (CS
  par Y0 du décodeur), avec **DC** (GPIO8), **BUSY** (GPIO9), **RST** (GPIO10).
- **Le circuit d'alimentation de l'écran** (le circuit de référence de ces écrans) : l'e-Paper a besoin de tensions de
  pilotage d'environ ±20 V, fabriquées à partir du 3,3 V par un petit élévateur que le SSD1681 commande lui-même :
  - **L6** (10 µH) et **Q2** (SI1308, MOSFET N) commandé par la broche GDR de l'écran ;
  - **R18** (3 Ω) : mesure du courant de Q2 (broche RESE) ; **R17** (10 kΩ) : tient la grille de Q2 à la masse
    au repos ;
  - **D3, D4, D5** (Schottky MBR0530) et **C27** (4,7 µF) : redresseur et pompe de charge qui donnent **PREVGH**
    (positive) et **PREVGL** (négative), stockées dans **C29** et **C35** (1 µF) ;
  - **C30, C32, C33, C34, C36** (1 µF) : découplage des autres tensions internes de l'écran (VSH, VSL, VCOM,
    régulateurs internes).


## 5. La carte micro-SD (J5)

- **J5** : lecteur micro-SD avec détection de carte, sur SPI0 (CS par Y1 du décodeur).
- **SD_DETECT** (GPIO21), tiré au 3,3 V par **R35** (100 kΩ) : présence de la carte (absente sur la V1.0).


## 6. La radio 433 MHz (U4 CC1101)

- **U4 CC1101** : émetteur-récepteur sub-GHz (300–928 MHz, ici 433 MHz) sur **SPI1** (SCK GPIO26, MOSI GPIO27,
  MISO GPIO24, CS GPIO25), avec ses sorties d'état **GDO0** (GPIO22) et **GDO2** (GPIO23).
- **X2** (quartz 26 MHz) et **C17, C18** (27 pF) : l'horloge de la radio (le firmware mesure le quartz au démarrage :
  certains badges avaient un 27 MHz).
- **R15** (56 kΩ) sur RBIAS : la référence de courant interne (valeur imposée par TI) ; **C16** (100 nF) sur
  DCOUPL : le régulateur interne de la radio.
- **Le balun et le filtre** (valeurs de la conception de référence de TI pour 433 MHz) : la sortie radio est
  différentielle (RF_P, RF_N), l'antenne est simple :
  - **L1, L2** (27 nH), **C22, C23** (3,9 pF) : le balun, qui transforme le différentiel en simple et adapte
    l'impédance ;
  - **C21** (220 pF) : découplage du point milieu ;
  - **L3** (22 nH), **C24** (8,2 pF), **L4** (27 nH), **C25** (5,6 pF) : filtre passe-bas (élimine les harmoniques
    de l'émission) ;
  - **C26** (220 pF) : condensateur de liaison vers l'antenne.
- **L5 ACAG1204-433** : antenne céramique CMS de 12 × 4 mm pour 433 MHz. Sa portée dépend beaucoup du plan de masse
  et de son dégagement : ~7 m mesurés entre deux badges V1.2 à 10 kbit/s (moins sur la V1.0, dont l'antenne est
  moins bien placée).


## 7. Les boutons

Quatre boutons-poussoirs entre le 3,3 V et un GPIO, **actifs à l'état haut**, chacun tiré à la masse par une
résistance de 10 kΩ :

| Bouton | Emplacement | Composant | GPIO | Résistance |
|---|---|---|---|---|
| **Y** | flanc gauche | SW3 (K2-1112SW, latéral) | 0 | R11 |
| **A** | aile gauche | SW4 (XDTD-6650) | 1 | R12 |
| **B** | aile droite | SW5 (XDTD-6650) | 14 | R13 |
| **X** | flanc droit | SW2 (K2-1112SW, latéral) | 15 | R10 |

De gauche à droite sur la carte : **Y A B X**. SW1 (RESET) et SW6 (BOOTSEL) sont au § 2.


## 8. Les LEDs (LED1, LED2 WS2812B)

- **LED1, LED2 WS2812B-2020** : deux LEDs RGB adressables (2 × 2 mm) en chaîne : GPIO11 → **R14** (330 Ω, protège
  l'entrée de données) → LED1 → LED2. Pilotées par le PIO du RP2040. Alimentées en 3,3 V.
- **TP6** (sortie de données de LED2) et **TP7** (masse) : pour prolonger la chaîne avec d'autres LEDs.


## 9. Le buzzer (BUZZER1)

- **BUZZER1 MLT-8530** : transducteur magnétique (sans oscillateur) : il joue la fréquence qu'on lui envoie (son de la
  cigale, musiques, sonneries, sons échantillonnés en PWM).
- **Q1** (2N7002, MOSFET N) : l'interrupteur côté masse, commandé par le PWM du **GPIO28** (PWM6A) à travers **R8**
  (1 kΩ) ; **R9** (100 kΩ) bloque Q1 au repos.
- **D1** (1N4148) : diode de roue libre aux bornes du buzzer (absorbe la surtension de la bobine à chaque coupure).


## 10. Les ports d'extension (J2, J3)

Deux connecteurs femelles 2 × 6 au pas de 2,54 mm.

| J3 (gauche) | | J2 (droite) | |
|---|---|---|---|
| 1 : +3,3 V | 2 : +3,3 V | 1 : +3,3 V | 2 : GPIO20 (réception IR) |
| 3 : I2C1 SCL (GPIO3) | 4 : I2C1 SDA (GPIO2) | 3 : I2C0 SCL (GPIO17, émission IR) | 4 : non connectée |
| 5 : SPI0 SCK | 6 : SPI0 MISO | 5 : I2C0 SDA (GPIO16) | 6 : non connectée |
| 7 : écran RST | 8 : écran BUSY | 7 : UART0 RX (GPIO13) | 8 : SPI0 CS2 (Y2 du décodeur) |
| 9 : SPI0 MOSI | 10 : écran DC | 9 : UART0 TX (GPIO12) | 10 : SPI0 CS3 (Y3 du décodeur) |
| 11 : masse | 12 : masse | 11 : masse | 12 : masse |

- **J3** reprend les signaux de l'écran et l'I2C1 : un écran OLED I2C (Badge > Écran OLED), un autre écran SPI.
- **J2** : la carte infrarouge (GPIO20, GPIO17), l'UART0 (libre : la console est sur l'USB ; prévu pour un module
  ESP32, voir [etude_module_esp32.md](../etude_module_esp32.md)), deux chip-selects SPI.


## 11. Pistes pour une prochaine carte (V1.3)

- Mesure de la batterie : amplificateur rail-à-rail ou pont direct sur l'ADC (§ 1).
- Antenne : plan de masse et dégagement autour de L5, ou une antenne à ressort / un connecteur u.FL (§ 6).
- Régulateur : essayer le mode économie du TPS63000 (PS/SYNC à la masse) pour l'autonomie (§ 1).
- Un GPIO pour couper l'alimentation d'un module d'extension (ESP32), un connecteur SAO.
