# Contrebande — la cigale contrebandière

*Social > Contrebande* : des marchandises virtuelles rares, à collectionner et à échanger **en douce** entre deux
badges tenus l'un contre l'autre. Thème : la contrebande pirate en Provence (vivres, rhum, épices, trésors), avec
trois marchandises légendaires.

*English version: [smuggler.md](../en/smuggler.md).*

## 1. Pour le joueur

### Les pages

| Page | Contenu | Boutons |
|---|---|---|
| Accueil | Cale (nombre), Échanger en douce, Donner, Collection (x / 26), Fortune (doublons) | flancs : choisir, D : ouvrir, G : retour |
| Cale | Grille des icônes possédées, avec leur nombre ; sous la grille : nom, rareté, valeur | flancs : choisir, D : détails, G : retour |
| Collection | Toutes les marchandises ; celles jamais possédées en silhouette grise avec « ? » | idem |
| Détails | Grande icône, rareté, valeur, nombre en cale, une petite histoire | G : retour |
| Fortune | Valeur totale en doublons, rang (Mousse, Matelot, Contrebandier, Capitaine, Roi de la contrebande), marchandises, différentes, affaires conclues | G : retour |
| À portée de main | Les cigales assez proches pour échanger (nom, RSSI) | flancs : choisir, D : proposer, G : retour |
| Échange (page sombre) | Proposition, choix de l'offre, les deux offres côte à côte, conclusion | voir ci-dessous |

Un appui long sur G quitte la page (une affaire en cours est annulée, sauf si votre marchandise est déjà scellée).

### Obtenir des marchandises

- **Première ouverture** : la cale reçoit 4 marchandises communes au hasard (`cargo_seeded`).
- **Rencontres** : à chaque nouvelle cigale rencontrée (le compteur du réseau des cigales augmente), une chance sur
  deux de trouver une marchandise « dans la cale », tirée selon sa rareté : commun 75 %, rare 22 %, légendaire 3 %.
  L'annonce est discrète : un message en bas de l'écran, sans son.
- **Échanges et cadeaux** avec les autres cigales.

### Échanger en douce

1. Accueil > *Échanger en douce* : la liste des cigales **à portée de main** (badges collés, voir § 3).
2. D sur une cigale : elle reçoit « Psst... *nom* propose une affaire en douce » ; D accepte, G refuse.
3. Chacun choisit dans sa cale la marchandise qu'il offre (grille sombre, D : offrir).
4. Les deux badges montrent les deux offres (« Votre X contre Y ») : D conclut, G annule.
5. Quand les deux ont conclu, l'échange se fait sur les deux badges : « Affaire conclue », avec la marchandise reçue.

Celui qui a été invité ne peut plus annuler après avoir conclu : sa marchandise est **scellée** (sortie de sa cale)
jusqu'à la décision de l'autre. S'il quitte la page, l'affaire se termine en arrière-plan, et un message discret
annonce la marchandise reçue (« Reçu en douce : ... »).

### Donner

Accueil > *Donner* : choisir la marchandise, puis la cigale à portée de main. Elle voit « *nom* vous offre : » avec
l'icône, D accepte (la marchandise passe d'une cale à l'autre), G refuse.

### Admin : ajouter une marchandise

Admin > Contrebande (admin) : les 26 marchandises une à une (flancs), avec leur rareté et le nombre dans la cale ;
l'aile droite en ajoute une à la cale de ce badge (enregistrée tout de suite), avec les succès d'une vraie
acquisition (« Trésor » pour une légendaire, « Collectionneur »). Pour débloquer une marchandise rare, la mettre en
jeu, ou préparer un badge de démonstration. Trace : `smuggler: admin added <nom> (<nombre>)`.

### Succès

| Succès | Quand |
|---|---|
| Contrebandier | une affaire conclue (échange ou cadeau, donné ou reçu) ; chaque affaire ajoute 1 au compteur `ACHV_CNT_TRADES` |
| Trésor | une marchandise légendaire obtenue (échange, cadeau ou trouvée dans la cale) |
| Collectionneur | toutes les marchandises possédées au moins une fois (la Collection est complète) |

## 2. Les marchandises

26 marchandises (`smuggler_goods.c`), au plus `STORE_CARGO_ITEMS` = 32, chacune avec une icône 32 × 32 dessinée en
ASCII art dans [tools/smuggler_icons.py](../../tools/smuggler_icons.py).

