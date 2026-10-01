# Radio pirate : une cigale émet en FM sur 433 MHz

**Admin > Radio pirate** : la cigale diffuse du son en **FM bande étroite (NFM)** sur 433 MHz, avec la radio CC1101
du badge. On l'écoute avec un **Portapack** (HackRF), un **SDR** (RTL-SDR + SDR#, GQRX, SDR++...) ou **une autre
cigale** (« Écouter la radio pirate »). Trois sources : une mélodie générée par le badge (« Au clair de la lune »,
domaine public, sans carte SD), une **tonalité de 1 kHz** (pour les mesures) ou un **fichier WAV** de la carte SD
(dossier `MUSIQUE`).

*English version: [pirate_radio.md](../en/pirate_radio.md).*

> **Note légale.** La bande 433,05 - 434,79 MHz est une bande ISM / SRD (appareils de faible portée) : en France et
> en Europe, l'émission y est libre sous conditions (puissance apparente ≤ 10 mW, rapport cyclique limité selon
> l'usage). Ce n'est **pas** une bande de radiodiffusion : réservez la Radio pirate à des **essais courts**, à
> **faible puissance** (-10 dBm par défaut), dans la salle, sans gêner les autres utilisateurs de la bande
> (télécommandes, capteurs...). C'est pourquoi elle n'est accessible que **depuis le mode admin**, et l'émission
> s'arrête d'elle-même au bout de **10 minutes**.

## Comment ça marche

Le CC1101 ne sait pas faire de FM analogique : il fait de la modulation numérique (2-FSK, GFSK, OOK...). L'astuce :

1. Le CC1101 est en **2-FSK, mode série asynchrone** (`PKTCTRL0 = 0x32`) : il émet *f0 + excursion* quand sa broche
   GDO0 est à 1, *f0 - excursion* quand elle est à 0, et il lit GDO0 **8 fois par bit** de son débit programmé
   (datasheet, « asynchronous serial operation »). Le débit est réglé au maximum de la 2-FSK, **500 kbauds** : GDO0
   est donc échantillonnée à **4 MHz**.
2. Le RP2040 pilote GDO0 avec une **PWM rapide** de ~**122 kHz** (125 MHz / 1024, 10 bits) dont le **rapport
   cyclique suit les échantillons du son** : 50 % = silence, 0 % et 100 % = les crêtes. Un canal DMA, cadencé par un
   timer DMA à la fréquence d'échantillonnage, copie les échantillons dans le registre de la PWM, exactement comme
   pour le haut-parleur (`audio.c`, sortie `AUDIO_OUT_RADIO`) : aucun calcul du processeur pendant l'émission.
3. Un récepteur NFM a un filtre de canal étroit (~12,5 kHz) : il ne voit pas les basculements à 122 kHz mais leur
   **moyenne**, *f0 + excursion × (2 × rapport cyclique - 1)* : la fréquence instantanée suit le son, c'est de la
   **FM**. (La synthèse de fréquence du CC1101 lisse aussi un peu les sauts.)
4. Les produits de la PWM tombent à ±122 kHz de la porteuse (et leurs multiples), à environ -30 dBc : hors du canal
   d'écoute, et à l'intérieur de la bande 433 MHz pour les canaux proposés.

Le réseau des cigales est mis en pause pendant l'émission (`net_pause()`), la radio est empruntée à `radio_tools.c`
(`radio_tools_claim()` / `radio_tools_release()`) puis rendue au réseau, qui la reconfigure (profil GFSK, mot de
synchronisation, réception). L'émission s'arrête à la fin du morceau, avec l'aile gauche ou droite, quand on quitte
la page, et au bout de 10 minutes dans tous les cas.

## Les réglages

Flancs : ligne précédente / suivante ; ailes : valeur précédente / suivante ; sur la dernière ligne, aile droite :
émettre (ou choisir le fichier), aile gauche : retour. Appui long sur l'aile gauche : quitter.

