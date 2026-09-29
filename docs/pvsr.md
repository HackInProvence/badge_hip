# Lecture rapide : PVSR (Présentation Visuelle Sérielle Rapide)

En anglais RSVP, *Rapid Serial Visual Presentation*. Menu « Lecture rapide (PVSR) » du firmware `badge_menu`,
code dans [src/menu/rsvp.c](../src/menu/rsvp.c).


## Le principe

Le texte est affiché **un mot à la fois, toujours au même endroit**.
En lecture normale, l'œil saute de mot en mot (saccades) et s'arrête sur chacun (fixations) ;
une partie du temps de lecture est perdue dans ces mouvements, et les retours en arrière (régressions) représentent 10 à 15 % des saccades.
En PVSR, l'œil reste immobile : c'est le texte qui défile.

La technique vient de la psychologie expérimentale (Mary Potter, années 1970-80) où elle sert à étudier la compréhension.
Elle a été popularisée pour la lecture sur petits écrans (montres, téléphones) par Spritz en 2014.


## Les règles appliquées sur le badge

### Point de reconnaissance optimal (ORP)

L'œil reconnaît un mot le plus vite en fixant une lettre **un peu à gauche du centre**.
Chaque mot est donc aligné sur cette lettre, repérée par les marques d'un réticule fixe et soulignée
(Spritz l'affiche en rouge, l'e-Paper n'a que du noir) :

| Lettres du mot | Lettre à fixer |
|---|---|
| 1 | 1re |
| 2 à 5 | 2e |
| 6 à 9 | 3e |
| 10 à 13 | 4e |
| 14 et plus | 5e |

Seules les lettres et les chiffres comptent (« J'ai » : 3 lettres, on fixe le « a »).

### Durée d'affichage

Durée de base : 60 000 / (mots par minute) ms, multipliée par :

| Cas | Facteur |
|---|---|
| Mot de plus de 8 lettres | × 1,2 |
| Avant une virgule, un point-virgule, deux-points | × 1,3 |
| Fin de phrase (`.` `!` `?`) | × 1,6 |
| Fin de paragraphe | × 2 |
| Premier mot après un démarrage, une reprise ou un saut | × 2 (le temps de se remettre dedans) |

Ces pauses rendent la lecture moins mécanique et laissent le temps d'intégrer la phrase.
L'écran e-Paper ne peut pas afficher un mot en moins de ~100 ms (c'est aussi la limite de la compréhension, ~600 mots/min).

### Mots longs

Les mots de plus de 13 caractères sont coupés en morceaux de taille égale avec un trait d'union
(« Anticons- / titutionne- / llement »), et la police est réduite si le mot ne tient pas dans la largeur.

### Typographie française

- La ponctuation séparée par une espace (`:` `;` `!` `?` `»`) est rattachée au mot précédent : on n'affiche jamais « ? » seul.
- Les guillemets ouvrants et les tirets de dialogue sont rattachés au mot suivant.
- Les caractères absents de la police sont remplacés : `’` → `'`, `« » “ ”` → `"`, `…` → `...`, `– —` → `-`, `œ` → `oe`.


## Utilisation

- Les textes sont des fichiers `.TXT` dans le dossier `TEXTES` de la carte SD (sous-dossiers possibles),
  en UTF-8 ou en Latin-1/Windows-1252 (détecté automatiquement). Leur taille n'a pas d'importance : ils sont lus mot à mot.
- Laisser une ligne vide entre les paragraphes (pause plus longue).

| Bouton | Appui court | Appui long (0,7 s, répété si maintenu) |
|---|---|---|
| Flanc gauche | Moins vite (-25 mots/min) | Recul de 10 s de lecture |
| Flanc droit | Plus vite (+25 mots/min) | Avance de 10 s de lecture |
| Aile droite | Pause / reprise (en fin de texte : relire) | |
| Aile gauche | Quitter (la position est mémorisée) | |

- Vitesse de 100 à 900 mots/min, 250 par défaut. Au-delà de 450 mots/min l'écran utilise une forme d'onde plus rapide mais moins contrastée.
- La vitesse et la position dans le dernier texte lu sont mémorisées (même après un redémarrage).
- « 10 s de lecture » = le nombre de mots lus en 10 s à la vitesse actuelle. En avant, c'est exact ;
  en arrière, c'est estimé à partir de la longueur moyenne des mots déjà lus.
- Sur le port série : `x` / `y` = appui court sur un flanc, `X` / `Y` = appui long, `b` = pause, `a` = quitter.


## Limites connues de la technique

- **La compréhension baisse quand la vitesse augmente.** Au-delà de la vitesse naturelle de lecture (~250 mots/min en moyenne),
  on retient moins bien : les études n'ont pas montré de « lecture rapide » sans perte de compréhension.
- **Pas de retour en arrière naturel** : en lecture normale, l'œil revient sur un passage mal compris ;
  en PVSR il faut le faire volontairement (appui long sur le flanc gauche).
- **Fatigue** : on ne cligne pas des yeux pendant la lecture, il faut faire des pauses.
- Adapté aux textes narratifs simples, moins aux textes techniques, aux tableaux ou à la poésie (la mise en page est perdue).

C'est donc un outil de démonstration et d'expérimentation (comparer sa compréhension à 200, 400, 600 mots/min)
plus qu'un moyen miracle de lire plus vite.


## Sources

- Rayner, Schotter, Masson, Potter, Treiman (2016), *So Much to Read, So Little Time: How Do We Read, and Can Speed Reading Help?*,
  Psychological Science in the Public Interest, 17(1) : https://journals.sagepub.com/doi/full/10.1177/1529100615623267
- Benedetto et al. (2015), *Rapid serial visual presentation in reading: The case of Spritz*, Computers in Human Behavior :
  https://www.sciencedirect.com/science/article/abs/pii/S0747563214007663
- Wikipédia, *Rapid serial visual presentation* : https://en.wikipedia.org/wiki/Rapid_serial_visual_presentation
- Implémentations libres consultées pour les règles (ORP, pauses) :
  https://github.com/StringManolo/RSVP, https://github.com/thomaskolmans/rsvp-reading,
  https://github.com/rajeshsatpathy1/SpeedReader/blob/main/RSVP_ALGORITHM.md
