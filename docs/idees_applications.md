# Idées d'applications pour la cigale SecSea 2026

Brainstorming pour faire du badge SecSea 2026 (Hack In Provence, La Ciotat) **le badge le plus extraordinaire jamais fait**.
Ce document complète [idees_reseau_extensions_ctf.md](idees_reseau_extensions_ctf.md) (réseau social radio entre cigales,
ports d'extension, challenges CTF) : ces idées-là ne sont pas répétées ici, on s'appuie dessus et on va plus loin.

Rappel du matériel (contraintes réelles) : RP2040 (2 × M0+ 133 MHz, 264 Ko de RAM, 2 PIO), flash 16 Mo,
USB CDC, e-Paper 200×200 N/B (4 gris en ~2 s, partiel ~0,3 s), **4 boutons** (ailes G/D, flancs G/D, courts et longs),
2 LEDs RGB WS2812, buzzer PWM (8 bits 16 kHz, son faible), CC1101 433 MHz (portée de quelques dizaines de mètres),
micro-SD, 2 ports 2×6 (I2C, GPIO, 3,3 V ; aujourd'hui IR à droite, OLED 128×64 à gauche), batterie Li-ion,
un secteur de flash de 4 Ko pour les réglages.

Légende de faisabilité :

| Note | Signification |
|---|---|
| **Facile** | quelques jours, réutilise l'existant (menus, SD, radio, LEDs) |
| **Moyen** | une à trois semaines, un nouveau protocole ou un moteur à écrire, des tests sur plusieurs badges |
| **Difficile** | gros chantier, limites matérielles serrées ou logistique lourde le jour J |


## 1. Ce qui rend un badge mémorable

Les badges dont on parle encore des années après (DEF CON 14 et 25, Card10 du CCC, Supercon, les badges « shitty add-on »)
ont presque toujours ces ingrédients :

1. **Social** : le badge donne une raison d'aller parler à un inconnu. Le meilleur badge est un brise-glace.
2. **Collectionnable** : des choses à obtenir qu'on ne peut pas avoir seul (rencontres, fragments, succès, badges « rares »).
3. **Hackable** : le code est ouvert, les ports sont exposés, le firmware se dumpe, et c'est **voulu**. Casser le badge fait partie du jeu.
4. **Surprise** : des choses qui arrivent sans prévenir (un événement à 17 h 00 pile, un easter egg, un badge qui se met à chanter en chœur avec les autres).
5. **Persistance** : le badge garde une trace de l'événement (souvenirs, rencontres, score) et reste utile ou joli **après** :
   l'e-Paper qui garde son image sans courant est un atout énorme, la cigale peut rester sur un bureau comme cadre photo.
6. **Ancrage local** : un badge qui ne pourrait exister qu'ici. La Ciotat, c'est le cinéma des frères Lumière,
   la pétanque (inventée ici en 1907), les chantiers navals, le Bec de l'Aigle, l'île Verte, les calanques, le mistral et... les cigales.

Chaque idée ci-dessous essaie de cocher au moins deux de ces cases.


## 2. Social et multi-badges

On suppose que la couche « balise + rencontre + identité » du document réseau existe. Tout ce qui suit s'appuie dessus.

### S1. Le chœur des cigales
Quand plusieurs cigales sont proches, elles se synchronisent (horloge radio commune) et chantent **ensemble** une polyphonie :
chaque badge joue une voix différente selon son rang, les LEDs pulsent en rythme. Plus il y a de cigales, plus le morceau est riche.
Un moment « wow » garanti dans la salle ou au bar.
- **Moyen** : synchronisation par un paquet « top départ » (la latence radio de quelques ms suffit), son faible du buzzer compensé par le nombre.

### S2. Infection virale (épidémie éthique)
Un « virus » (inoffensif !) se propage de cigale en cigale par proximité : patient zéro choisi par l'orga, contagion au RSSI,
symptômes visibles (la cigale tousse sur l'écran, LEDs vertes). Des « vaccins » se gagnent en résolvant des énigmes.
La station de base affiche la courbe épidémique en direct : super support pour un talk sur la propagation des vers informatiques.
- **Moyen** : réutilise la balise ; il faut un état « infecté » signé pour éviter la triche facile (ou l'assumer comme challenge).

### S3. Messagerie maillée (mesh) à faible débit
Des messages courts (40 caractères, emojis 16×16) relayés de badge en badge jusqu'au destinataire, avec un TTL.
On voit sur l'écran par combien de cigales le message est passé. Saisie avec les 4 boutons via des phrases prédéfinies
et un clavier en « roue » (voir U8), ou depuis l'application PC.
- **Moyen** : routage par inondation contrôlée (TTL 3-4, identifiants de messages déjà vus), attention au rapport cyclique radio.

### S4. Votes du public en direct
L'orateur pose une question, chaque cigale vote avec les ailes (A/B/C/D), la station de base affiche le résultat en temps réel.
Utilisable aussi pour élire le meilleur talk, le meilleur costume, le prochain morceau du DJ.
- **Facile** : un paquet de 8 octets par vote ; un vote par badge et par question (identifiant de question dans le paquet).

### S5. Chasse aux trésors radio collective
Des fragments d'une carte ou d'une phrase sont répartis entre les badges au démarrage : il faut rencontrer les bonnes personnes
pour compléter sa collection (comme des vignettes Panini). Échange de doubles possible par une « poignée d'ailes ».
- **Moyen** : distribution des fragments par hash de l'identifiant, échange par la poignée de main existante.

### S6. Équipes et territoires
Au démarrage, chaque cigale est affectée à une équipe (Calanques, Bec de l'Aigle, Île Verte, Lumière).
Les balises fixes du lieu (voir C1) sont « capturées » par l'équipe qui a le plus de membres à proximité.
Tableau des territoires sur l'écran de la salle.
- **Moyen** : nécessite des balises fixes et la station de base.

### S7. Duel de cigales
Deux badges proches peuvent se défier (pierre-feuille-ciseaux, bataille navale de La Ciotat, quiz sécurité) en radio.
Le perdant perd... une plume virtuelle. Rapide, drôle, parfait pour la file d'attente du café.
- **Facile à moyen** : jeux au tour par tour, parfaits pour l'e-Paper lent ; protocole requête/réponse simple.

### S8. Carte de visite échangée
Une poignée d'ailes (les deux personnes appuient en même temps) échange pseudo, Mastodon, site, clé PGP courte (empreinte).
Après l'événement, l'application PC exporte les contacts en vCard.
- **Facile** : la rencontre existe déjà, il suffit d'un champ « contact » ; export via USB. Respect du consentement : échange **explicite** uniquement.

### S9. Radar de cigales (« qui est là ? »)
Liste des cigales entendues avec leur RSSI sous forme de barres, et un mode « chaud / froid » pour retrouver un ami
(les LEDs virent au rouge quand on s'approche). Utile aussi pour retrouver un badge perdu.
- **Facile** : RSSI déjà mesuré ; seule la cible choisie est suivie, ne suivre que des amis ayant accepté (voir éthique).

### S10. Mur de pensées (graffiti radio)
Chaque cigale diffuse un « statut » d'une ligne (« cherche un binôme pour le CTF », « j'ai du café »).
En se promenant, on voit les statuts des gens autour de soi, comme un mur d'expression anonyme de proximité.
- **Facile** : champ texte dans la balise (émis moins souvent), modération : liste de mots filtrés et bouton « masquer ».

### S11. Le relais de la flamme
Un « jeton » unique circule : celui qui l'a doit le passer à quelqu'un qu'il n'a jamais rencontré dans les 10 minutes,
sinon il s'éteint. La station affiche le trajet du jeton. Crée des interactions en chaîne très drôles.
- **Moyen** : jeton signé par l'orga, transfert par poignée de main, preuve de passage stockée.

### S12. Rencontres « orateur » et « staff »
Les badges orateurs émettent un type spécial ; les rencontrer débloque une carte à collectionner (portrait pixel art
de l'orateur + résumé de son talk) stockée sur la SD.
- **Facile** : les images sont préparées à l'avance sur la SD, la rencontre débloque juste un indice.


## 3. Sécurité et hacking pédagogique

L'objectif : que chaque participant reparte en ayant **compris** quelque chose de concret sur la radio, le matériel ou la crypto.

### H1. Analyseur de spectre 433 MHz
Balayage de 433,05 à 434,79 MHz (et plus large en réception seule, 300-928 MHz selon les bandes du CC1101),
affichage d'une courbe RSSI ou d'une « cascade » (waterfall) qui défile ligne par ligne. Idéal sur l'e-Paper :
une ligne par balayage en rafraîchissement partiel, et l'OLED pour la vue instantanée.
- **Moyen** : réception uniquement, donc sans contrainte légale ; balayage ~100 canaux en ~0,5 s.

### H2. Sniffer OOK/ASK et décodeur de protocoles
Capture brute des impulsions (via PIO, horodatage à la µs), affichage de la forme d'onde sur l'écran,
décodage des protocoles courants (Princeton PT2262, sondes météo, sonnettes) façon `rtl_433` simplifié.
Export des captures au format `.sub` du Flipper Zero sur la SD.
- **Moyen à difficile** : le PIO est parfait pour la capture ; les décodeurs se portent un par un. Voir l'éthique sur le rejeu.

### H3. Atelier « pourquoi les codes fixes sont faibles »
Une cible fournie par l'orga (une sonnette ou une prise télécommandée de l'atelier, à code fixe) sert de démonstration :
le badge montre la trame reçue, explique qu'elle est toujours identique, puis compare avec un code tournant (rolling code)
et ce qui le protège. On repart en sachant choisir ses équipements.
- **Moyen** : uniquement sur le matériel de l'atelier, en bande ISM, à faible puissance (voir la section éthique).

### H4. Défis de cryptographie sur l'écran
Une série d'énigmes progressives : César, Vigenère, XOR, hash tronqué, signature à vérifier... L'écran affiche le chiffré,
les ailes font défiler les indices, la réponse se saisit avec la roue de caractères (celle du nom de la cigale).
Chaque défi réussi donne un fragment du grand flag final.
- **Facile** : du texte et un peu de calcul ; les solutions sont stockées sous forme de hash dans le firmware.

### H5. Le badge « outil de talk »
L'orateur diffuse en radio le numéro de la diapositive, un lien ou un petit quiz : les cigales de la salle affichent
un résumé ou un QR code à la fin du talk. Et un mode « minuteur » pour l'orateur (temps restant en grand, LEDs qui passent à l'orange puis au rouge).
- **Facile** : un paquet radio ; le QR code demande un petit générateur (voir C4).

### H6. Hacking matériel du badge lui-même
Les ports SWD et d'extension sont documentés : un défi consiste à lire la flash, trouver un flag caché dans le firmware,
ou brancher un analyseur logique sur le SPI de l'écran pour « voir » une image transmise.
Documenter la démarche sur la SD (`docs/`) en fait une vraie initiation au reverse engineering.
- **Facile** : c'est surtout de la documentation et un flag bien caché.

### H7. Station météo et capteurs radio du lieu
Réception seule des sondes 433 MHz présentes (thermomètres de la salle installés par l'orga) : température,
humidité, et le « mistral-mètre » du jour sur l'écran de veille.
- **Moyen** : décodeurs des sondes courantes, réception uniquement.


## 4. CTF et quêtes pendant la conférence

### C1. Balises cachées dans le lieu
Des petites balises radio (un badge sans écran suffit) cachées dans le bâtiment. Le mode « chasse » de la cigale
affiche le RSSI en « chaud / froid » ; trouver une balise donne un succès et une énigme. Quelques balises ne s'allument
qu'à certaines heures.
- **Moyen** : logistique des balises le jour J ; le code est celui du radar (S9).

### C2. Succès (achievements)
Une page « Succès » : 30 badges à débloquer (rencontrer 10 cigales, finir Simon niveau 15, lire un texte à 600 mots/min,
trouver le code Konami, voir la veille à minuit...). Chaque succès allume une icône pixel art. Très collectionnable.
- **Facile** : un champ de bits dans le stockage flash, les événements existent déjà.

### C3. Quête narrative
Une histoire en chapitres (la cigale a perdu sa chanson dans les calanques...) : chaque chapitre se débloque par un
défi, une rencontre ou une balise, et se lit sur l'écran comme un livre dont vous êtes le héros.
- **Moyen** : surtout de l'écriture ; le moteur est un lecteur de texte avec choix.

### C4. QR codes
Générer un QR code sur l'écran : contact, flag, lien vers un talk. L'e-Paper 200×200 est parfait pour un QR version 3-5.
- **Moyen** : un générateur QR en C (quelques Ko), sans dépendance.


## 5. Création et personnalisation

| Idée | Description | Faisabilité |
|---|---|---|
| **P1. Éditeur de pixel art** | Dessiner une image 25×25 agrandie (les flancs déplacent, les ailes changent la couleur), en faire son image de veille | Moyen |
| **P2. Badge nominatif** | Grand pseudo en police large, sous-titre (« Orateur », « Staff »), QR de contact : l'écran e-Paper reste affiché même éteint | Facile |
| **P3. Galerie souvenir** | Les photos de l'événement copiées sur la SD à la sortie, la cigale devient un cadre photo qui change d'image chaque jour | Facile |
| **P4. SSTV** | Recevoir une image transmise en son (SSTV) par l'orga... ou l'afficher depuis le PC, clin d'œil radioamateur | Difficile |
| **P5. Thèmes** | Polices, cadres et sons de menu au choix (Provence, rétro 8 bits, terminal vert) | Facile |


## 6. Jeux adaptés à l'e-Paper

| Idée | Description | Faisabilité |
|---|---|---|
| **J1. Tamagotchi cigale** | La cigale grandit, mue et chante selon vos rencontres, vos jeux, l'heure. Négligée, elle s'endort. Le cœur émotionnel du badge | Moyen |
| **J2. Pétanque** | Clin d'œil à La Ciotat : jauge de force et d'effet avec les boutons, trajectoire dessinée, bouchon ; en duel radio avec un autre badge | Moyen |
| **J3. Roguelike des calanques** | Donjon au tour par tour en tuiles 8×8 : parfait pour un écran lent | Moyen |
| **J4. Démineur, 2048, taquin, Sokoban** | Grands classiques au tour par tour, jouables avec 4 boutons | Facile |
| **J5. Quiz sécurité** | Questions de culture sécurité, seul ou en duel radio | Facile |
| **J6. Bataille navale** | En radio entre deux badges, en souvenir des chantiers navals | Moyen |
| **J7. L'arrivée d'un train** | Mini-jeu hommage aux frères Lumière : garder le train à l'heure en gare de La Ciotat | Facile |


## 7. Utilitaires

- **U1. Programme de la conférence** : planning sur la SD, « prochain talk » en page d'accueil, rappel par les LEDs 5 min avant. *Facile.*
- **U2. Minuteur / chronomètre** : pour les orateurs et les ateliers. *Facile.*
- **U3. Horloge** : l'heure, réglée par l'application PC ou par une balise radio de l'orga, et la veille qui affiche l'heure. *Facile.*
- **U4. Veilleuse** : les 2 LEDs en blanc, pour se repérer dans un couloir sombre (pas une vraie lampe de poche). *Facile.*
- **U5. Niveau à bulle / métronome / diapason** : le buzzer et les LEDs au service des musiciens. *Facile.*
- **U6. Mode « ne pas déranger »** : coupe la radio et le son d'un appui long. *Facile.*
- **U7. Mise à jour depuis la SD** : copier un `.uf2` sur la carte et l'installer sans ordinateur. *Difficile (bootloader).*
- **U8. Clavier en roue** : un composant de saisie commun (lettres, emojis, phrases favorites) pour tous les modes. *Moyen.*


## 8. Musique et son

- **M1. Instrument** : les 4 boutons jouent des notes, les flancs changent d'octave ; enregistrer et rejouer une boucle. *Facile.*
- **M2. Boîte à rythmes** : séquenceur 16 pas affiché sur l'écran. *Moyen.*
- **M3. Karaoké de la cigale** : paroles synchronisées sur une musique de la SD. *Moyen.*
- **M4. Orchestre de cigales** : la version musicale du chœur (S1), chaque badge une partition. *Moyen.*


## 9. Extensions et Flipper Zero

- **E1. Accéléromètre (I2C)** : secouer pour relancer, inclinaison pour les jeux, podomètre de la conférence. *Facile avec un module.*
- **E2. Capteur de lumière / température** : la cigale chante plus fort au soleil, comme une vraie. *Facile.*
- **E3. Vibreur** : notifications discrètes pendant les talks. *Facile.*
- **E4. Échanges avec le Flipper Zero** : le chat existe ; ajouter un fichier `.sub` d'exemple et une application Flipper qui affiche le score d'une cigale. *Moyen.*
- **E5. Module GPS ou horloge temps réel** : pour les balises horaires et la chasse. *Moyen.*
- **E6. « Shitty add-ons »** : publier le brochage des ports pour que chacun fabrique et échange ses modules. *Facile (documentation).*


## 10. Idées « wow » et easter eggs

- **W1. L'heure de la cigale** : à 17 h 00 pile, toutes les cigales chantent ensemble dans le bâtiment.
- **W2. Hommage aux frères Lumière** : une vidéo « L'arrivée d'un train en gare de La Ciotat » (1895, domaine public) sur la SD, et un easter egg qui la lance.
- **W3. Mode nuit** : après 22 h, la veille affiche un ciel étoilé et la cigale dort ; ses LEDs clignotent doucement au rythme d'une respiration.
- **W4. La cigale qui mue** : l'image de veille change avec le score social (larve, nymphe, cigale adulte, cigale dorée).
- **W5. Réveil des souvenirs** : un an après, le badge affiche « Il y a un an à SecSea... » avec vos statistiques.
- **W6. Code secret du mistral** : souffler sur le badge (capteur en extension) déclenche une animation de vent.


## 11. Top 10 conseillé

Classement par impact et effort raisonnable, dans l'ordre de réalisation proposé :

| # | Idée | Pourquoi |
|---|---|---|
| 1 | **C2. Succès** | Réutilise tous les événements existants, rend tout collectionnable, peu de code |
| 2 | **P2. Badge nominatif + C4. QR de contact** | Utile toute la conférence, l'e-Paper reste affiché sans courant |
| 3 | **U1. Programme de la conférence** | Utile à tous dès la première minute |
| 4 | **S8. Carte de visite échangée** | Le brise-glace parfait, consentement explicite |
| 5 | **S4. Votes du public** | Implique la salle pendant les talks, protocole simple |
| 6 | **J1. Tamagotchi cigale** | Lien émotionnel, relie social, jeux et temps |
| 7 | **W1. Le chœur de 17 h** | Moment collectif mémorable, facile à synchroniser |
| 8 | **H4. Défis de cryptographie** | Pédagogique, s'intègre au CTF existant |
| 9 | **H1. Analyseur de spectre** | Montre la radio de façon visuelle, réception uniquement |
| 10 | **J2. Pétanque en duel** | Ancrage local, drôle, parfait au tour par tour |


## 12. Éthique, légalité et vie privée

- **Bandes et puissances** : n'émettre qu'en bande ISM 433,05–434,79 MHz, dans les limites de la réglementation
  européenne des appareils à courte portée (10 mW PAR, rapport cyclique limité selon la sous-bande).
  Les balises du badge émettent à −20 dBm, loin de ces limites.
- **Jamais de brouillage** : aucune fonction ne doit empêcher d'autres équipements de communiquer.
  La porteuse de test s'arrête d'elle-même après 30 s.
- **Rejeu et capture** : uniquement sur les équipements de l'atelier ou les siens. Rejouer le signal d'un équipement
  qui ne vous appartient pas (portail, voiture, alarme) est illégal et contraire à l'esprit du hacking éthique.
- **Vie privée** : pas de traçage des personnes sans leur accord.
  - Le radar ne suit que des amis consentants.
  - Les identifiants radio sont des pseudonymes.
  - La station de base ne garde que des statistiques agrégées.
  - L'échange de contacts est toujours explicite.
- **Modération** : les textes diffusés (statuts, messages) passent un filtre, et chacun peut masquer un émetteur.
- **Transparence** : le code est ouvert ; chaque fonction radio indique sur l'écran ce qu'elle émet et quand.
