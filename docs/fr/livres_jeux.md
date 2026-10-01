# Livres-jeux : les livres dont vous êtes le héros

Le badge contient des **livres-jeux** : une histoire découpée en sections numérotées, et à la fin de chaque section,
c'est vous qui choisissez la suite. Certains choix mènent à la victoire, d'autres à une fin moins glorieuse...

Un livre est livré avec le badge, **Le Trésor du capitaine Cigalon** (une cigale hackeuse, la Fourmi, un trésor dans
les calanques de La Ciotat), et vous pouvez en ajouter autant que vous voulez sur la carte SD, ou écrire les vôtres.

Menu : **Médias > Livres-jeux**.


## 1. Lire un livre

### 1.1 La liste des livres

- Le livre intégré au badge, puis les livres du dossier `LIVRES` de la carte SD (16 au plus).
- **Continuer : ...** en tête de liste : reprend le dernier livre là où vous l'aviez laissé.
- Flancs : choisir un livre, **D** : l'ouvrir, **G** : quitter.

### 1.2 Le menu d'un livre

Le titre, l'auteur, puis :

| Ligne | Effet |
|---|---|
| Commencer | Le début de l'histoire. |
| Reprendre la lecture | La section où vous en étiez. |
| Recommencer | Retour au début, sans les objets ramassés. |
| Objets | Les objets que vous avez sur vous (s'il y en a dans le livre). |
| Autres livres | Retour à la liste. |

### 1.3 La page de lecture

```
┌─────────────────────────────┐
│ 12   Le Trésor du ca...  1/3│  numéro de la section, titre, page
│   Le texte de la section,   │
│ coupé à la largeur de       │
│ l'écran...                  │
│                             │
│ > Ouvrir la porte           │  le choix sélectionné est en noir
│ > Faire demi-tour           │
├─────────────────────────────┤
│   D : choisir  G : menu     │
└─────────────────────────────┘
```

| Bouton | Effet |
|---|---|
| Flanc droit | Page suivante, puis choix suivant. |
| Flanc gauche | Choix précédent, puis page précédente. |
| D (aile droite) | Prendre le choix sélectionné (ou page suivante s'il n'y a pas encore de choix à l'écran). |
| G (aile gauche) | Le menu du livre. |
| G maintenu | Quitter les livres-jeux. |

- Quand le livre lance un **dé**, le résultat s'affiche (« Le dé roule... et donne 4. ») et seuls les choix
  correspondants sont proposés.
- Un **objet** gagné ou perdu est annoncé (« Vous obtenez : antenne. ») ; certains choix n'apparaissent que si vous
  avez (ou n'avez pas) un objet.
- À une **fin** : « ~ FIN ~ », gagné ou perdu, puis *Recommencer* ou *Autres livres*. Chaque fin atteinte compte pour
  les succès (**Héros** à la première fin).
- La progression (livre, section, objets) est **sauvegardée** à chaque section, même si le badge est éteint.
  Un seul livre à la fois : commencer un autre livre remplace la sauvegarde.


## 2. Ajouter des livres sur la carte SD

Copier les fichiers `.txt` dans le dossier `LIVRES` à la racine de la carte :

```
carte SD
└── LIVRES/
    ├── tresor_cigalon.txt
    └── mon_livre.txt
```

Le dépôt contient un exemple : `docs/sd/LIVRES/tresor_cigalon.txt` (le livre intégré au badge).

> Respectez les droits d'auteur : n'utilisez que vos propres textes ou des œuvres libres de droits.


## 3. Écrire un livre

Un livre est un simple fichier texte, écrit avec n'importe quel éditeur (Bloc-notes, VS Code...), de préférence en
**UTF-8** (le Windows-1252 des vieux éditeurs est aussi accepté).

### 3.1 Un exemple complet

```
La Cigale et le Code perdu
Une cigale anonyme
// Les lignes qui commencent par // sont des commentaires.

== 1
Vous arrivez devant la porte du local. Elle est fermée
par un digicode.

Une fourmi passe avec un trousseau de clés.

-> 2 : Demander la clé à la fourmi
-> 3 : Essayer 1234 sur le digicode

== 2
« Je ne prête pas », dit la fourmi. Mais elle laisse
tomber une clé en partant.
+[clé]
-> 4 [clé] : Ouvrir la porte avec la clé

== 3
Le digicode bipe. Trois fois. Puis l'alarme se déclenche.
-> 5 [dé 1-3] : Courir !
-> 6 [dé 4-6] : Rester calme

== 4 FIN gagné
La porte s'ouvre : bienvenue au hackerspace !

== 5 FIN perdu
Vous courez droit dans le vigile.

== 6
Le vigile vous reconnaît et vous laisse passer.
-> 4
```

### 3.2 Les règles

| Ligne | Sens |
|---|---|
| 1re ligne | Le **titre** du livre (affiché dans la liste). |
| 2e ligne | L'**auteur** (facultatif). Les lignes suivantes, jusqu'à la première section, sont ignorées. |
| `// texte` | Un commentaire, ignoré partout. |
| `== 12` | Le début de la **section 12** (de 1 à 65535). La **première section du fichier** est le début de l'histoire. |
| `== 12 FIN` | Une section qui est une **fin**. `== 12 FIN gagné` : une victoire, `== 12 FIN perdu` : une défaite. |
| texte | Le texte de la section. Les lignes qui se suivent forment un paragraphe ; une **ligne vide** commence un nouveau paragraphe ; une ligne qui commence par un tiret de dialogue (`—`, `–` ou `- `) va à la ligne. |
| `-> 34 : Ouvrir la porte` | Un **choix** qui mène à la section 34. Sans texte (`-> 34`), le choix s'appelle « Continuer ». |
| `-> 34 [clé] : ...` | Choix proposé seulement si l'on a l'objet `clé`. |
| `-> 34 [!clé] : ...` | Choix proposé seulement si l'on **n'a pas** l'objet `clé`. |
| `-> 34 [dé 1-3] : ...` | Le badge lance un dé à 6 faces en arrivant dans la section : ce choix n'est proposé que si le dé donne 1, 2 ou 3 (`[dé 6]` : seulement 6). Prévoyez un choix pour chaque valeur. |
| `+[clé]` | On **gagne** l'objet `clé` en arrivant dans la section (`-[clé]` : on le **perd**). Plusieurs sur une ligne : `+[clé] -[carte]`. |

Quelques détails :
- Les choix et les objets peuvent être placés n'importe où dans la section ; le badge affiche toujours le texte, puis
  les objets et le dé, puis les choix.
- Une section **sans aucun choix** est une fin (mieux vaut l'écrire avec `FIN`).
- Les noms d'objets ne tiennent pas compte des majuscules (`Clé` = `clé`). Un livre a **8 objets au plus**.
- Les fins de ligne Windows, Mac ou Linux sont acceptées, ainsi que l'en-tête UTF-8 (BOM) du Bloc-notes.
- Les guillemets « », les apostrophes typographiques ’, les points de suspension … et les tirets — sont convertis
  pour les polices du badge (" ' ... -), de même que œ (oe). Les lettres accentuées du français sont affichées ;
  les émojis et les autres caractères deviennent « ? ».

### 3.3 Les limites du badge

Le badge ne charge jamais le livre entier en mémoire : il lit seulement la section affichée.

| Limite | Valeur |
|---|---|
| Sections par livre | 400 |
| Texte d'une section | 2 000 octets environ (≈ 1 900 caractères) ; au-delà, le texte est coupé par « ... » |
| Choix par section | 8 |
| Texte d'un choix | 95 octets |
| Objets par livre | 8 |
| Livres sur la carte | 16 |

Pour le confort de lecture, visez des sections de **2 ou 3 pages** (300 à 500 caractères) : une page de l'écran
contient 8 lignes d'environ 30 caractères.

### 3.4 Vérifier son livre

Avant de le copier sur la carte, vérifiez le livre avec le script du dépôt (Python 3) :

```bash
python tools/gamebook_check.py LIVRES/mon_livre.txt
python tools/gamebook_check.py LIVRES/mon_livre.txt --pages   # taille et nombre de pages de chaque section
python tools/gamebook_check.py LIVRES/mon_livre.txt --play    # y jouer dans le terminal
```

Il signale :
- **erreurs** : pas de section, une section en double, un choix vers une section qui n'existe pas, trop de sections,
  de choix ou d'objets ;
- **remarques** : des sections jamais atteintes, des sections d'où aucune fin n'est possible (boucle sans issue), un
  dé dont certaines valeurs n'ont pas de choix, un objet testé mais jamais donné, un texte trop long, des caractères
  absents des polices.

Sur le badge, une section manquante ne plante pas la lecture : une page « La section 34 est introuvable » propose de
revenir en arrière ou de recommencer.

### 3.5 Changer le livre intégré au firmware

Le livre intégré est généré à partir du fichier d'exemple de la carte SD :

```bash
python tools/gamebook_check.py docs/sd/LIVRES/tresor_cigalon.txt --c src/menu/gamebook_builtin.c
```

Le test `python src/tests/host/run_tests.py gamebook` vérifie que les deux sont identiques, que tous les liens
sont valides et que des milliers de parties au hasard arrivent toutes à une fin.
