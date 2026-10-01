# Badge SecSea — guide utilisateur

Ce guide explique comment utiliser le badge SecSea de Hack In Provence : allumer, naviguer dans les menus,
profiter des médias, des jeux, des fonctions sociales, de la radio et des extensions, et préparer une carte SD.
Pour modifier le logiciel du badge, voir le [guide développeur](guide_developpeur.md).
Toutes les pages du badge en images, avec leurs textes : [les écrans du badge](ecrans.md).

*English version: [user guide](../en/user_guide.md).*


## 1. Le badge en bref

- Un écran e-Paper (encre électronique) de 200 × 200 pixels, en noir et blanc ou 4 niveaux de gris.
  L'image reste affichée même sans courant.
- 4 boutons, 2 LEDs de couleur, un buzzer.
- Une radio 433 MHz (CC1101) compatible avec le Flipper Zero. Les badges s'en servent pour se parler :
  messages, votes, jeux à deux, échange de cartes de visite, choeur...
- Un lecteur de carte micro-SD pour les vidéos, musiques, textes, images, sonneries et livres-jeux.
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
| **Médias** | Images, Vidéos, Musique, Sonneries, Lecture rapide, Livres-jeux, Volume |
| **Jeux** | Morpion, Puissance 4, Simon, Réflexes, Snake, Démineur, 2048, Taquin, Sokoban, Mastermind, Pendu, Blind test, CTF, Défis crypto, Duel, Bataille navale, Loup-garou, Assassin, Tir à la corde |
| **Social** | Réseau des cigales, Messages, Contacts, Compétences, Programme, Vote, Radar des cigales, Chaud - froid, Virus des cigales, Choeur, Annonces, Contrebande |
| **Radio & IR** | Message radio, porteuse radio, Décodeur 433 MHz, Station météo, Envoyer une image, Recevoir une image, Infrarouge, Chasse 433 MHz, Écouter la radio pirate |
| **Badge** | Badge nominatif, Lampe, Badge de talk, son de la cigale, animations des LEDs, démo de l'écran, écran OLED, Succès |
| **Réglages** | Veille de l'écran, Télécommande, Mode muet, Infos, Crédits, Réglage radio |

