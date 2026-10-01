# Brainstorm : tout ce qu'on pourrait encore ajouter à la cigale

Liste ouverte, sans filtre, pour faire le tri ensuite. Elle complète [idees_applications.md](idees_applications.md)
et [idees_reseau_extensions_ctf.md](idees_reseau_extensions_ctf.md) sans les répéter.

Le badge sait déjà beaucoup de choses : chœur, virus, messages, contacts vCard, votes, radar, chaud-froid, duels,
annonces, images radio, LEDs pilotées, chasse 433 MHz, CTF, jeux, vidéos et musique.

**Ce qu'il y a dans la cigale** : RP2040 (2 cœurs, PIO), radio CC1101 433 MHz, e-Paper 200×200, émetteur et
récepteur IR, haut-parleur (son PWM, jusqu'à ~19 kHz), 2 LEDs RGB, carte SD, 4 boutons (2 ailes, 2 flancs),
USB, ports d'extension, batterie Li-ion.

**Légende**

- Effort : ★ facile (moins d'un jour), ★★ moyen, ★★★ gros chantier.
- 🛠 demande du matériel en plus.
- 🤪 idée farfelue, assumée.

---

## 1. Social et rencontres

| # | Idée | Effort |
|---|---|---|
| 1 | **Speed-dating de compétences** : chaque cigale annonce 3 tags (« RF », « Web », « Crypto »...) ; deux cigales proches qui partagent un tag clignotent et affichent « Parlez de RF ! » | ★★ |
| 2 | **Bingo humain** : grille 4×4 (« un orateur », « quelqu'un venu de plus de 500 km », « un badge staff »...), cochée automatiquement par les rencontres radio | ★★ |
| 3 | **Poignée de main radio** : coller les deux badges (RSSI très fort) et appuyer en même temps : rencontre « certifiée », qui rapporte plus de points | ★ |
| 4 | **Arbre généalogique du virus** : qui a contaminé qui, affiché en fin de journée sur le badge admin ou exporté en graphe | ★★ |
| 5 | **Le fil rouge** : on reçoit un message à transmettre à une personne précise qu'on ne connaît pas, de cigale en cigale | ★★ |
| 6 | **Compliments anonymes** : envoyer un compliment prédéfini à la cigale la plus proche, sans signature | ★ |
| 7 | **Constellations** : les badges rencontrés deviennent des étoiles, et la veille dessine votre ciel personnel | ★★ |
| 8 | **Mode « je cherche »** : « je cherche un co-voiturage pour Marseille », « je cherche quelqu'un qui connaît le SDR » ; les cigales proches l'affichent | ★ |
| 9 | **Statut d'humeur** : 🙂 / ☕ / 🧠 « en plein focus » / 🍻, visible sur le radar des autres | ★ |
| 10 | **Tribus** : à la première rencontre on rejoint la tribu de la cigale la plus fréquentée ; les tribus se disputent des points | ★★ |
| 11 | **Le ver d'oreille** : une mélodie contagieuse, comme le virus mais en musique ; on « l'attrape » et on la fredonne au buzzer | ★ |
| 12 | **Livre d'or radio** : chaque cigale garde une phrase de chaque personne rencontrée, à relire après l'événement | ★★ |
| 13 | **Chaises musicales** : l'admin lance une musique sur toutes les cigales ; à l'arrêt, les dernières à appuyer sont éliminées | ★★ |
| 14 | **Loup-garou** : rôles distribués par radio, nuits et votes sur le badge, maître du jeu sur le badge admin | ★★★ |
| 15 | **Mafia des cigales** : version plus légère du loup-garou, jouable entre 2 talks | ★★ |
| 16 | **Le parrain** : un vétéran parraine un nouveau ; les deux gagnent des points quand le filleul fait des rencontres | ★ |

## 2. Jeux multi-cigales

| # | Idée | Effort |
|---|---|---|
| 17 | **Pétanque en duel radio** : jauge de force, la boule « roule » d'un écran à l'autre | ★★ |
| 18 | **Morpion géant** : une grille 10×10 partagée par toute la salle, chacun pose un pion par minute | ★★ |
| 19 | **Pong inter-badges** : la balle sort de l'écran d'une cigale et entre dans celle d'à côté (e-Paper lent : partie au ralenti) | ★★ 🤪 |
| 20 | **Course de relais** : un « bâton » numérique doit passer par N cigales en un temps record | ★ |
| 21 | **Capture de drapeau** : 2 équipes, des balises 433 MHz cachées à prendre et à défendre en restant à côté | ★★★ |
| 22 | **Assassin (le jeu)** : chacun reçoit une cible ; il faut s'approcher à moins d'un mètre sans être repéré | ★★ |
| 23 | **Bombe à retardement** : une « patate chaude » qui explose (son, LEDs rouges) si on la garde trop longtemps | ★ |
| 24 | **Quiz de salle en direct** : l'orateur pose une question, les réponses sont comptées par radio, histogramme instantané | ★ (proche du vote) |
| 25 | **Enchères** : chaque cigale mise ses points sociaux sur des goodies réels | ★★ |
| 26 | **Bourse aux cigales** : le score de chaque cigale est une action ; on achète, on vend, des krachs aléatoires | ★★★ 🤪 |
| 27 | **Simon collectif** : la séquence de couleurs passe d'un badge à l'autre ; chaque joueur ajoute une note | ★★ |
| 28 | **Tir à la corde** : deux équipes martèlent les ailes, le compteur radio décide | ★ |

## 3. Radio et hacking pédagogique

| # | Idée | Effort |
|---|---|---|
| 29 | **Waterfall 433 MHz** : spectrogramme défilant (RSSI balayé sur 1 MHz), comme un mini SDR | ★★ |
| 30 | **Rejeu commenté** : capturer une télécommande à code fixe, la rejouer, et l'écran explique pourquoi ça marche | ★★ |
| 31 | **Rolling code expliqué** : une démo KeeLoq-like entre deux cigales, avec l'attaque « RollJam » expliquée (sans brouillage réel) | ★★★ |
| 32 | **Oscilloscope du pauvre** : visualiser la trame OOK démodulée (GDO0) sur l'écran | ★★ |
| 33 | **Décodeur de Morse** : écouter un signal OOK manuel et le traduire en texte | ★ |
| 34 | **Émetteur Morse « QRP »** : apprendre le Morse avec l'aile droite et l'émettre en OOK, reçu par les autres cigales | ★ |
| 35 | **POCSAG** : afficher les pages des bipeurs de démonstration émises par l'orga (pas les vrais réseaux) | ★★ |
| 36 | **Détecteur de brouilleur** : alerte quand le plancher de bruit monte anormalement | ★ |
| 37 | **Chasse au renard (ARDF)** : un vrai foxhunting avec affichage de la direction par la variation du RSSI | ★★ |
| 38 | **Mode « honeypot radio »** : la cigale imite une prise télécommandée et note qui essaie de la piloter | ★★ |
| 39 | **IR universel** : couper les télés et vidéoprojecteurs (la base TV-B-Gone), en mode « pédagogique » seulement 😇 | ★ |
| 40 | **Apprendre l'IR** : décoder NEC, RC5 et Sony, et afficher l'adresse et la commande | ★ |
| 41 | **Lien IR entre cigales** : échange de contacts « en pointant », plus discret que la radio | ★★ |
| 42 | **Chiffrement des messages** : AES sur le RP2040, clé partagée par une poignée de main IR | ★★ |
| 43 | **Signature des scores** : HMAC pour que les scores affichés au classement soient infalsifiables (ou presque : défi CTF) | ★★ |
| 44 | **Glitching pédagogique** : un défi CTF où l'on doit faire sauter une vérification en coupant l'alimentation au bon moment | ★★★ 🛠 |
| 45 | **Firmware avec une faille volontaire** : un dépassement de buffer dans une commande série, à exploiter pour un flag | ★★ |
| 46 | **SWD exposé** : un défi de lecture de la flash par SWD avec un deuxième Pico | ★★ 🛠 |
| 47 | **Analyseur logique PIO** : 4 voies sur les ports d'extension, capture affichée sur l'écran ou envoyée au PC | ★★ |
| 48 | **Pont USB ↔ radio** : la cigale devient un modem 433 MHz pour le PC (scripts Python, GNU Radio) | ★ |
| 49 | **Ultrason** : transmission de données à 18–20 kHz entre badges par le haut-parleur (micro en extension) | ★★ 🛠 |

## 4. CTF, quêtes et énigmes

| # | Idée | Effort |
|---|---|---|
| 50 | **Énigme sur plusieurs jours** : un indice se débloque chaque matin | ★ |
| 51 | **Flag dans l'image de veille** : de la stéganographie dans les pixels de l'écran | ★ |
| 52 | **Flag dans une vidéo** : une frame unique, visible seulement en lecture image par image | ★ |
| 53 | **Flag dans une musique** : un spectrogramme qui dessine le flag (classique, mais toujours efficace) | ★ |
| 54 | **Flag dans la courbe des LEDs** : les LEDs clignotent en binaire à un moment précis | ★ |
| 55 | **Flag à plusieurs** : 4 morceaux sur 4 cigales différentes ; il faut se réunir à 4 | ★★ |
| 56 | **Flag au « mauvais » endroit** : seulement en se tenant près d'une balise cachée dans les toilettes 🤪 | ★ |
| 57 | **Énigme du Konami code étendu** : codes secrets cachés dans les menus | ★ |
| 58 | **Mini-VM** : une machine virtuelle 8 bits dans le firmware, avec un programme chiffré à rétro-ingénierer | ★★★ |
| 59 | **Chasse au QR** : des QR codes dans le lieu, scannés... par un humain, qui tape le code sur la cigale | ★ |
| 60 | **Escape game de salle** : une salle fermée, les indices arrivent sur les cigales présentes | ★★★ |
| 61 | **Leaderboard en direct** : écran géant alimenté par une cigale « passerelle » branchée en USB | ★★ |
| 62 | **Succès (achievements)** : « a vu 50 cigales », « a fini Sokoban », « a écouté toute la musique »... | ★ |

## 5. Écran e-Paper et image

| # | Idée | Effort |
|---|---|---|
| 63 | **Niveaux de gris** : 4 niveaux grâce aux formes d'onde de l'e-Paper, pour des photos plus belles | ★★ |
| 64 | **Tramage Floyd-Steinberg** en direct pour les images de la SD | ★ |
| 65 | **Éditeur de pixel art** partagé : dessiner à plusieurs sur la même toile par radio | ★★ |
| 66 | **Fond d'écran du jour** : l'admin envoie une image à toutes les cigales chaque matin | ★ (l'envoi d'image existe) |
| 67 | **Horloge analogique** sur la veille, rafraîchie à la minute | ★ |
| 68 | **Calendrier de l'événement** : la veille montre la salle et l'heure du prochain talk choisi | ★ |
| 69 | **Mode portrait** : photo + nom + pronoms + « demandez-moi : ... » | ★ |
| 70 | **Avatar généré** : un identicon unique dessiné à partir de l'identifiant de la cigale | ★ |
| 71 | **Bande dessinée** : une BD en cases sur la SD, une case par appui | ★ |
| 72 | **Livre dont vous êtes le héros** : choix avec les ailes, histoire sur la SD | ★ |
| 73 | **Lecture de l'écran à voix haute** : synthèse vocale très simple (formants) pour l'accessibilité | ★★★ 🤪 |
| 74 | **Mode « gros caractères »** : polices agrandies, pour l'accessibilité | ★★ |
| 75 | **Génératif** : fractales (Mandelbrot), automates cellulaires (jeu de la vie) sur la veille | ★ |
| 76 | **Jeu de la vie partagé** : les cellules « migrent » d'une cigale à l'autre par radio | ★★ 🤪 |

## 6. Son et musique

| # | Idée | Effort |
|---|---|---|
| 77 | **Synthé** : les 4 boutons + accords ; flanc maintenu = octave | ★ |
| 78 | **Séquenceur 16 pas** affiché sur l'écran, partagé par radio pour faire un groupe | ★★ |
| 79 | **Lecteur de MOD / chiptune** : de vraies musiques de démo sur la SD | ★★ |
| 80 | **Lecteur RTTTL** : les sonneries Nokia, téléchargeables | ★ |
| 81 | **Cigales en stéréo de salle** : chaque badge joue un instrument différent selon sa place | ★★ |
| 82 | **Applaudimètre** : avec un micro en extension, mesurer les applaudissements après un talk | ★ 🛠 |
| 83 | **Diapason / accordeur** avec un micro en extension | ★★ 🛠 |
| 84 | **Le son d'une vraie cigale** selon la « température » (le RSSI, l'heure...) : plus il fait chaud, plus elle chante | ★ |
| 85 | **Radio pirate** : une mélodie émise en FM par le CC1101, captée par... les autres cigales | ★★ 🤪 |

## 7. LEDs

| # | Idée | Effort |
|---|---|---|
| 86 | **Ola dans l'amphi** : l'admin déclenche une vague de LEDs de rang en rang (retard selon le RSSI) | ★★ |
| 87 | **Applaudissements lumineux** : toutes les LEDs flashent à la fin d'un talk | ★ (LEDs pilotées) |
| 88 | **Couleur de tribu** : les LEDs montrent l'équipe | ★ |
| 89 | **Battement de cœur** : les LEDs pulsent plus vite quand on approche d'une cigale « compatible » | ★ |
| 90 | **Morse lumineux** : un message caché émis en continu par les LEDs | ★ |
| 91 | **Light painting** : en photo longue pose, la cigale écrit un mot avec ses LEDs en bougeant | ★★ 🤪 |
| 92 | **Spectre audio sur les LEDs** pendant la musique | ★ |

## 8. Utilitaires et vie de l'événement

| # | Idée | Effort |
|---|---|---|
| 93 | **Minuteur de talk** pour les orateurs, en plus du badge de talk | ★ |
| 94 | **Questions du public** : envoyer une question à l'orateur ou au modérateur, avec un vote pour remonter les meilleures | ★★ |
| 95 | **Retour sur le talk** : 1 à 5 étoiles envoyées au badge admin en fin de talk | ★ |
| 96 | **File d'attente** : « vous êtes 12e pour l'atelier soudure », mise à jour par radio | ★★ |
| 97 | **Objets trouvés** : annonce radio d'un objet perdu | ★ |
| 98 | **Repas** : ticket repas sur l'écran (QR code), coché par le staff | ★★ |
| 99 | **Alerte évacuation** : message prioritaire de l'orga, son fort, LEDs rouges | ★ |
| 100 | **Plan du lieu** sur la SD, avec « vous êtes ici » estimé par les balises | ★★ |
| 101 | **Wifi de l'événement** : QR code du wifi dans les annonces (déjà possible) et une page dédiée | ★ |
| 102 | **Badge de bénévole** : planning du staff, appel d'aide « besoin de bras en salle 2 » | ★★ |
| 103 | **Compteur de cafés** : le café le plus bu de la conférence 🤪 | ★ |
| 104 | **Mot de passe à usage unique** (TOTP) avec horloge réglée par la balise de l'orga | ★★ |

## 9. Matériel et extensions

| # | Idée | Effort |
|---|---|---|
| 105 | **Accéléromètre** : secouer pour mélanger (2048, taquin), compter les pas, détecter une chute | ★ 🛠 |
| 106 | **Capteur de CO₂** : la cigale « étouffe » quand la salle n'est pas aérée | ★ 🛠 |
| 107 | **Module NFC** : échange de contacts par contact, lecture de cartes (pédagogique) | ★★ 🛠 |
| 108 | **Module LoRa** : messages entre cigales à plusieurs kilomètres, et Meshtastic | ★★★ 🛠 |
| 109 | **Module BLE** (ex. nRF) : le badge visible depuis un téléphone, application compagnon | ★★★ 🛠 |
| 110 | **Petit écran OLED couleur** en extension : jeux d'action | ★★ 🛠 |
| 111 | **Panneau solaire** : la cigale qui chante au soleil, vraiment 🤪 | ★★ 🛠 |
| 112 | **Vibreur** : notifications pendant les talks, sans son | ★ 🛠 |
| 113 | **Joystick** en extension pour les jeux | ★ 🛠 |
| 114 | **Haut-parleur plus gros** en boîtier « boombox » pour le chœur | ★ 🛠 |
| 115 | **Mesure de radioactivité** avec un tube Geiger 🤪 | ★★ 🛠 |
| 116 | **Clé USB HID** : la cigale tape un texte sur le PC (démo BadUSB, en mode pédagogique) | ★★ |
| 117 | **Clé de sécurité FIDO2** sur le RP2040 (projets existants) | ★★★ |
| 118 | **Contrôleur MIDI USB** : les boutons et le RSSI pilotent un logiciel de musique | ★★ |

## 10. Plateforme et outils

| # | Idée | Effort |
|---|---|---|
| 119 | **Applications sur la SD** : scripts (MicroPython ou petit interpréteur) lancés depuis la carte, sans reflasher | ★★★ |
| 120 | **Mise à jour par la SD** ou **par radio** (OTA de cigale en cigale) 🤪 | ★★★ |
| 121 | **Application web** (WebSerial) : remplacer badge_remote.py par une page web, sans Python | ★★ |
| 122 | **Émulateur PC** du badge : développer et tester les applications sans matériel | ★★★ |
| 123 | **SDK des applications** documenté, avec un concours d'applications pendant l'événement | ★★ |
| 124 | **Mode démo** en boucle pour le stand (toutes les fonctionnalités l'une après l'autre) | ★ |
| 125 | **Journal de bord** exporté sur la SD : rencontres, scores, flags, pour des statistiques après l'événement | ★ |
| 126 | **Économie d'énergie** : mettre en veille le CC1101 et le RP2040 entre deux balises, et afficher l'autonomie estimée | ★★ |

## 11. Idées vraiment farfelues 🤪

| # | Idée |
|---|---|
| 127 | **La cigale qui meurt en hiver** : après l'événement, elle « hiberne » et ne se réveille qu'au SecSea suivant (avec ses souvenirs) |
| 128 | **Élection du roi des cigales** : le badge le plus rencontré devient « roi » et peut envoyer un message à tous, une fois |
| 129 | **Mode fourmi** : la cigale économise ses points ; la fable de La Fontaine en mini-jeu |
| 130 | **Cigale espionne** : un badge reçoit secrètement la mission de rencontrer tous les orateurs sans se faire repérer |
| 131 | **Horoscope radio** : basé sur le dernier octet de l'identifiant, mis à jour chaque matin par l'admin |
| 132 | **Tamagotchi collectif** : un seul animal pour toute la conférence ; il faut le nourrir à plusieurs |
| 133 | **Mistral** : quand le vent souffle (réglé par l'admin), les jeux deviennent plus durs et les messages « s'envolent » |
| 134 | **Concert final** : toutes les cigales jouent la même musique, synchronisée, pour la clôture |
| 135 | **Message dans une bouteille** : un message lâché qui dérive au hasard de cigale en cigale pendant des jours |
| 136 | **Fantômes** : les cigales éteintes laissent un « fantôme » sur le radar pendant une heure |
| 137 | **Mode ivre** : après 23 h, les menus tremblent et les lettres se mélangent 🍻 |
| 138 | **Cigale qui ronfle** : en veille la nuit, un ronflement toutes les 10 minutes (désactivable, promis) |
| 139 | **Cigale contrebandière** : des objets virtuels rares à échanger en douce entre badges |
| 140 | **Le badge maudit** : un seul badge porte une malédiction qui se transmet par rencontre ; le dernier à l'avoir à la clôture gagne un prix |

---

## Pistes pour le tri

- **Vite faits et visibles** : 3, 6, 9, 23, 62, 66, 67, 87, 93, 95, 99, 124.
- **Gros effet en salle** : 13, 24, 86, 94, 134.
- **Pédagogie sécurité** : 29, 30, 32, 40, 43, 45, 47.
- **Pour la prochaine révision du matériel** : 105, 106, 107, 108, 112.