| Rareté | Marchandises (valeur en doublons) |
|---|---|
| Commun | Biscuit de mer (1), Fromage (3), Poisson salé (2), Citrons (2), Olives (2), Figues (3), Navettes (3), Bouteille de rhum (5), Pastis (4), Saucisson (3), Lavande (2), Savon de Marseille (3) |
| Rare | Tonneau de rhum (20), Calissons (12), Safran (30), Poivre (15), Vanille (20), Boussole (18), Longue-vue (20), Carte au trésor (35), Clé du coffre (25), Perle noire (40), Bourse de doublons (30) |
| Légendaire | La cigale d'or (250), Le crâne de cristal (180), Le perroquet savant (150) |

Modifier une icône : éditer l'ASCII art (`#` = noir, `.` = blanc), puis

    python tools/smuggler_icons.py              # régénère les tableaux C dans src/menu/smuggler_goods.c
    python tools/smuggler_icons.py --png a.png  # planche de contrôle (zoom 4)

Le test hôte `smuggler` vérifie que `smuggler_goods.c` est à jour (`--check`).
Ajouter une marchandise : une icône dans l'outil, une ligne dans le tableau de `smuggler_goods.c`, et
`SMUGGLER_GOODS` (au plus 32) ; l'ordre du tableau est l'index enregistré dans la cale : ne jamais réordonner ni
retirer une marchandise une fois les badges distribués (ajouter à la fin).

**La cale** : `store_t.cargo[32]`, un octet par marchandise : bits 0-6 = nombre (99 au plus), bit 7 = déjà possédée
(la Collection). `cargo_seeded` = 1 après la première ouverture. Ces champs sont remis à 0 quand le store est
initialisé (`v2_magic`).

## 3. « À portée de main » : le seuil de RSSI

On n'échange qu'avec une cigale dont les balises (+10 dBm) arrivent avec un RSSI ≥ `SMUGGLER_TRADE_RSSI`
(**-55 dBm** par défaut, dans `smuggler.c`, modifiable à la compilation : `-DSMUGGLER_TRADE_RSSI=-60`).
Pour mémoire, à 1 m le RSSI est de -70 à -83 dBm (social.h) : -55 dBm correspond à des badges collés ou à quelques
centimètres. **À calibrer sur place** : *Social > Radar des cigales* affiche le RSSI ; la page *À portée de main*
aussi, et la liaison série affiche `smuggler: at hand <nom>#<id> <rssi> dBm`.

Une invitation reçue est acceptée jusqu'à `SMUGGLER_TRADE_RSSI - 10` dBm (`SMUGGLER_RSSI_MARGIN`), car le RSSI varie
d'un paquet à l'autre. Une fois l'affaire commencée, le RSSI n'est plus vérifié.

## 4. Le protocole radio (NET_TRADE = 0x07)

Code : [smuggler_trade.c](../../src/menu/smuggler_trade.c) (logique pure, sans SDK, testée sur PC) ;
[smuggler.c](../../src/menu/smuggler.c) branche la radio, la page et les succès.

