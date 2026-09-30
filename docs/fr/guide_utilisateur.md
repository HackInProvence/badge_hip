# Badge SecSea — guide utilisateur

Ce guide explique comment utiliser le badge SecSea de Hack In Provence : allumer, naviguer dans les menus,
profiter des médias, des jeux, des fonctions sociales, de la radio et des extensions, et préparer une carte SD.
Pour modifier le logiciel du badge, voir le [guide développeur](guide_developpeur.md).

*English version: [user guide](../en/user_guide.md).*


## 1. Le badge en bref

- Un écran e-Paper (encre électronique) de 200 × 200 pixels, en noir et blanc ou 4 niveaux de gris.
  L'image reste affichée même sans courant.
- 4 boutons, 2 LEDs de couleur, un buzzer.
- Une radio 433 MHz (CC1101) compatible avec le Flipper Zero. Les badges s'en servent pour se parler :
  messages, votes, jeux à deux, échange de cartes de visite, choeur...
- Un lecteur de carte micro-SD pour les vidéos, musiques, textes et images.
- Deux ports d'extension (infrarouge, écran OLED...).
- Une batterie rechargeable par USB-C.


## 2. Démarrer

1. Mettez l'interrupteur sur **ON**. Sur **OFF**, le badge ne démarre pas, même branché en USB
   (seule la charge fonctionne : LED rouge pendant la charge, verte quand la batterie est pleine).
2. Le menu principal s'affiche après quelques secondes.

L'écran e-Paper est lent (environ 0,3 s pour changer l'image) et clignote parfois en noir et blanc :
c'est normal, il se « nettoie » régulièrement pour éviter les traces des images précédentes.


## 3. Les boutons

Le badge est vu de face, la tête de la cigale en haut.

| Bouton | Dans les menus | Rôle habituel |
|---|---|---|
| **Aile droite** (D) | valider | OK, pause, lancer |
| **Aile gauche** (G) | retour | annuler, quitter |
| **Flanc gauche** | monter | précédent, moins |
| **Flanc droit** | descendre | suivant, plus |

Le bas de chaque écran rappelle les boutons utiles, par exemple « Flancs : choix  D : OK  G : retour ».
Dans le menu principal, l'aile gauche coupe aussi le son de la cigale et les LEDs.

Certaines pages utilisent aussi l'**appui long** (environ 0,8 s) : dans les casse-têtes, l'appui long sur
l'aile gauche quitte le jeu ; dans les saisies de texte, l'appui long sur l'aile droite valide.

**Saisir du texte avec 4 boutons** (cartes de visite, réponses aux défis, remède du virus) :
- flancs : caractère précédent / suivant (en maintenant, les caractères défilent) ;
- aile droite (appui court) : position suivante ; aile gauche : position précédente ;
- aile gauche sur la première position : annuler ;
- aile droite (appui long) : valider.


## 4. Les menus

Le menu principal regroupe les fonctions par thème :

| Thème | Contenu |
|---|---|
| **Médias** | Images, Vidéos, Musique, Lecture rapide, Volume |
| **Jeux** | Morpion, Puissance 4, Simon, Réflexes, Snake, Démineur, 2048, Taquin, Sokoban, Mastermind, Pendu, Blind test, CTF, Défis crypto, Duel, Bataille navale |
| **Social** | Réseau des cigales, Messages, Contacts, Programme, Vote, Radar des cigales, Chaud - froid, Virus des cigales, Choeur |
| **Radio & IR** | Message radio, porteuse radio, Décodeur 433 MHz, Station météo, Envoyer une image, Recevoir une image, Infrarouge |
| **Badge** | Badge nominatif, Lampe, Badge de talk, son de la cigale, animations des LEDs, démo de l'écran, écran OLED |
| **Réglages** | Veille de l'écran, Télécommande, Mode muet, Infos, Crédits |