| Réglage | Valeurs | Défaut |
|---|---|---|
| Source | Mélodie, Tonalité 1 kHz, Fichier SD | Mélodie |
| Fréquence | 433,300 / 433,650 / **433,920** / 434,200 / 434,500 MHz | 433,920 MHz |
| Puissance | -20, **-10**, 0, +5, +10 dBm | -10 dBm |
| Excursion | 2,5 kHz, **5 kHz** (crête, son à pleine échelle) | 5 kHz |
| Haut-parleur | oui / non : le badge joue aussi le son | non |

- **Mélodie** : « Au clair de la lune » (~23 s), sinus avec une petite enveloppe, 16 kHz.
- **Tonalité 1 kHz** : sinus continu à 78 % de l'excursion (jusqu'à l'arrêt ou aux 10 minutes) : pour les mesures.
- **Fichier SD** : les `.WAV` du dossier `MUSIQUE` (ou de la racine), lus par le lecteur WAV du badge (8, 16, 24 ou
  32 bits, float, mono ou stéréo, jusqu'à 192 kHz). Conseillé : mono 16 kHz, normalisé (`audio2wav.py`). Le son est
  émis tel quel (pas de pré-accentuation, pas de filtre) : un son très aigu élargit un peu le spectre.

Paramètres radio : 2-FSK asynchrone, débit 500 kbauds (`MDMCFG4/3` = `0x4E` / `0x3B` à 26 MHz), `DEVIATN` calculé
pour le quartz mesuré (`0x15` = 5,16 kHz ou `0x05` = 2,58 kHz à 26 MHz), `FREND0 = 0x10` (PATABLE[0]), PATABLE
`0x0E` / `0x34` / `0x60` / `0x84` / `0xC0`, la correction de fréquence du réglage de la radio (`FSCTRL0`, voir
Réglages > Réglage radio) est conservée.

Pendant l'émission, la page affiche **ÉMISSION** en grand, la fréquence, la puissance, la source et le temps écoulé.

## Écouter

### Avec un Portapack (HackRF + Mayhem)

Application **Audio** (Receive > Audio) : modulation **NFM**, fréquence **433,920 MHz** (ou celle choisie), filtre
**16k** pour une excursion de 5 kHz (ou **11k** pour 2,5 kHz), gain LNA/VGA moyens, squelch à 0 pour commencer.
Approchez-vous : à -10 dBm la portée est de quelques mètres.

### Avec un SDR (RTL-SDR, Airspy, HackRF...)

SDR#, GQRX, SDR++ : démodulation **NFM**, largeur ~12,5 kHz (5 kHz d'excursion : 16 kHz), centrée sur la fréquence
choisie. Sur le waterfall, on voit la porteuse et, de part et d'autre, les raies de la PWM à ±122 kHz.

### Avec un Flipper Zero

L'application Sub-GHz du Flipper ne démodule pas la FM audio : elle permet seulement de **voir la porteuse**
(Sub-GHz > **Frequency Analyzer** : la fréquence et le RSSI s'affichent quand la cigale émet). « Read RAW » en preset
FM ne donne pas un son exploitable.

### Avec une autre cigale : « Écouter la radio pirate »

Le deuxième badge est un vrai récepteur pour ce signal :

1. Son CC1101 est en **2-FSK asynchrone en réception** sur la même fréquence, avec un **canal large de 406 kHz** (la
   PWM de l'émetteur, à 122 kHz, y passe) et le même débit de 500 kbauds. Son démodulateur sort sur **GDO2** un 1
   quand la fréquence reçue est au-dessus du centre, un 0 en dessous (`IOCFG2 = 0x0D`) : ce train de bits suit la PWM
   de l'émetteur, son **rapport cyclique est le son** (avec une pente d'environ la moitié).
2. GDO2 est la broche « B » d'une tranche PWM du RP2040 : en mode `PWM_DIV_B_HIGH`, son compteur compte les cycles
   de l'horloge (125 MHz) **pendant que GDO2 est à 1**. Un canal DMA cadencé à **16 kHz** recopie ce compteur dans
   un anneau : la différence entre deux copies est le temps haut sur 62,5 µs, exact au cycle près, sans processeur.
3. La boucle principale en tire le son (rapport cyclique moins sa moyenne), le joue sur le haut-parleur et le
   mesure chaque seconde : fréquence par comptage des passages par zéro, pureté de 1 kHz par l'algorithme de
   Goertzel, niveau (amplitude en % de la demi-plage du rapport cyclique), RSSI.
4. **Correction de fréquence (AFC)** : les quartz des deux badges diffèrent (jusqu'à ±20 ppm chacun, soit ±17 kHz).
   Le rapport cyclique moyen doit valoir 50 % : sinon le récepteur décale sa fréquence par pas de ~400 Hz (toutes
   les 250 ms, par pas de 2 kHz quand le signal est saturé), uniquement quand une porteuse est reçue
   (RSSI ≥ -92 dBm). S'il atteint ±32 kHz sans trouver, il essaie la polarité inverse du démodulateur.

Écran : fréquence, signal (dBm et jauge), correction de fréquence, tonalité détectée (seulement si c'est une vraie
tonalité : pureté ≥ 50 % et niveau ≥ 5 %), niveau et rapport cyclique moyen, état du silencieux. Flancs : canal
précédent / suivant ; aile droite : silencieux (squelch) oui / non ; aile gauche : retour.

Sur le port série USB, une ligne par seconde (valeurs d'exemple) :

```
pirate: rx tone 1000 Hz, level 38 %, purity 1 kHz 97 %, duty 50 %, rssi -62 dBm, afc +1191 Hz
```

## Essai avec deux badges

1. Badge A (récepteur) : ouvrir « Écouter la radio pirate », sur 433,920 MHz. Brancher son USB, ouvrir le port série.
2. Badge B (émetteur, mode admin) : Admin > Radio pirate, Source « Tonalité 1 kHz », Puissance **0 dBm** (ou +10 dBm
   si les badges sont loin : à -10 dBm on mesure ~-97 dBm à 1 m, en limite de sensibilité dans un canal de 406 kHz),
   « > Émettre ». Les deux badges à 1 m l'un de l'autre.
3. Sur A, en quelques secondes : RSSI nettement au-dessus de -92 dBm, la correction se stabilise, et le port série
   affiche `rx tone 1000 Hz` (±5 Hz) avec une pureté élevée ; le haut-parleur siffle à 1 kHz.
4. Puis la Mélodie (on la reconnaît sur A), puis un WAV de la carte SD. Arrêt : aile gauche sur B ; la ligne
   `no carrier` apparaît sur A.

## Messages sur le port série

- `pirate: start on 433920000 Hz, deviation 5157 Hz (DEVIATN for 5000 Hz), power -10 dBm (PATABLE 0x34), source ...`
- `pirate: stop (fin du morceau | arrêt manuel | page quittée | sécurité : 10 min max) after N s`
- `pirate: error, ...`, `pirate: radio busy ...` (récepteur OOK actif, par exemple le badge de talk : réessayer
  ailleurs que sur ce badge).
- Récepteur : `pirate: rx start ...`, `pirate: rx tone ...`, `pirate: rx afc at its limit, polarity -1`,
  `pirate: rx stop`.

## Limites

- Le récepteur « cigale » a un canal large (406 kHz) : il est moins sensible qu'un vrai récepteur NFM, et plus
  sensible aux autres émetteurs de la bande. Le son reçu est correct mais un peu compressé.
- Pas de pré-accentuation ni de filtrage du son à l'émission.
- La fréquence réelle dépend du quartz du badge (voir Réglages > Réglage radio) : quelques kHz d'écart sont normaux,
  le Portapack les tolère en NFM 16k.