Paquet (après l'en-tête du réseau) : `[id de l'affaire 4][type][destinataire 4][données]`, envoyé à +10 dBm.
L'id est tiré au hasard par celui qui invite.

| Type | Données | Sens |
|---|---|---|
| INVITE (1) | mode (0 échange, 1 cadeau), marchandise du cadeau, nom (8) | invitant → invité, répété toutes les 500 ms |
| ACCEPT (2) | — | invité → invitant (répété pendant le choix de l'offre) |
| REFUSE (3) | — | invité → invitant |
| ALIVE (4) | — | invitant, pendant le choix de son offre |
| OFFER (5) | marchandise | les deux, répété |
| CONFIRM (6) | ma marchandise, la sienne | les deux, répété |
| DONE (7) | marchandise de l'invitant, de l'invité | invitant → invité, répété jusqu'à ACK (20 s au plus) |
| ACK (8) | — | invité → invitant |
| ABORT (9) | — | les deux |

### Validation en deux phases

L'**invitant décide** :

1. Quand l'invité conclut, sa marchandise sort de sa cale (**scellée**) et il envoie CONFIRM jusqu'à connaître la
   décision. Il ne peut plus annuler.
2. L'invitant, quand il a conclu **et** reçu le CONFIRM de l'invité (avec les mêmes offres), **valide** : sa cale
   change d'un coup (il donne la sienne, reçoit celle de l'invité), l'affaire est notée « conclue », il envoie DONE.
3. L'invité reçoit DONE : il reçoit la marchandise de l'invitant (la sienne est déjà partie), note l'affaire, répond
   ACK.
4. Tant qu'il n'a pas validé, l'invitant peut annuler (bouton, ou silence de l'autre pendant 20 s) : l'affaire est
   notée « annulée », il envoie ABORT ; l'invité scellé récupère alors sa marchandise.

**Paquets perdus ou en double** : chaque badge garde les 16 dernières affaires décidées (id, pair, issue,
marchandises) et y répond toujours pareil : un CONFIRM répété après la validation reçoit DONE, un DONE répété reçoit
ACK (et n'est appliqué qu'une fois), tout paquet d'une affaire annulée reçoit ABORT. Les offres ne changent jamais
pendant une affaire. Le cadeau suit le même chemin : l'invité accepte avec CONFIRM (il ne donne rien), l'invitant
valide et envoie DONE.

**Séparation** : l'invité scellé n'abandonne jamais seul ; si l'invitant s'éloigne, il redemande toutes les 3 s, en
arrière-plan (même page fermée), et l'affaire se termine quand les badges se retrouvent.

**Pire cas** : l'invitant est éteint (ou a décidé 16 autres affaires depuis) avant que l'invité connaisse la
décision, puis l'invité est éteint aussi : la marchandise scellée est **perdue**, jamais dupliquée.
La cale est écrite en flash **tout de suite** à chaque changement dû à une affaire (`store_save_now()`, store.h) :
un badge éteint juste après un échange ne retrouve pas sa cale d'avant (ce qui pourrait dupliquer une marchandise).
Les marchandises de la première ouverture et les trouvailles des rencontres sont écrites, elles, 5 s plus tard
(`store_changed()`) : un badge éteint dans ces 5 s les perd.

Le test hôte simule 3000 affaires sur une radio qui perd jusqu'à 70 % des paquets, en double jusqu'à 40 %, dans le
désordre, avec des annulations et des séparations : aucune marchandise n'est jamais créée, et à la fin chaque
affaire est soit faite sur les deux badges, soit sur aucun, sans perte.

### Journal (liaison série)

    smuggler: seeded the cargo: Figues, Olives, Pastis, Fromage
    smuggler: found Safran (rare) in the hold
    smuggler: page near
    smuggler: 1 at hand, 2 farther
    smuggler: at hand Marius#ABCD -48 dBm
    smuggler: inviting Marius#ABCD, trade 1A2B3C4D, exchange
    smuggler: invited by Fanny#5678, trade 1A2B3C4D, exchange, rssi -47
    smuggler: accepted trade 1A2B3C4D from Fanny
    smuggler: trade 1A2B3C4D accepted by Marius
    smuggler: offer Fromage / smuggler: peer offers Safran / smuggler: review Fromage <-> Safran
    smuggler: confirmed, Safran sealed          (invité)
    smuggler: Marius confirmed / smuggler: confirmed   (invitant)
    smuggler: done trade 1A2B3C4D inviter gave=1 got=14 (Fromage -> Safran)
    smuggler: done trade 1A2B3C4D guest gave=14 got=1 (Safran -> Fromage)
    smuggler: trade 1A2B3C4D acknowledged
    smuggler: trade ... cancelled | refused | cancelled by the peer | lost (peer silent)
    smuggler: Safran back in the cargo
    smuggler: error: ...

## 5. Fichiers et tests

| Fichier | Rôle |
|---|---|
| `src/menu/smuggler.c` | l'application (pages, radio, trouvailles, succès), `smuggler_init()`, `smuggler_task()`, `smuggler_invited()`, `smuggler_event()` |
| `src/menu/smuggler_goods.c/.h` | les marchandises, leurs icônes (générées), la cale |
| `src/menu/smuggler_trade.c/.h` | le protocole d'échange (logique pure) |
| `tools/smuggler_icons.py` | l'ASCII art des icônes, la génération du C, la planche PNG |
| `src/tests/host/test_smuggler.c` | test hôte : la cale, les tirages, le protocole sur une radio avec pertes |
| `tools/test_smuggler.py` | test à deux badges |

Tests :

    python src/tests/host/run_tests.py smuggler
    python tools/test_smuggler.py --ports COM9 COM11

Le test à deux badges (badges collés) ouvre *Social > Contrebande* sur les deux, puis enchaîne : un échange (offres
vues des deux côtés, marchandises croisées), un cadeau, un refus, et une annulation par l'invitant après que l'invité
a scellé sa marchandise (elle lui revient). Les captures d'écran sont dans `smuggler_<date>/A` et `/B`.