Un septième thème, **Admin**, est caché : il est réservé aux organisateurs (voir [§ 4.8](#48-le-mode-admin-organisateurs)).

**Notifications** : quand un message, une question de vote, une invitation à jouer ou une annonce des organisateurs
arrive, le badge émet un bip et l'écrit en bas de l'écran. Depuis les menus ou la veille, la page concernée s'ouvre
directement (vote, programme, virus, invitation au duel, à la bataille navale ou à une affaire de contrebande,
nouvelles de l'assassin et du loup-garou, annonce en plein écran).
Un nouveau succès (« Succès : Sociable ») ou un nouveau niveau s'affiche aussi en bas de l'écran, avec un carillon.


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
- Les fichiers WAV sont lus en PCM 8, 16, 24 ou 32 bits, ou en flottant 32 bits, mono ou stéréo, de 4 à 192 kHz ;
  le mieux reste le 8 bits 16 kHz mono de `audio2wav.py` (voir [§ 7.3](#73-convertir-ses-fichiers)).

**Sonneries** (dossier `SONNERIES`, facultatif) : des sonneries au format RTTTL, celui des vieux téléphones Nokia.
12 mélodies du domaine public sont dans le badge (Lettre à Élise, Ode à la joie, Frère Jacques, La Marseillaise,
Korobeiniki...), celles de la carte SD suivent.
- Flancs : choisir ; aile droite : jouer ; aile gauche : retour. Une sonnerie marquée `(!)` contient une erreur :
  aile droite affiche la ligne, la colonne et la raison.
- Pendant la lecture : le nom, la note jouée, une barre de progression ; les LEDs s'allument d'une couleur par note.
  Aile gauche : arrêter ; flancs : sonnerie précédente / suivante ; aile droite : reprendre au début.
- Le format et l'ajout de sonneries : [sonneries.md](sonneries.md).

**Lecture rapide** (dossier `TEXTES`)
- Les mots s'affichent un par un au même endroit, les yeux ne bougent plus : on lit beaucoup plus vite
  (méthode PVSR, voir [pvsr.md](../pvsr.md)).
- Aile droite : pause / reprise ; aile gauche : quitter.
- Flancs (appui court) : vitesse −/+ (de 100 à 900 mots par minute, 250 au départ).
- Flancs (appui long) : recul / avance d'environ 10 secondes de lecture.
- La position est retenue : un texte reprend là où vous l'avez quitté.

**Livres-jeux** (dossier `LIVRES`, facultatif) : des livres dont vous êtes le héros. Un livre est dans le badge,
« Le Trésor du capitaine Cigalon », les livres `.txt` de la carte SD suivent (16 au plus).
- La liste : « Continuer : ... » reprend le dernier livre ; flancs : choisir ; aile droite : ouvrir ; aile gauche : quitter.
- Le menu d'un livre : Commencer, Reprendre la lecture, Recommencer, Objets, Autres livres.
- La lecture : flanc droit : page suivante, puis choix suivant ; flanc gauche : choix précédent, puis page précédente ;
  aile droite : prendre le choix sélectionné ; aile gauche : le menu du livre ; aile gauche maintenue : quitter.
- Dés, objets, fins gagnées ou perdues ; la progression est gardée même badge éteint (un seul livre à la fois).
- Lire, ajouter et écrire des livres : [livres_jeux.md](livres_jeux.md).

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

**Jeux de groupe** : **Tir à la corde**, **Assassin** et **Loup-garou** se jouent à plusieurs badges, par radio,
avec le même salon :
- « Créer une partie » (« Mener une partie » au loup-garou) : votre badge héberge la partie et liste les joueurs
  qui la rejoignent. Aile droite : lancer (quand il y a assez de joueurs) ; aile gauche : annuler.
- « Rejoindre une partie » : les parties ouvertes autour (nom de l'hôte, nombre de joueurs), la plus proche en
  premier. Aile droite : rejoindre ; la « Salle d'attente » attend le lancement ; aile gauche : quitter.
- Une seule partie de groupe à la fois sur un badge. La partie continue pendant que vous utilisez les autres pages :
  appui long sur l'aile gauche pour quitter la page sans quitter la partie.

**Tir à la corde** (2 joueurs au moins) : deux équipes, les **Cigales** et les **Fourmis**, tirées au hasard ;
avec un nombre impair de joueurs, l'un d'eux est l'**arbitre** (il regarde la corde).
- Les équipes s'affichent 5 s, puis un compte à rebours 3, 2, 1 (bips, LEDs jaune, orange, rouge), puis
  « Tirez ! » pendant 20 s : **aile gauche puis aile droite** = une traction (les deux en même temps ne comptent pas).
- Le nœud de la corde se déplace selon les tractions des deux équipes, sur tous les badges. Une équipe qui prend
  20 tractions d'avance par joueur de l'équipe atteint sa marque et gagne tout de suite ; sinon, la plus forte à la
  fin des 20 s gagne.
- Le résultat : l'équipe gagnante, le score « Cigales - Fourmis », vos tractions et le meilleur tireur ; LEDs vertes
  pour les gagnants, rouges pour les perdants. Aile droite : revenir au choix Créer / Rejoindre ; aile gauche : retour.

**Assassin** (3 joueurs au moins ; 2 quand l'hôte est en mode admin) : un jeu qui dure toute la conférence.
- Au lancement, l'hôte tire un cercle secret : chaque joueur reçoit une **cible**, et lui-même est la cible
  d'un autre.
- La page : « Ta cible : » et son nom, une jauge chaud - froid d'après ses signaux (« Glacial », « Froid », « Tiède »,
  « Chaud », « Brûlant ! », ou « Pas captée »), le nombre de survivants.
- **Aile droite : éliminer**. Il faut être tout près de la cible, les badges presque collés (le badge de la cible
  vérifie la force du signal ; seuil provisoire, à régler sur place). « Cible éliminée ! » : la cible de votre victime
  devient la vôtre. « Trop loin ou absente » : réessayez plus près.
- La victime voit « Éliminé par ... » ; sa cible passe à son tueur, puis aile droite : quitter la partie.
- Flanc : abandonner (confirmer avec l'aile droite) : votre cible passe à votre chasseur.
- La dernière cigale debout gagne. Les nouvelles (« Éliminé par ... », « Assassin : nouvelle cible »,
  « Assassin : victoire ! ») arrivent même depuis une autre page.
- Le badge de l'hôte doit rester allumé ; un badge qui redémarre quitte la partie.

**Loup-garou** (5 à 20 joueurs, 7 à 20 en mode avancé, plus un meneur) : le jeu du loup-garou, sans cartes.
- Le meneur, qui ne joue pas, choisit « Mener une partie » (mode simple ou avancé, durée du débat : 2, 3 ou
  5 minutes) puis lance la partie ; les joueurs choisissent « Rejoindre une partie ».
- Chacun découvre son rôle secret sur son badge (**cachez votre écran**) : loup-garou, voyante, villageois, et en mode
  avancé sorcière, chasseur, Cupidon. Le badge du meneur enchaîne les phases (nuit, aube, débat, vote, verdict) et
  tous les badges sonnent à chaque nouvelle phase.
- La nuit, tous les joueurs vivants choisissent un nom dans une liste (les villageois font semblant) : personne ne
  devine les rôles en regardant qui appuie. Le jour, chacun vote sur son badge.
- Aile gauche : retour au menu, la partie continue ; aile gauche maintenue : quitter la partie ; aile droite maintenue :
  revoir son rôle.
- Les rôles, les phases, les votes et les conseils au meneur : [loup_garou.md](loup_garou.md).


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
  votre accord. Votre carte est renvoyée toutes les 3 secondes environ ; la carte reçue s'affiche : aile droite :
  la garder ; aile gauche : l'ignorer. Aile gauche sur la page d'échange : arrêter.
- La carte part **en clair**, au format vCard, sur le canal du « SubGHz chat » du Flipper Zero : n'importe quel
  Flipper à portée qui lance `subghz chat 433920000 0` la lit pendant l'échange. Seuls les champs cochés sont envoyés.
- Un Flipper peut aussi **envoyer** une carte au badge en mode échange : dans `subghz chat 433920000 0`, taper les
  lignes une par une, par exemple `BEGIN:VCARD`, `N:Nom;Prénom;;;`, `TEL:0612345678`, `END:VCARD`.
- « Contacts reçus » : les 12 dernières cartes gardées (la plus ancienne est oubliée au-delà).
  Aile droite : voir ; aile droite (appui long) : supprimer.
- Sur l'ordinateur, `tools/contacts_export.py` exporte les cartes reçues en fichier vCard (`.vcf`),
  à importer dans un téléphone ou un carnet d'adresses.
- Vos compétences (ci-dessous) partent avec la carte (ligne `CATEGORIES` de la vCard) dès qu'au moins une est
  cochée ; celles d'une carte reçue s'affichent en pictogrammes en bas de la carte.

**Compétences** : 20 compétences, chacune avec son pictogramme : Électronique, Flipper Zero, Android, iOS,
Radio / SDR, Web, Réseau, Crypto, Reverse, Pentest, Forensic, OSINT, Linux, Windows, Cloud, IA, Développement, CTF,
Lockpicking, Défense.
- « Mes compétences » : la liste avec des cases ; aile droite : cocher / décocher. Les pictogrammes des compétences
  cochées s'affichent sur la page, sur le badge nominatif et partent dans les signaux du réseau des cigales.
- « Qui les partage ? » : les cigales entendues qui partagent au moins une de vos compétences, avec les pictogrammes
  en commun (mis à jour toutes les 3 s).
- Quand une cigale qui partage une compétence s'approche (aussi près que pour une rencontre), le badge l'annonce
  en bas de l'écran, avec un carillon : « Marius aime aussi : Radio / SDR » (une fois par visite).
- Les badges d'un firmware plus ancien n'envoient pas leurs compétences.

**Programme** : le programme de la conférence, sans carte SD. Aile droite : le détail d'un talk, avec le QR code
de son lien ; flancs : talk précédent / suivant.
Le programme de SecSea 2026 n'est pas encore publié : les talks affichés sont provisoires.

**Vote** : quand les organisateurs posent une question (« Ce talk vous a plu ? »...), la page s'ouvre.
Flancs : choisir la réponse ; aile droite : voter. On peut changer d'avis tant que le vote est ouvert :
seul le dernier vote de chaque badge compte.

**Radar des cigales** : les cigales entendues, avec leur niveau (« N3 ») et la force de leur signal (en dBm, « * » pour une cigale déjà
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

**Annonces** : les annonces des organisateurs (pause café, prochain talk...). Une annonce est un écran construit
par le badge : l'heure dans un bandeau noir, le texte, et souvent un QR code (un lien, un numéro de téléphone,
un réseau Wi-Fi...) à scanner avec un téléphone.
- Quand le badge est sur les menus ou en veille, l'annonce s'affiche toute seule, comme l'image de veille
  (nette, elle reste affichée) ; n'importe quel bouton ramène au menu.
- Sinon, le badge émet un bip et l'écrit en bas de l'écran : la prochaine ouverture de Social > Annonces
  affiche directement la nouvelle annonce.
- La page garde les 5 dernières annonces reçues (heure et texte). Flancs : choisir ; aile droite : l'afficher ;
  aile gauche : retour.

**Contrebande** : la cigale contrebandière. 26 marchandises virtuelles (vivres, rhum, épices, trésors), communes,
rares ou légendaires, à collectionner et à échanger **en douce** entre deux badges tenus l'un contre l'autre.
- L'accueil : Cale, Échanger en douce, Donner, Collection (x / 26), Fortune (en doublons, avec un rang).
- La cale reçoit 4 marchandises communes à la première ouverture ; chaque nouvelle cigale rencontrée donne une chance
  sur deux d'en trouver une autre (message discret en bas de l'écran, sans son).
- « Échanger en douce » : les cigales **à portée de main** (badges collés) ; aile droite : proposer une affaire.
  L'autre reçoit « Psst... » et accepte (aile droite) ou refuse (aile gauche) ; chacun choisit sa marchandise, les deux
  offres s'affichent, aile droite : conclure. « Donner » offre une marchandise sans rien en retour.
- Le détail des pages, des marchandises et du protocole : [contrebande.md](contrebande.md).


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
elle est convertie en noir et blanc et envoyée deux fois par radio aux badges qui l'attendent (environ 17 secondes).
Aile gauche : arrêter.

**Recevoir une image** : attendez qu'un autre badge envoie une image. Pendant la réception, la page indique
le nombre de blocs reçus, avec une jauge ; des blocs de contrôle permettent de reconstituer ceux qui se sont perdus
en route. Aile gauche : arrêter ; aile droite : recommencer (attendre une autre image).
Une fois complète, l'image s'affiche comme la veille (nette, sans traces), avec « Reçue » et le nombre
de blocs corrigés ; n'importe quel bouton ramène au menu.

**Infrarouge** (module sur le port droit)
- « Enregistrer un signal » : visez le récepteur avec une télécommande et appuyez sur une touche.
- Les 4 emplacements rejouent les signaux enregistrés, comme une télécommande.

**Chasse 433 MHz** (réception seule) : retrouvez au « chaud - froid » un émetteur 433 MHz caché (télécommande,
sonde, brouilleur qui répète son code).
- La page liste les codes entendus (8 au plus) : protocole et code, force de la dernière trame (dBm) et nombre
  de trames reçues (« x3 »...). Flancs : choisir ; aile droite : le chasser ; aile gauche : retour.
- Le badge suit alors ce code comme Social > Chaud - froid : de « Glacial » à « BRÛLANT ! », jauge, LEDs du bleu
  au rouge et bips de plus en plus rapides. La force est mesurée sur chaque trame reçue : l'émetteur doit émettre.
  Sans trame pendant 15 s, il est « hors de portée ». Aile gauche : retour à la liste.
- Pendant la chasse, le badge n'entend plus les autres cigales.

**Écouter la radio pirate** (réception seule) : le badge reçoit le son FM qu'émet un badge organisateur
(Admin > Radio pirate) et le joue sur son buzzer.
- La page : la fréquence, la force du signal (dBm et jauge), la correction de fréquence, la tonalité détectée,
  le niveau.
- Flancs : canal précédent / suivant (les 5 canaux de la radio pirate, 433,920 MHz au départ) ; aile droite :
  silencieux oui / non ; aile gauche : retour.
- Pendant l'écoute, le badge n'entend plus les autres cigales. Le principe et les essais : [radio_pirate.md](radio_pirate.md).


### 4.5 Badge

- **Badge nominatif** : « SecSea 2026 », le nom de votre cigale en grand et votre type (PARTICIPANT, ORATEUR ou STAFF)
  dans un bandeau noir, puis les pictogrammes de vos compétences (Social > Compétences, 10 au plus).
  La veille ne le remplace pas : il reste affiché, même badge éteint.
  Le type est choisi par les organisateurs. Aile gauche : retour.
- **Lampe** : les 2 LEDs en blanc. Flancs : moins / plus fort, par pas de 10 % (en maintenant : plus vite) ;
  aile droite : éteindre / allumer. La luminosité est retenue et affichée dans le menu (« Lampe : 50 % »).
  En mode muet, les LEDs restent éteintes.
- **Badge de talk** : pour les orateurs, les LEDs indiquent le temps de parole :

  | État | LEDs |
  |---|---|
  | Éteint | éteintes |
  | OK | vert fixe : tout va bien |
  | 5 min | orange qui respire doucement : il reste 5 minutes |
  | FINI | rouge qui clignote : le temps est écoulé |
  | STOP ! | rouge qui clignote vite, et la cigale chante à plein volume (même en mode muet) : on conclut ! |

  L'état est changé par la télécommande des organisateurs (voir [§ 4.7](#47-télécommande-et-mode-muet)) ou à la main :
  flanc gauche : état précédent ; flanc droit ou aile droite : état suivant. Cette page fonctionne même en mode muet,
  et écoute la télécommande en permanence tant qu'elle est ouverte.
- **Cigale** : active ou coupe le chant de la cigale.
- **LEDs** : change l'animation (arc-en-ciel, respiration, battement, clignotement, vert fixe, éteintes).
- **Démo écran** : montre les possibilités de l'écran (noir et blanc, 4 gris, animation rapide).
- **Écran OLED** : démos sur un petit écran OLED branché sur le port gauche (étoiles, cube 3D, cigale, texte, vidéo).
- **Succès** : les succès obtenus et le niveau de votre cigale (voir ci-dessous).

**Succès et niveau** : comme le dauphin du Flipper Zero, votre cigale gagne de l'expérience (XP) : chaque succès
donne des XP, et chaque cigale rencontrée **2 XP**. Le niveau va de 1 à 10 :

| Niveau | Nom | XP | Niveau | Nom | XP |
|---|---|---|---|---|---|
| 1 | Oeuf | 0 | 6 | Cigale | 260 |
| 2 | Larve | 20 | 7 | Chanteuse | 380 |
| 3 | Nymphe | 50 | 8 | Virtuose | 530 |
| 4 | Mue | 100 | 9 | Maestro | 720 |
| 5 | Jeune cigale | 170 | 10 | Cigale d'or | 1000 |

La page montre le niveau, les XP (sur ceux du niveau suivant) avec une jauge, le nombre de succès obtenus, puis la
liste (case pleine : obtenu). Flancs : choisir ; aile droite : comment l'obtenir et ses XP ; aile gauche : retour.
Un nouveau succès s'annonce en bas de l'écran (« Succès : Sociable »), ou le nouveau niveau (« Niveau 3 : Nymphe ! »).

| Succès | Comment l'obtenir | XP |
|---|---|---|
| Premiers pas | allumer sa cigale | 5 |
| Bonjour ! | rencontrer une cigale (rester près d'elle) | 10 |
| Sociable | rencontrer 10 cigales | 30 |
| Star du réseau | rencontrer 50 cigales | 80 |
| Facteur | envoyer un message (Social > Messages) | 10 |
| Carte de visite | recevoir un contact (Social > Contacts) | 15 |
| Citoyen | voter (Social > Vote) | 10 |
| Choriste | chanter dans le choeur | 15 |
| Patient | attraper le virus des cigales | 10 |
| Remède | guérir du virus | 20 |
| Duelliste | gagner un pierre-feuille-ciseaux | 20 |
| Amiral | gagner une bataille navale | 30 |
| Pleine lune | jouer au loup-garou | 20 |
| Survivant | gagner au loup-garou | 40 |
| Ombre | éliminer sa cible à l'assassin | 20 |
| Dernier debout | gagner l'assassin | 50 |
| Costaud | gagner le tir à la corde | 20 |
| Héros | finir un livre-jeu | 30 |
| Mélomane | jouer une sonnerie | 5 |
| Contrebandier | échanger une marchandise (Social > Contrebande) | 20 |
| Trésor | obtenir une marchandise légendaire | 50 |
| Collectionneur | posséder toutes les marchandises | 100 |
| Hacker | trouver un flag du CTF | 30 |
| Cryptographe | résoudre un défi crypto | 20 |
| Fin limier | trouver la balise chaud-froid (« BRÛLANT ! ») | 30 |
| Chasseur d'ondes | entendre une télécommande 433 MHz (Chasse 433 MHz) | 15 |
| Recordman | battre un record dans un jeu (ou gagner contre la cigale) | 15 |
| Cinéphile | regarder une vidéo jusqu'au bout | 15 |
| Photographe | envoyer une image par radio | 15 |
| Expert | cocher ses compétences (Social > Compétences) | 10 |
| Âmes soeurs | croiser une cigale qui partage une compétence | 20 |
| Platine | obtenir tous les autres succès | 200 |

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
- **Infos** : 6 lignes : version du firmware (numéro et commit git, un « + » si les sources différaient du commit),
  date de compilation, version de la radio, quartz utilisé (« (mesure) » pendant sa mesure), carte SD, batterie.
  Aile droite : les crédits.
- **Crédits** : les personnes et associations derrière le badge.
- **Réglage radio** : règle la radio automatiquement, en 3 étapes (environ 15 s) :
  1. le quartz de la radio (26 ou 27 MHz) ;
  2. le bruit radio de l'endroit (3 s) : le badge écoute les télécommandes à partir de 15 dB au-dessus du bruit ;
  3. la fréquence, d'après les paquets des autres cigales entendues pendant 10 s : restez près d'autres badges
     allumés.

  Le réglage se fait tout seul au premier démarrage du badge (et après une mise à jour qui l'apporte), puis
  à la demande : aile droite : régler à nouveau ; aile gauche : arrêter ou revenir. La page affiche le résultat :
  quartz, bruit, seuil des télécommandes (« Télécommandes : > −90 dBm »...), correction de fréquence, et le nombre
  de paquets d'autres cigales entendus (sans autre cigale, la fréquence n'est pas corrigée).


### 4.7 Télécommande et mode muet

Les organisateurs peuvent envoyer des commandes à tous les badges de la salle, depuis un badge en mode admin
ou depuis un Flipper Zero. Le badge affiche la commande reçue en bas de l'écran.

| Commande | Effet |
|---|---|
| 0x01 | la cigale chante quelques secondes |
| 0x02 | **mode muet** : plus de son ni de LEDs (pendant les talks) |
| 0x03 | fin du mode muet |
| 0x10 à 0x14 | lumières du badge de talk (si sa page est ouverte) : éteint, OK, 5 min, FINI, STOP ! |
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

Le badge écoute le réseau des cigales en permanence ; quand il entend un émetteur qui n'est pas un badge, il passe
un instant à l'écoute des télécommandes pour décoder le code, qui doit être reçu deux fois : maintenez l'envoi
une seconde. La commande `subghz tx` de la ligne de commande du Flipper ne transmet pas le code tel quel :
utilisez un fichier `.sub` (elle remplace les 4 derniers bits du code par 6).

Un badge admin envoie ses commandes des deux façons, comme une télécommande puis par le réseau des cigales ;
une commande reçue deux fois n'est exécutée qu'une fois.


### 4.8 Le mode admin (organisateurs)

Dans le menu principal (la liste des thèmes), tapez sur les flancs **gauche, gauche, droit, droit, gauche, droit,
gauche, droit** en moins de 8 secondes : « Mode admin activé » s'affiche et le thème **Admin** apparaît, déjà sélectionné,
après les autres. Le mode admin reste actif après extinction.
Plus discret : la case « Mode admin » de `tools/badge_remote.py`, badge branché en USB (voir [§ 6](#6-piloter-le-badge-depuis-un-ordinateur)).

| Entrée | Rôle |
|---|---|
| **Commandes radio** | envoie une commande à tous les badges autour : mode muet, fin du mode muet, cigale, lumières du badge de talk |
| **LEDs des cigales** | choisit la couleur et l'animation des LEDs de toutes les cigales autour (voir ci-dessous) |
| **Annonces (admin)** | écrit et envoie les annonces à toutes les cigales (voir ci-dessous) |
| **Vote (admin)** | ouvre une question, compte les votes (un par badge) et affiche l'histogramme ; aile droite : fermer le vote |
| **Choeur : lancer** | lance un morceau du choeur ; ce badge chante la première voix |
| **Balise chaud-froid** | ce badge émet une balise par seconde : cachez-le, les autres le cherchent avec Social > Chaud - froid |
| **Virus : patient zéro** | infecte ce badge pour lancer l'épidémie ; flanc gauche : le guérir |
| **Remise à zéro** | efface les scores et la progression de ce badge (voir ci-dessous) |
| **Batterie (calibration)** | calibre la mesure de la batterie avec un multimètre (voir § 5) |
| **Radio pirate** | émet une mélodie, une tonalité de 1 kHz ou un fichier WAV de la carte SD en FM bande étroite sur 433 MHz, à écouter avec un Portapack, un SDR ou une autre cigale (voir [radio_pirate.md](radio_pirate.md)) ; faible puissance, essais courts |
| **Mode démo** | pour un stand : le badge présente ses fonctions en boucle (voir ci-dessous) |
| **Type du badge** | Participant, Orateur ou Staff, affiché par le badge nominatif |
| **Quitter le mode admin** | cache à nouveau le thème Admin |

**LEDs des cigales** : une liste de réglages ; flancs : choisir la ligne.
- « Couleur » : ailes : couleur précédente / suivante (rouge, orange, jaune, vert, cyan, bleu, violet, rose, blanc) ;
- « Rouge (R) », « Vert (G) », « Bleu (B) » : de 0 à 255 ; aile gauche : −, aile droite : + (en maintenant :
  de plus en plus vite) ; la couleur devient « personnalisée » ;
- « Mode » : Fixe, Clignotant ou Fondu (ailes : mode précédent / suivant). En Clignotant ou Fondu, un appui long
  sur l'aile droite ouvre les durées : allumé / éteint, ou vers la couleur / vers le noir, de 50 ms à 5 s par pas
  de 50 ms (flancs : choisir la durée ; ailes : − / + ; appui long sur une aile : retour) ;
- « > Envoyer aux cigales » (aile droite) : les cigales autour, et ce badge, prennent ces LEDs à la place de leur
  animation, jusqu'à « > Rétablir leurs LEDs » ou leur redémarrage. Le mode muet éteint toujours leurs LEDs,
  et les pages qui pilotent elles-mêmes les LEDs (jeux, badge de talk) les gardent.

Pendant le réglage, les LEDs de ce badge montrent la couleur et le mode choisis. Aile gauche longue (ou aile gauche
sur les deux dernières lignes) : quitter.

**Annonces (admin)** : 6 annonces, gardées après extinction ; au départ, des exemples (accueil, pauses, remise des
prix du CTF...). Flancs : choisir ; aile droite : l'ouvrir ; aile gauche : retour. Une annonce a 6 lignes
(flancs : choisir ; aile gauche : retour à la liste) :
- « Heure » (par exemple 10:30) et « Texte » (56 caractères au plus, lettres accentuées comprises) : aile droite :
  modifier, avec l'éditeur à 4 boutons (ou le clavier de l'ordinateur) ;
- « QR code » : le type du QR code, avec les ailes : Aucun, Lien (URL), Texte, Téléphone, SMS, E-mail, Wi-Fi,
  Position GPS ;
- « Contenu » : ce que contient le QR code (aile droite : modifier) ; le badge le met dans la forme que
  comprennent les téléphones :

  | Type | Contenu à saisir | QR code |
  |---|---|---|
  | Lien (URL) | `www.exemple.fr` ou `https://...` | `https://` ajouté s'il manque |
  | Texte | un texte | le texte |
  | Téléphone | `+33612345678` | `tel:+33612345678` |
  | SMS | `numéro:message` | `SMSTO:numéro:message` |
  | E-mail | `adresse@mail.fr` | `mailto:adresse@mail.fr` |
  | Wi-Fi | `réseau;mot de passe` | `WIFI:T:WPA;S:réseau;P:mot de passe;;` (sans « ; » : réseau ouvert) |
  | Position GPS | `43.17,5.60` | `geo:43.17,5.60` |

- « > Aperçu » : l'écran de l'annonce, tel que les cigales l'afficheront (une aile : retour) ;
- « > Envoyer à toutes les cigales » : l'annonce part par radio, 3 fois de suite (pour les cigales qui l'auraient
  manquée). Ce badge ne se l'affiche pas : c'est le rôle de l'aperçu.

**Remise à zéro** : avant l'événement ou après des essais. Flancs : choisir ; aile droite, puis **appui long sur
l'aile droite** pour confirmer (aile gauche : non) :
- « Scores sociaux » : le score et les rencontres du réseau des cigales ;
- « Records des jeux » : les records des jeux et des casse-têtes ;
- « Défis CTF et crypto » : les flags du CTF et les défis crypto résolus ;
- « Contacts reçus » : les cartes de visite reçues (pas votre carte) ;
- « Virus » : l'état du virus (en forme) ;
- « Succès et niveau » : les succès obtenus et leurs compteurs (le niveau repart de 1 ; les rencontres comptent
  toujours dans les XP tant que les scores sociaux ne sont pas remis à zéro) ;
- « Contrebande » : la cale de la contrebande (les marchandises sont distribuées à nouveau à la prochaine ouverture) ;
- « Tout » : tout cela à la fois, plus la progression du livre-jeu. Le nom, les réglages et votre carte de visite
  sont gardés ;
- « Annonces d'origine » : les 6 annonces du menu admin reprennent leurs textes d'origine (pas compris dans « Tout »).

**Mode démo** : aile droite : lancer. Le badge enchaîne en boucle, LEDs en arc-en-ciel : badge nominatif (8 s),
succès (6 s), images de la carte SD (18 s, une nouvelle toutes les 6 s), la première vidéo (20 s), compétences (5 s),
la première musique (10 s), démo de l'écran (15 s), programme (6 s), radar (6 s), livres-jeux (5 s), crédits (6 s),
infos (5 s). Sans carte SD (ou sans fichier), les images, la vidéo et la musique sont sautées. La veille ne démarre
pas pendant la démo ; **n'importe quel bouton l'arrête** (« Mode démo arrêté »), et les LEDs reprennent leur animation.


## 5. Batterie

- La batterie se recharge par le port USB-C, même interrupteur sur OFF.
  LED rouge : en charge ; LED verte : chargée.
- Le niveau de batterie n'est affiché (icône en haut à droite du menu, et dans Infos)
  que si le badge a été calibré : sinon « non calibrée ».
- **Calibrer** (Admin > Batterie (calibration)), une fois par badge :
  1. badge branché en USB (en charge), mesurez la tension de la batterie au multimètre ;
  2. sur la ligne « Multimètre », réglez cette tension avec les ailes (- / +, maintenir : vite) ;
  3. flanc : « Enregistrer le point », aile droite ;
  4. recommencez sur batterie, débranché depuis quelques minutes (la tension doit avoir baissé d'au moins
     0,2 V) : le niveau s'affiche dès ce 2e point.
  Un nouveau point remplace le plus proche. « Effacer » (aile droite deux fois) oublie la calibration.
  La calibration est un réglage « usine » : gardée par la Remise à zéro (même « Tout ») et par les mises à jour
  du firmware.


## 6. Piloter le badge depuis un ordinateur

Branché en USB, le badge apparaît comme un port série. L'application `tools/badge_remote.py` affiche l'écran du badge
en grand sur l'ordinateur et le pilote au clavier (flèches = boutons, Maj + flèche = appui long).

```bash
pip install pyserial
python tools/badge_remote.py
```

- Les boutons de la fenêtre simulent ceux du badge ; sous chacun, « appui long ».
- **Badge :** la liste des badges branchés, pour choisir celui à piloter quand il y en a plusieurs.
- **Mode clavier (saisie de texte)** : quand un éditeur de texte est ouvert sur le badge (carte de visite, réponse
  à un défi...), les caractères tapés sur l'ordinateur y sont insérés au curseur ; Entrée : valider ; Échap : annuler ;
  Retour arrière : effacer. Les flèches restent les boutons. Seuls les caractères sans accent passent par le clavier :
  les lettres accentuées se choisissent avec les flancs.
- **Mode admin** : active ou désactive le mode admin du badge (voir [§ 4.8](#48-le-mode-admin-organisateurs)) ;
  la case suit l'état du badge.
- Un problème de connexion (badge débranché, port occupé) s'affiche dans la ligne d'état et le journal ;
  l'application retrouve le badge quand il revient.

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
├── TEXTES/     textes .txt pour la lecture rapide
├── SONNERIES/  sonneries RTTTL .txt, .rtttl ou .rtx (24 fichiers, 128 sonneries au plus)
└── LIVRES/     livres-jeux .txt (16 au plus)
```

Des exemples de `SONNERIES` et de `LIVRES` sont dans le dépôt, dans [docs/sd/](../sd) : copier ces dossiers à la
racine de la carte.

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

Un WAV non converti est lu aussi (PCM 8, 16, 24 ou 32 bits, flottant 32 bits, jusqu'à 192 kHz), mais moins fort et
moins clair sur le buzzer.

**Textes** : n'importe quel fichier `.txt` (UTF-8 ou Windows/Latin-1) dans `TEXTES`.

**Sonneries** : un fichier texte, une sonnerie RTTTL par ligne (`nom:d=4,o=5,b=120:e,e,f,g`), voir
[sonneries.md](sonneries.md).

**Livres-jeux** : un fichier texte par livre, avec des sections `== 12` et des choix `-> 34 : ...`, voir
[livres_jeux.md](livres_jeux.md) ; `python tools/gamebook_check.py LIVRES/mon_livre.txt` le vérifie avant de le
copier.

> Respectez les droits d'auteur : utilisez des œuvres libres de droits ou dont vous avez les droits.


## 8. En cas de problème

| Problème | Solution |
|---|---|
| Le badge ne s'allume pas | Interrupteur sur ON ? Batterie chargée (brancher en USB) ? |
| Le badge n'est pas vu par l'ordinateur | Câble USB de données (pas un câble de charge seule) ; interrupteur sur ON. |
| « Carte SD absente » | Carte bien enfoncée ? Formatée en FAT32 ou exFAT ? |
| Un fichier n'apparaît pas | Bonne extension (`.epi`, `.epv`, `.wav`, `.txt`, `.rtttl`, `.rtx`) et bon dossier ? Nom de moins de 64 caractères ? |
| Une sonnerie est marquée `(!)` | Une erreur dans la ligne : aile droite affiche la colonne et la raison (voir [sonneries.md](sonneries.md)). |
| Pas de son | Volume à 0 ? (Médias > Volume). Le buzzer est discret : collez l'oreille. |
| Le message radio n'arrive pas au Flipper | Le Flipper doit être sur 433,92 MHz dans « SubGHz chat ». Dans Infos, le quartz doit être 26 ou 27 MHz. |
| Plus de son ni de LEDs | Le mode muet est peut-être actif (commande des organisateurs pendant un talk) : Réglages > Mode muet. |
| Le badge n'obéit pas au Flipper | Réglages > Télécommande : oui ? Code Princeton `0xC16Axx`, envoyé depuis un fichier `.sub` et maintenu une seconde. |
| Les autres cigales ne sont plus entendues | Normal tant que le Décodeur 433 MHz, la Station météo, la Chasse 433 MHz, le Badge de talk, l'échange de contacts ou la radio pirate (émission ou écoute) est ouvert : ils occupent la radio. |
| « Trop loin ou absente » à l'assassin, pas de cigale « à portée de main » en contrebande | Collez les badges l'un contre l'autre : ces jeux demandent un signal très fort. |
| Les télécommandes ou les autres cigales passent mal | Réglages > Réglage radio, près d'autres badges allumés. |
| L'écran garde des traces | Normal après de nombreux rafraîchissements rapides : il se nettoie au prochain rafraîchissement complet. |
| Le badge ne répond plus | Bouton RESET, ou éteindre / rallumer. |