Un septième thème, **Admin**, est caché : il est réservé aux organisateurs (voir [§ 4.8](#48-le-mode-admin-organisateurs)).

**Notifications** : quand un message, une question de vote, une invitation à jouer ou l'annonce d'un talk arrive,
le badge émet un bip et l'écrit en bas de l'écran. Depuis les menus ou la veille, la page concernée s'ouvre
directement (vote, programme, virus, invitation au duel ou à la bataille navale).


### 4.1 Médias

Les médias sont lus sur la carte SD (voir [§ 7](#7-préparer-la-carte-sd)).
Chaque lecteur affiche la liste des fichiers ; les sous-dossiers sont précédés de « > ».

**Images** (dossier `IMAGES`)
- Flancs : image précédente / suivante.
- Aile droite : choisir cette image comme image de veille.
- Aile gauche : retour à la liste.

**Vidéos** (dossier `VIDEOS`)
- Aile droite : pause / reprise ; flancs : volume ; aile gauche : arrêt.
- Le son sort par le buzzer : il est reconnaissable, pas hi-fi.

**Musique** (dossier `MUSIQUE`)
- Aile droite : pause ; aile gauche : stop ; flanc gauche : moins fort ; flanc droit : plus fort.

**Lecture rapide** (dossier `TEXTES`)
- Les mots s'affichent un par un au même endroit, les yeux ne bougent plus : on lit beaucoup plus vite
  (méthode PVSR, voir [pvsr.md](../pvsr.md)).
- Aile droite : pause / reprise ; aile gauche : quitter.
- Flancs (appui court) : vitesse −/+ (de 100 à 900 mots par minute, 250 au départ).
- Flancs (appui long) : recul / avance d'environ 10 secondes de lecture.
- La position est retenue : un texte reprend là où vous l'avez quitté.

**Volume** : chaque appui sur l'aile droite augmente le volume (0 à 8, puis retour à 0) et joue un petit carillon.


### 4.2 Jeux

Dans les jeux du tableau ci-dessous (sauf Simon), **l'aile gauche quitte le jeu**.
Dans les casse-têtes, c'est un **appui long sur l'aile gauche**.

| Jeu | Comment jouer |
|---|---|
| **Morpion** | Flancs : choisir la case ; aile droite : jouer. Vous avez les croix, la cigale les ronds. Chacun commence à son tour. La cigale se trompe de temps en temps... |
| **Puissance 4** | Flancs : choisir la colonne ; aile droite : lâcher le pion. Alignez 4 pions pleins avant les anneaux de la cigale. |
| **Simon** | Chaque bouton est une zone de l'écran, à sa place sur le badge (flancs en haut, ailes en bas), avec sa couleur et sa note. Répétez la séquence, qui s'allonge à chaque tour. Ici l'aile gauche est une zone du jeu : pour quitter, attendez la fin de la partie puis aile gauche. |
| **Réflexes** | Quand les LEDs s'allument en vert (avec un bip), appuyez vite sur l'aile droite ou un flanc. 5 essais, la moyenne compte. Trop tôt : l'essai recommence. |
| **Snake** | Aile droite : départ / pause. Flanc gauche : tourner à gauche ; flanc droit : tourner à droite (par rapport à la direction du serpent). |
| **Blind test** | Choisissez un dossier de musiques : les morceaux passent dans un ordre aléatoire, sans répétition. Aile droite : afficher le titre ; flanc droit : morceau suivant ; flanc gauche : pause. |
| **CTF** | Un défi : entrer un code célèbre des jeux vidéo avec les 4 boutons pour obtenir un « flag ». |

Les records sont gardés même après extinction : meilleur score de Simon et Snake, meilleure moyenne de Réflexes,
nombre de victoires contre la cigale au Morpion et au Puissance 4.

**Score en QR code** : à la fin d'une partie, un appui sur un flanc affiche un QR code qui contient le jeu, le score,
le nom de votre cigale et une signature : les organisateurs peuvent le scanner pour un classement, et un score
modifié à la main est refusé. N'importe quel bouton referme le QR code.
Dans le menu Jeux, un **appui long sur l'aile droite** sur le nom d'un jeu affiche de la même façon le QR code de son record.

En bas de l'écran, les boutons sont indiqués dans l'ordre où ils sont sur le badge : G (aile gauche) à gauche,
D (aile droite) à droite.

**Casse-têtes** : Démineur, 2048, Taquin, Sokoban, Mastermind et Pendu se jouent au tour par tour, au rythme de l'écran.
Ils partagent les mêmes commandes :
- appuis courts : se déplacer. Flanc gauche : à gauche ; flanc droit : à droite ; aile gauche : en haut ;
  aile droite : en bas (le badge vu de face a ses flancs en haut et ses ailes en bas) ;
- appuis longs : agir. Aile droite : action principale (ouvrir, valider...) ; flanc gauche : action secondaire
  (drapeau, annuler) ; flanc droit : page d'aide ; aile gauche : quitter.

Chaque jeu commence par sa page d'aide (règles, commandes, record).

| Jeu | Comment jouer |
|---|---|
| **Démineur** | Grille de 9 × 9 avec 12 mines ; la première case ouverte n'est jamais une mine. Aile droite longue : ouvrir ; flanc gauche long : drapeau. Le chiffre d'une case donne le nombre de mines autour. |
| **2048** | Toutes les tuiles glissent vers le bouton pressé ; deux tuiles égales qui se touchent fusionnent. Atteignez la tuile 2048 ! |
| **Taquin** | Remettez les cases de 1 à 15 dans l'ordre, le trou à la fin : la case voisine du trou glisse vers le bouton pressé. |
| **Sokoban** | Poussez les caisses sur les cibles, une à la fois. 6 niveaux ; flanc gauche long : annuler le dernier coup ; aile droite longue : recommencer le niveau. |
| **Mastermind** | Trouvez le code secret de 4 symboles parmi 6 (disque, anneau, carré plein, carré vide, triangle, croix) en 10 essais. Flancs : case ; ailes : symbole ; aile droite longue : valider. Un point : un symbole bien placé ; un rond : un symbole présent mais mal placé. |
| **Pendu** | Trouvez le mot, lettre par lettre, avant d'être pendu (8 erreurs). Flancs et ailes : choisir la lettre ; aile droite longue : la proposer. |

Records gardés : meilleur temps au Démineur, meilleur score au 2048, moins de coups au Taquin, nombre de niveaux
réussis au Sokoban, moins d'essais au Mastermind, plus de mots trouvés d'affilée au Pendu.

**Défis crypto** : 13 énigmes de cryptographie classique, de la plus simple à la plus difficile
(acrostiche, César, ROT13, Morse, binaire, hexadécimal, base64, Atbash, scytale, XOR, Vigenère, hash tronqué,
ultrason).
- La liste coche les défis résolus ; aile droite : ouvrir un défi.
- Sur un défi : aile droite (appui court) : répondre ; aile droite (appui long) : un indice ;
  flanc droit : écouter le Morse (buzzer et LEDs), pour les défis qui en contiennent. Le défi « Ultrason »
  joue son Morse en boucle à 19 kHz, sans LEDs : presque inaudible, il se lit sur le spectrogramme d'un
  téléphone (Spectroid, Spek...) ; flanc droit à nouveau : arrêter.
- La réponse se saisit avec les 4 boutons (lettres majuscules, chiffres, espace) ; aile droite longue : valider.
- Chaque défi résolu donne un morceau du flag ; « > Le flag final » l'affiche en entier quand tous sont résolus.
  La progression est gardée même après extinction.

**Duel** (pierre-feuille-ciseaux) et **Bataille navale** se jouent à deux badges, par radio :
- la page liste les cigales à portée ; flancs : choisir, aile droite : défier ;
- l'autre badge reçoit l'invitation (bip, et la page s'ouvre depuis les menus) : aile gauche : refuser ;
  aile droite : accepter.
- **Duel** : à chaque manche, flancs : pierre, feuille ou ciseaux ; aile droite : jouer. Le premier à 3 manches gagne.
  Aucun badge ne peut attendre le choix de l'autre pour tricher : chacun s'engage sur son choix avant de le révéler.
- **Bataille navale** : une mer de 6 × 6 et 3 bateaux (3, 2 et 2 cases) placés au hasard. Celui qui invite tire
  le premier, puis chacun son tour. Flancs : viser (case par case) ; aile droite : tirer.
  Le premier qui coule toute la flotte adverse gagne. À la fin, chaque badge révèle sa flotte et l'autre vérifie
  qu'il a répondu honnêtement : « Flotte OK » ou « TRICHE ! ».
- À la fin d'une partie : aile droite : rejouer ; aile gauche : quitter.


### 4.3 Social

**Réseau des cigales**
- Les badges allumés s'envoient discrètement des signaux radio.
- Approchez votre badge tout près d'un autre pendant quelques secondes : vous gagnez
  **10 points** pour une nouvelle rencontre, **1 point** pour une cigale déjà rencontrée (une fois par heure au plus).
- Aile droite : choisir le **nom de votre cigale** (8 caractères).
  - Flancs : changer la lettre.
  - Aile droite (appui court) : lettre suivante ; aile gauche : lettre précédente.
  - Aile droite (appui long) : enregistrer.
  - Aile gauche sur la première lettre : annuler.
- Flanc droit : activer / couper les signaux.

**Messages** : des messages courts entre cigales, choisis dans une liste de 16 (« Salut ! », « Café ? »,
« Qui fait le CTF ? »...), sans clavier.
- La boîte de réception garde les 10 derniers messages ; « (privé) » marque ceux qui ne sont que pour vous.
- « > Ecrire un message », puis le destinataire : « Tout le monde », une cigale voisine ou l'auteur d'un message reçu.
  Une cigale est désignée par son nom et la fin de son identifiant, par exemple « Tristan#33EC ».
- Aile droite sur un message reçu : lui répondre.
- Les autres cigales relaient le message (jusqu'à 3 fois) pour qu'il arrive à destination.

**Contacts** : une carte de visite, échangée par radio avec un badge proche.
- « Ma carte » : 13 champs (prénom, nom, téléphone, e-mail, société, poste, adresse, ville, LinkedIn, Git,
  site web, Mastodon, commentaire). Aile droite : modifier le champ (saisie avec les 4 boutons) ;
  aile droite (appui long) : cocher ou décocher « [x] » pour l'envoyer ou non. Seuls les champs cochés sont envoyés.
- « Echanger (badges proches) » : les **deux** badges doivent être sur cette page, l'échange n'a lieu qu'avec
  votre accord. La carte reçue s'affiche : aile droite : la garder ; aile gauche : l'ignorer.
- « Contacts reçus » : les 12 dernières cartes gardées (la plus ancienne est oubliée au-delà).
  Aile droite : voir ; aile droite (appui long) : supprimer.
- Sur l'ordinateur, `tools/contacts_export.py` exporte les cartes reçues en fichier vCard (`.vcf`),
  à importer dans un téléphone ou un carnet d'adresses.

**Programme** : le programme de la conférence, sans carte SD. Aile droite : le détail d'un talk, avec le QR code
de son lien ; flancs : talk précédent / suivant. Quand les organisateurs annoncent le prochain talk,
sa page s'ouvre toute seule (« Prochain : ... »).
Le programme de SecSea 2026 n'est pas encore publié : les talks affichés sont provisoires.

**Vote** : quand les organisateurs posent une question (« Ce talk vous a plu ? »...), la page s'ouvre.
Flancs : choisir la réponse ; aile droite : voter. On peut changer d'avis tant que le vote est ouvert :
seul le dernier vote de chaque badge compte.

**Radar des cigales** : les cigales entendues, avec la force de leur signal (en dBm, « * » pour une cigale déjà
rencontrée). Aile droite : suivre une cigale en mode « chaud - froid » ; aile gauche : retour à la liste.

**Chaud - froid** : les organisateurs cachent un badge balise ; trouvez-le à la force de son signal.
Le badge indique « Glacial », « Froid », « Tiède », « Chaud » ou « BRÛLANT ! » avec une jauge,
les LEDs passent du bleu au rouge et les bips s'accélèrent en approchant.
Aile droite : chercher une autre balise.

**Virus des cigales** : un virus (inoffensif) circule de cigale en cigale.
- Restez trop longtemps près d'une cigale infectée et vous risquez de l'attraper ; votre badge « tousse » alors
  à son tour et peut contaminer les cigales proches.
- Le remède : une énigme. Aile droite : se soigner, puis saisir la réponse avec les 4 boutons.
  Une cigale guérie est immunisée.

**Choeur** : quand un chef de choeur (un organisateur ou une télécommande) lance un morceau, les cigales autour le
chantent ensemble, chacune sa voix, avec un départ synchronisé par radio. Deux morceaux : « Frère Jacques » en canon
à 4 voix et « L'Ode à la joie » à 3 voix.
Votre badge participe par défaut ; aile droite : participer / ne plus chanter. En mode muet, le choeur est silencieux.


### 4.4 Radio & IR

**Radio : message** envoie « SecSea <nom de votre cigale> coucou #<numéro> », que l'on peut lire avec l'application
« SubGHz chat » d'un Flipper Zero (433,92 MHz).
Un **appui long sur l'aile droite** lance le **mode test** : un message toutes les 5 secondes.
- Flancs : pause plus courte / plus longue (1 seconde au minimum, sans maximum). En maintenant, les valeurs défilent de plus en plus vite.
- Aile droite : pause / reprise ; aile gauche : quitter.

**Radio : porteuse** émet un signal continu pendant 30 s (visible avec l'analyseur de fréquence d'un Flipper Zero).

**Décodeur 433 MHz** (réception seule) : les dernières trames entendues, avec leur âge : télécommandes Princeton,
CAME, Nice FLO, sondes météo. Une trame reçue plusieurs fois est suivie de « x2 », « x3 »... Aile droite : effacer.
Pendant l'écoute, le badge n'entend plus les autres cigales.

**Station météo** (réception seule) : la dernière mesure de chaque sonde 433 MHz entendue (4 au plus) :
température, humidité, protocole, canal, et « pile ! » quand la pile de la sonde est faible.
Sondes reconnues : Nexus-TH, inFactory, ThermoPRO TX-4, GT-WT02, LaCrosse TX141TH-Bv2, Acurite 592TXR.
Les sondes émettent toutes les 30 à 60 s : un peu de patience. Aile droite : effacer.

**Envoyer une image** : choisissez l'image intégrée « SecSea » ou une image du dossier `IMAGES` ;
elle est convertie en noir et blanc et envoyée deux fois par radio aux badges qui l'attendent (une douzaine de secondes).
Aile gauche : arrêter.

**Recevoir une image** : attendez qu'un autre badge envoie une image. Elle s'affiche au fur et à mesure,
les morceaux manquants en gris ; des blocs de contrôle permettent de reconstituer ceux qui se sont perdus en route.
« Reçue ! » quand elle est complète. Aile droite : attendre une autre image.

**Infrarouge** (module sur le port droit)
- « Enregistrer un signal » : visez le récepteur avec une télécommande et appuyez sur une touche.
- Les 4 emplacements rejouent les signaux enregistrés, comme une télécommande.


### 4.5 Badge

- **Badge nominatif** : « SecSea 2026 », le nom de votre cigale en grand et votre type (PARTICIPANT, ORATEUR ou STAFF)
  dans un bandeau noir. La veille ne le remplace pas : il reste affiché, même badge éteint.
  Le type est choisi par les organisateurs. Aile gauche : retour.
- **Lampe** : les 2 LEDs en blanc. Flancs : moins / plus fort, par pas de 10 % (en maintenant : plus vite) ;
  aile droite : éteindre / allumer. La luminosité est retenue et affichée dans le menu (« Lampe : 50 % »).
  En mode muet, les LEDs restent éteintes.
- **Badge de talk** : pour les orateurs, les LEDs indiquent le temps de parole :

  | État | LEDs |
  |---|---|
  | Eteint | éteintes |
  | OK | vert fixe : tout va bien |
  | 5 min | orange qui respire doucement : il reste 5 minutes |
  | FINI | rouge qui clignote : le temps est écoulé |
  | STOP ! | rouge qui clignote vite, avec un bip toutes les 1,5 s : on conclut ! |

  L'état est changé par la télécommande des organisateurs (voir [§ 4.7](#47-télécommande-et-mode-muet)) ou à la main :
  flanc gauche : état précédent ; flanc droit ou aile droite : état suivant. Cette page fonctionne même en mode muet,
  et écoute la télécommande en permanence tant qu'elle est ouverte.
- **Cigale** : active ou coupe le chant de la cigale.
- **LEDs** : change l'animation (arc-en-ciel, respiration, battement, clignotement, vert fixe, éteintes).
- **Démo écran** : montre les possibilités de l'écran (noir et blanc, 4 gris, animation rapide).
- **Écran OLED** : démos sur un petit écran OLED branché sur le port gauche (étoiles, cube 3D, cigale, texte, vidéo).


### 4.6 Réglages

- **Veille de l'écran** :
  - le délai avant la veille (1, 2, 5, 10 ou 30 minutes, ou désactivée) ;
  - l'image affichée pendant la veille (la SecSea par défaut, ou une image du dossier `IMAGES`) ;
  - « Aperçu » pour l'essayer.

  L'image de veille est affichée en noir et blanc tramé (les gris deviennent des motifs de points) :
  c'est l'affichage le plus stable de l'écran, l'image reste nette pendant des heures sans courant.

  Pendant la veille, n'importe quel bouton réveille le badge.
- **Télécommande : oui / non** : le badge obéit (ou non) aux commandes radio des organisateurs et du Flipper Zero
  (voir [§ 4.7](#47-télécommande-et-mode-muet)). Activée par défaut.
- **Mode muet : oui / non** : coupe le son et les LEDs. Les organisateurs peuvent l'activer à distance pendant les talks.
- **Infos** : version de la radio, quartz, carte SD, batterie. Aile droite : les crédits.
- **Crédits** : les personnes et associations derrière le badge.


### 4.7 Télécommande et mode muet

Les organisateurs peuvent envoyer des commandes à tous les badges de la salle, depuis un badge en mode admin
ou depuis un Flipper Zero. Le badge affiche la commande reçue en bas de l'écran.

| Commande | Effet |
|---|---|
| 0x01 | la cigale chante quelques secondes |
| 0x02 | **mode muet** : plus de son ni de LEDs (pendant les talks) |
| 0x03 | fin du mode muet |
| 0x10 à 0x14 | lumières du badge de talk (si sa page est ouverte) : éteint, OK, 5 min, FINI, STOP ! |
| 0x20 + n | affiche le talk n du programme (« Prochain : ... ») |
| 0x30 + n | lance le morceau n du choeur |

Le réglage du mode muet est gardé après extinction ; il se désactive par la commande 0x03 ou dans Réglages > Mode muet.
Pour ne plus obéir aux commandes : Réglages > Télécommande : non.

**Depuis un Flipper Zero** : une commande est un code **Princeton** 24 bits `0xC16Axx`, où `xx` est la commande
(par exemple `0xC16A02` pour le mode muet).

Le plus simple : copier les deux fichiers de `tools/flipper/` dans le dossier `subghz` de la carte SD du Flipper,
puis Sub-GHz > Saved > le fichier. La télécommande du Flipper envoie avec chaque bouton le même code avec un autre
bouton Princeton (les 4 derniers bits), et chaque bouton a sa commande :

| Bouton du Flipper | `SecSea_general.sub` | `SecSea_talk.sub` |
|---|---|---|
| OK | la cigale chante | talk : vert |
| Haut | mode muet | talk : orange (5 min) |
| Bas | fin du mode muet | talk : rouge énervé |
| Droite | — | talk : rouge (fini) |
| Gauche | — | talk : éteint |

Autres commandes :
- Sub-GHz > Add Manually > Princeton_433, enregistrer, puis modifier la ligne `Key:` du fichier enregistré ;
- ou générer un fichier `.sub` sur l'ordinateur et le copier dans le dossier `subghz` de la carte SD du Flipper :
  `python tools/ook_sub.py princeton 0xC16A02 -o muet.sub`, puis Sub-GHz > Saved > muet > Send.

Le badge écoute les télécommandes un court instant (220 ms) toutes les 800 ms environ : maintenez l'envoi
une seconde. La commande `subghz tx` de la ligne de commande du Flipper ne transmet pas le code tel quel :
utilisez un fichier `.sub` (elle remplace les 4 derniers bits du code).


### 4.8 Le mode admin (organisateurs)

Dans le menu principal (la liste des thèmes), tapez sur les flancs **gauche, gauche, droit, droit, gauche, droit,
gauche, droit** en moins de 8 secondes : « Mode admin activé » s'affiche et le thème **Admin** apparaît, déjà sélectionné,
après les autres. Le mode admin reste actif après extinction.

| Entrée | Rôle |
|---|---|
| **Commandes radio** | envoie une commande à tous les badges autour : mode muet, fin du mode muet, cigale, lumières du badge de talk |
| **Annoncer un talk** | affiche un talk du programme sur tous les badges |
| **Vote (admin)** | ouvre une question, compte les votes (un par badge) et affiche l'histogramme ; aile droite : fermer le vote |
| **Choeur : lancer** | lance un morceau du choeur ; ce badge chante la première voix |
| **Balise chaud-froid** | ce badge émet une balise par seconde : cachez-le, les autres le cherchent avec Social > Chaud - froid |
| **Virus : patient zéro** | infecte ce badge pour lancer l'épidémie ; flanc gauche : le guérir |
| **Type du badge** | Participant, Orateur ou Staff, affiché par le badge nominatif |
| **Quitter le mode admin** | cache à nouveau le thème Admin |


## 5. Batterie

- La batterie se recharge par le port USB-C, même interrupteur sur OFF.
  LED rouge : en charge ; LED verte : chargée.
- Le niveau de batterie n'est affiché (icône en haut à droite du menu, et dans Infos)
  que si le badge a été calibré : sinon « non calibrée » (voir le guide développeur).


## 6. Piloter le badge depuis un ordinateur

Branché en USB, le badge apparaît comme un port série. L'application `tools/badge_remote.py` affiche l'écran du badge
en grand sur l'ordinateur et le pilote au clavier (flèches = boutons, Maj + flèche = appui long).

```bash
pip install pyserial
python tools/badge_remote.py
```

Voir le [guide développeur](guide_developpeur.md#8-le-protocole-usb-série) pour les touches du port série.


## 7. Préparer la carte SD

### 7.1 La carte

- Toute capacité fonctionne (micro-SD, SDHC, SDXC).
- Formatée en **FAT32** ou **exFAT**, comme le fait un ordinateur ou un appareil photo.
  Le badge ne formate jamais la carte et n'y écrit rien.
- Les noms de fichiers peuvent être longs et contenir des accents.

### 7.2 Les dossiers

```
carte SD
├── IMAGES/     images .epi (200×200, 4 gris)
├── VIDEOS/     vidéos .epv
├── MUSIQUE/    sons .wav (sous-dossiers possibles, pratiques pour le blind test)
└── TEXTES/     textes .txt pour la lecture rapide
```

### 7.3 Convertir ses fichiers

Les scripts sont dans le dépôt du projet. Ils demandent Python 3 et, pour le son et la vidéo,
[ffmpeg](https://ffmpeg.org/) installé et accessible dans le PATH.

```bash
pip install pillow numpy
```

**Images** : n'importe quelle photo (JPEG, PNG...) devient une image 200 × 200 en 4 gris.

```bash
python src/images/image2epi.py photo1.jpg photo2.png -o IMAGES
python src/images/image2epi.py affiche.jpg --fit -o IMAGES        # image entière, avec des bandes blanches
python src/images/image2epi.py paysage.jpg --equalize --preview    # plus de contraste, et un aperçu .png
```

Options utiles :
- `--fit` : l'image n'est pas recadrée ;
- `--contrast 3` : contraste plus fort ;
- `--equalize` : pour les images ternes ;
- `--bw` : noir et blanc pur ;
- `--preview` : écrit un aperçu en .png.

**Vidéos** : la vidéo est réduite à 200 × 200, en noir et blanc, 10 images par seconde, avec le son pour le buzzer.

```bash
python src/video/video2epaper.py film.mp4 -o VIDEOS/film.epv
python src/video/video2epaper.py film.mp4 --fit --start 60 --duration 30 -o VIDEOS/extrait.epv
```

Options utiles :
- `--fit` : l'image n'est pas recadrée ;
- `--fps 10|20|30` : 10 donne le meilleur contraste ;
- `--no-audio` : sans le son ;
- `--preview apercu.gif` : écrit un aperçu animé.

**Sons** : MP3, OGG, FLAC... deviennent des WAV adaptés au buzzer (8 bits, 16 kHz, son compressé pour être audible).

```bash
python src/audio/audio2wav.py "musiques/*.mp3" -o MUSIQUE/Films
python src/audio/audio2wav.py dossier_complet -o MUSIQUE
```

**Textes** : n'importe quel fichier `.txt` (UTF-8 ou Windows/Latin-1) dans `TEXTES`.

> Respectez les droits d'auteur : utilisez des œuvres libres de droits ou dont vous avez les droits.


## 8. En cas de problème

| Problème | Solution |
|---|---|
| Le badge ne s'allume pas | Interrupteur sur ON ? Batterie chargée (brancher en USB) ? |
| Le badge n'est pas vu par l'ordinateur | Câble USB de données (pas un câble de charge seule) ; interrupteur sur ON. |
| « Carte SD absente » | Carte bien enfoncée ? Formatée en FAT32 ou exFAT ? |
| Un fichier n'apparaît pas | Bonne extension (`.epi`, `.epv`, `.wav`, `.txt`) et bon dossier ? Nom de moins de 64 caractères ? |
| Pas de son | Volume à 0 ? (Médias > Volume). Le buzzer est discret : collez l'oreille. |
| Le message radio n'arrive pas au Flipper | Le Flipper doit être sur 433,92 MHz dans « SubGHz chat ». Dans Infos, le quartz utilisé doit être 26 ou 27 MHz. |
| Plus de son ni de LEDs | Le mode muet est peut-être actif (commande des organisateurs pendant un talk) : Réglages > Mode muet. |
| Le badge n'obéit pas au Flipper | Réglages > Télécommande : oui ? Code Princeton `0xC16Axx`, envoyé depuis un fichier `.sub` et maintenu une seconde. |
| Les autres cigales ne sont plus entendues | Normal tant que le Décodeur 433 MHz, la Station météo ou le Badge de talk est ouvert : ils occupent la radio. |
| L'écran garde des traces | Normal après de nombreux rafraîchissements rapides : il se nettoie au prochain rafraîchissement complet. |
| Le badge ne répond plus | Bouton RESET, ou éteindre / rallumer. |
