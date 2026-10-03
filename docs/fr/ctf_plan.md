# Plan du CTF du badge SecSea (brouillon)

*Document de conception, à relire et valider avec Tristus1er avant de coder. Il ne contient **aucune réponse ni flag
en clair** : les solutions vivent dans les fichiers générateurs (`tools/crypto_ctf_make.py` et à venir), obfusquées
dans le firmware. Prolonge [idees_reseau_extensions_ctf.md § 3](../idees_reseau_extensions_ctf.md).*

Rédigé le 2026-10-03.

## 1. Objectifs et principes

- **Public** : participant·es de tous niveaux ; certain·es ont un Flipper, un SDR, picotool, un debugger.
- **Progressif** : des épreuves débutant → avancé, chacune exploitant une **capacité réelle du badge**, pour donner
  envie d'explorer le matériel sans frustrer.
- **Format des flags** : `SECSEA{...}`.
- **Autonome** : tout doit pouvoir se valider **sur le badge lui-même**, sans réseau ni serveur (une station de base
  reste une option pour plus tard, § 3).
- **Sans secret fort** : le firmware ne stocke que des **SipHash** de réponses/flags (jamais le texte en clair), et
  les flags sont **obfusqués** (pas lisibles avec `strings`). C'est une protection anti-triche, pas de la crypto
  forte — et c'est assumé (certaines épreuves avancées consistent justement à casser ça).
- **Légal** : radio dans la bande ISM 433 MHz, puissance et rapport cyclique raisonnables, **jamais de brouillage**.
- **Reproductible** : les réponses sont dans des **fichiers de solutions** (générateurs) qu'on **ne projette jamais**
  et qu'on ne lance pas devant les participants.

## 2. Ce qui existe déjà (socle)

| Épreuve | Où | État |
|---|---|---|
| 13 défis **crypto** (César → hash tronqué), flag final `SECSEA{...}` en morceaux | `Jeux solo › Défis crypto`, `crypto_ctf.c` + `tools/crypto_ctf_make.py` | ✅ en place |
| Flag **code Konami** (boutons) | page « Saisir un code », `ctf.c` | ✅ en place |
| **Progression** (bits des défis résolus, flags trouvés) | `store_t.crypto_solved`, `store_t.flags_found` (32 bits libres) | ✅ en place |
| Compteur « CTF : n/m flags », succès à la clé | menu CTF, `achievements.c` | ✅ en place |

## 3. Validation : galerie de trophées, sans saisie de texte (décision prise)

On **ne tape jamais de flag au clavier** (l'éditeur à 4 boutons est pénible). À la place : une **galerie de trophées
cigales**, chaque épreuve se validant **automatiquement**. Épreuve réussie → une **🦗 cigale** s'allume ; pas encore
→ case vide/verrouillée. C'est exactement le modèle du thème **« Succès »** existant (`achievements.c` :
`achv_unlock()` / `achv_unlocked()`, « Obtenu ! » / « Pas encore obtenu », niveaux et XP ; deux succès CTF existent
déjà, « Hacker » et « Cryptographe »).

**On réutilise ce système** : soit en ajoutant des succès, soit avec une page CTF dédiée qui affiche la même grille
de cigales (verrouillées = silhouette). Peu de code d'interface neuf.

**Conséquence de l'auto-validation** : le firmware doit pouvoir **détecter** la réussite. Deux cas :

1. **Épreuve-action** : le badge observe directement l'action → trophée. Ex. : séquence de boutons (comme le Konami),
   menu/état caché atteint, commande série cachée reçue, **paquet radio** précis reçu (rejeu Flipper, balise staff),
   **bon fichier** ouvert sur la SD.
2. **Épreuve-décodage** (Morse, crypto, spectrogramme…) : le décodage aboutit à une **courte séquence de boutons**
   (pas du texte) à saisir sur la page « Saisir un code » (celle du Konami) ; le firmware la compare au **SipHash**
   de la bonne séquence → trophée. Les flags `SECSEA{...}` restent affichés comme **récompense** une fois l'épreuve
   validée (pour un éventuel classement), mais ne sont jamais ressaisis.

**À décider** : les épreuves de **reverse pur** (dump firmware, SWD) n'ont pas d'action observable par le badge ;
elles finiront soit par une séquence de code, soit par une validation à une **station de base** (un PC qui écoute le
réseau) — à trancher plus tard. Le modèle trophée couvre parfaitement débutant + intermédiaire.

> **Note anti-triche** : des séquences de boutons courtes sont peu entropiques et faciles à partager. Pour un CTF
> d'atelier convivial, c'est acceptable (l'objectif est d'apprendre, pas de verrouiller).

## 4. Le parcours proposé

Chaque ligne : la capacité exploitée, ce que fait le·la joueur·se, le matériel nécessaire, où vivrait la réponse, et
si c'est à construire. Les épreuves notées ✅ existent déjà.

### Débutant (dans le badge, sans outil)

| # | Épreuve | Capacité | Le joueur… | Matériel | À faire |
|---|---|---|---|---|---|
| D1 | Défis crypto | éditeur, SipHash | déchiffre, tape la réponse | — | ✅ (on peut en **ajouter**) |
| D2 | Code Konami | boutons | trouve la séquence | — | ✅ |
| D3 | **Cigale bavarde** : un flag joué en **Morse** sur le buzzer **et** les LEDs | `music`/`noise_gen` + `leds` | décode le Morse | oreille / œil | à construire (le Morse crypto existe déjà comme brique) |
| D4 | **Accueil série** : un message sur l'USB ; une **commande cachée** (non listée par `help`) donne un flag | protocole USB (`main.c`) | ouvre le port série, trouve la commande | PC + `badge_remote.py` | à construire (ajouter une touche cachée) |
| D5 | **Fichier planqué** sur la carte SD (ou flag dans **une** image d'une vidéo à 10 i/s) | `sd`, `video` | fouille la carte / fait pause sur la bonne image | carte SD | à construire (contenu SD) |

### Intermédiaire (un outil)

| # | Épreuve | Capacité | Le joueur… | Matériel | À faire |
|---|---|---|---|---|---|
| I1 | **Ultrason** : un Morse joué à 19 kHz, quasi inaudible | `audio_pwm_tone` | voit le signal au **spectrogramme** | téléphone/PC + appli spectro | ✅ (brique `ultrasound` déjà là, à relier à un flag) |
| I2 | **Rémanence de l'écran** : le flag affiché une fraction de seconde puis effacé ; le *ghosting* le laisse deviner | mode multiframe de l'e-Paper | observe l'écran | écran e-Paper | à construire (nécessite l'écran) |
| I3 | **Plan mémoire caché** : en N&B le flag est écrit dans la RAM « RED », visible seulement en **4 gris** | `screen` | bascule le mode (menu caché) | écran e-Paper | à construire |
| I4 | **Écoute radio** : le badge émet périodiquement un message sur une autre fréquence/modulation | `radio` / `net` | le trouve au **Frequency Analyzer** du Flipper puis le décode | Flipper ou SDR | à construire |
| I5 | **Rejeu radio** : une « porte » (station) s'ouvre sur le bon paquet ; on capture celui d'un badge staff et on le rejoue | `remote` (Princeton) | capture puis rejoue avec le Flipper | Flipper | à construire (+ station) |

### Avancé (reverse / matériel)

| # | Épreuve | Capacité | Le joueur… | Matériel | À faire |
|---|---|---|---|---|---|
| A1 | **Bug du quartz** : un badge « mal calibré » émet le flag décalé (~27 vs 26 MHz) | `radio_tune` | comprend le décalage et réaccorde | Flipper/SDR | à construire (clin d'œil au bug trouvé en juin) |
| A2 | **Dump du firmware** : `picotool save` puis reverse (Ghidra, ARM Thumb) d'une clé/flag chiffré | flash non protégée | dumpe et rétro-ingénie | picotool | à construire (placer un flag obfusqué) |
| A3 | **SWD** : attacher un debugger aux pastilles, lire un secret qui n'existe qu'en **RAM** | SWD | openocd + une Pico en picoprobe | debugger | à construire |
| A4 | **Flag social** : un flag en morceaux, chaque morceau donné par un **type** de badge (staff, orateur…) ; il faut les rencontrer | `social` + `badge_type` | rencontre les bons badges | plusieurs badges | à construire |

> Remarque faisabilité **sans l'écran** : D1-D4, I1, I4-I5, A1-A4 se testent via `badge_remote.py` (écran à l'image)
> et les LEDs/buzzer/radio de la carte. I2-I3 ont vraiment besoin de l'e-Paper (à ta réception).

## 5. Pour le workshop

- **Débutants dans le firmware d'origine** (D1-D5, I1) : rien à outiller, parfaits pour animer.
- **Intermédiaires/avancés** : chacun est une **mini-démo d'atelier** (Flipper, SDR, spectrogramme, picotool, SWD).
- Prévoir, par épreuve : un **indice** (le système d'indices des défis crypto existe déjà), le matériel, et la
  solution pour les animateurs (fichier de solutions, non projeté).

## 6. Ordre de réalisation proposé

1. **Ajouter des défis crypto** (thème Provence/hacking) — rodage du flux, aucun risque.
2. **Galerie de trophées CTF** (§ 3, sur le modèle des Succès) + 1ʳᵉ épreuve « capacité » simple (**D3 Cigale
   bavarde** ou **D4 commande série**) pour valider la mécanique auto de bout en bout.
3. **1 épreuve radio** (I4 écoute) — spectaculaire, et tu as le Flipper.
4. Le reste selon le temps et l'arrivée de l'écran (I2-I3) et le nombre de badges (A4).

## 7. À décider avec Tristus1er

- Validation locale « Saisir un flag » (§ 3.1) : OK comme mécanique commune ? intégrée au menu CTF existant ?
- Nombre d'épreuves visé pour l'événement, et lesquelles.
- Format exact des flags (`SECSEA{minuscule_avec_underscores}` comme le flag crypto actuel ?).
- Où range-t-on les fichiers de solutions (déjà `crypto_ctf_make.py` ; en ajouter un pour les flags « capacité »).
- Les épreuves radio/émission : fréquences, puissance, rapport cyclique — rester dans l'ISM, pas de brouillage.
- Coordination : branche dédiée + PR vers `dev-LLM`, par petits lots (comme les LEDs).
