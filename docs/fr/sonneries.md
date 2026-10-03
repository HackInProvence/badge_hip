# Sonneries (lecteur RTTTL)

Menu **Médias > Sonneries** : le badge joue des sonneries au format RTTTL, celui des sonneries des vieux téléphones
Nokia. Une douzaine de mélodies du domaine public sont dans le badge (Lettre à Élise, Ode à la joie, Frère Jacques,
Au clair de la lune, La Marseillaise, Korobeiniki, Greensleeves...), et on peut en ajouter autant qu'on veut sur la
carte SD.

*English version: [ringtones.md](../en/ringtones.md).*

## Utilisation

- **La liste** : les dossiers `SONNERIES/` et `RTTTL/` de la carte SD, puis les sonneries du badge. Dans un dossier :
  ses sous-dossiers (`Nom/`), puis ses sonneries, avec le nom du fichier entre parenthèses. Les fichiers sont lus
  par pages de 32, dans l'ordre alphabétique : les lignes `< Précédents` et `Suivants >` changent de page (un dossier
  peut donc contenir des milliers de fichiers).
  Flancs : monter / descendre (maintenus : défilement), aile droite (D) : ouvrir le dossier ou jouer, aile gauche
  (G) : dossier parent, puis retour au menu.
  Une sonnerie marquée `(!)` contient une erreur : D affiche la ligne, la colonne et la raison.
- **Les réglages**, en bas de la liste principale : « Volume : 6/8 » (le volume du badge) et « LEDs : 25 % » (la
  luminosité des LEDs pendant la lecture : éteintes, 10, 25 (par défaut), 50 ou 100 %) ; D : valeur suivante.
- **Pendant la lecture** : le nom, la note jouée (ex. `La#5`, `Silence`), une barre de progression et le temps.
  Les LEDs s'allument à chaque note, d'une couleur par note (Do rouge, Ré orange, Mi vert clair, Sol cyan, La bleu...).
  G : arrêter, flancs : sonnerie précédente / suivante, D : reprendre au début.
- Le **mode muet** est respecté : pas de son ni de LEDs (la lecture continue en silence).
- Jouer une sonnerie débloque le succès **Mélomane**.

## Ajouter des sonneries sur la carte SD

Créer un dossier `SONNERIES` ou `RTTTL` (ou les deux) à la racine de la carte SD et y mettre des fichiers texte
`.txt`, `.rtttl` ou `.rtx` (format RTTTL) ou `.bas` (format PICAXE, voir plus bas), directement ou dans des
sous-dossiers :

```
carte SD
├── SONNERIES/
│   ├── classique.txt
│   └── exemples.rtttl
├── RTTTL/
│   ├── Films/
│   │   └── western.rtx
│   └── jeux.txt
└── ...
```

- **une sonnerie par ligne** (un fichier peut en contenir plusieurs) ;
- les lignes vides et celles qui commencent par `#` sont ignorées (commentaires) ;
- texte en UTF-8 (les accents des noms s'affichent) ou ASCII ; lignes de 2048 caractères au plus ;
- dans un dossier : 40 sous-dossiers montrés, des fichiers en nombre quelconque (par pages de 32), 128 sonneries
  par page au plus ;
- la page de lecture montre le chemin complet du fichier ; les flancs passent aux sonneries de la même page.

Des exemples sont dans [`docs/sd/SONNERIES`](../sd/SONNERIES) : copier ce dossier sur la carte SD.

## Le format RTTTL

```
nom:réglages:notes
Ode à la joie:d=4,o=5,b=120:e,e,f,g,g,f,e,d,c,c,d,e,e.,8d,2d
```

**Le nom** : tout ce qui précède le premier `:`.

**Les réglages** (séparés par des virgules, dans n'importe quel ordre, tous facultatifs) :

| Réglage | Sens | Valeurs | Par défaut |
|---|---|---|---|
| `d` | durée des notes sans durée | 1 (ronde), 2 (blanche), 4 (noire), 8, 16, 32 (64 accepté) | 4 |
| `o` | octave des notes sans octave | 4 à 7 (3 et 8 acceptés) | 6 |
| `b` | tempo, en noires par minute | 1 à 999 | 63 |

D'autres réglages (`l=` par exemple) sont ignorés. `nom::notes` (sans réglages) est valide.

**Les notes**, séparées par des virgules : `[durée] note [#] [.] [octave] [.]`

- durée : 1, 2, 4, 8, 16, 32 (sinon celle de `d`) ;
- note : `c d e f g a b` (do ré mi fa sol la si ; `h` = si, à l'allemande), `p` pour un silence ;
- `#` : dièse (`c#` = do dièse ; pas de bémol en RTTTL : écrire `a#` pour si bémol) ;
- `.` : note pointée (durée × 1,5), avant ou après l'octave (`4c.6` et `4c6.` sont pareils) ; deux points : × 1,75 ;
- octave : chiffre de 4 à 7 (sinon celle de `o`). `a4` = 440 Hz, `a5` = 880 Hz.

Majuscules et minuscules sont équivalentes, les espaces sont permis entre les éléments.

Variantes de certains convertisseurs, acceptées aussi : `_` pour le dièse (`f_5` = `f#5`), le dièse ou le point
avant la note (`8#d4`, `8.c6`), une partie vide entre le nom et les réglages (`Nom: :d=4,o=5,b=112:...`).

Exemples :

```
# Une gamme avec les réglages par défaut
Gamme::c,d,e,f,g,a,b,c7
# Croche pointée, double croche, dièse, silence, blanche de l'octave 6
Exemple:d=8,o=5,b=100:c.,16d,f#,p,2g6
```

## Le format PICAXE (.bas)

Les fichiers `.bas` sont des programmes BASIC des microcontrôleurs PICAXE ; le badge y lit les commandes `tune`
(générées par le « Tune Wizard » du PICAXE), chacune nommée par le commentaire `'` qui la précède (sinon par le nom
du fichier). Les autres lignes sont ignorées.

```
'Jingle Bells
tune 0, 2,($EB,$EB,$EB,$EC,$EB,$EB,$EB,$EC,$EB,$C2,$E7,$E9,$AB)
```

`tune broche, vitesse, [masque des LEDs,] (notes)` : la vitesse va de 1 à 15 (une noire dure vitesse × 73,84 ms) ;
chaque note est un octet (`$` hexadécimal, `%` binaire ou décimal) :

| Bits | Sens |
|---|---|
| 7-6 | durée : `00` noire, `01` croche, `10` ronde, `11` blanche |
| 5-4 | octave : `00` du milieu (do = 523 Hz), `01` haute, `10` basse |
| 3-0 | note : 0 = do ... 11 = si, 12 à 15 = silence |

Le badge convertit la commande en RTTTL (`'Jingle Bells` → `Jingle Bells:d=4,o=5,b=406:2b4,2b4,...`). Pas de notes
pointées ni de doubles croches dans ce format.

## Trier une collection

[`tools/rtttl_sort.py`](../../tools/rtttl_sort.py) range une grande collection de sonneries (des milliers de
fichiers) pour le badge : il lit les fichiers comme le badge, retire les doublons (même mélodie, même transposée ou à
un autre tempo), écarte les sonneries invalides, écrit un fichier par sonnerie nommé d'après son meilleur titre et
les classe en `Dessins animés`, `Génériques de séries`, `Musiques de films` et `Autre` (découpé par initiale) selon
un fichier de catégories `chemin#ligne<TAB>D|S|F|A`. Un `rapport.tsv` liste chaque fichier écrit, ses copies dans la
source, les erreurs et les fichiers vides.

```
python tools/rtttl_sort.py F:/RTTTL_origine F:/RTTTL --categories categories.tsv
```

## Les erreurs

Une ligne invalide reste dans la liste, marquée `(!)` ; D affiche sa ligne, sa colonne (en octets depuis le début
de la ligne) et la raison : `':' manquant après le nom`, `':' manquant avant les notes`, `réglage d, o ou b invalide`,
`durée invalide`, `note invalide`, `octave invalide`, `',' attendue après la note`, `aucune note`,
`ligne trop longue`, `commande tune PICAXE invalide`. Le port série (USB) affiche aussi les détails : lignes `rtttl: ...`.

## Pour les développeurs

- `src/menu/rtttl_parse.c/.h` : l'analyseur RTTTL, en C pur, et les sonneries du badge (`RTTTL_BUILTIN`) ;
  testé sur PC par `python src/tests/host/run_tests.py rtttl` (y compris les fichiers de `docs/sd/SONNERIES`).
- `src/menu/rtttl.c` : l'application. Le son est synthétisé (onde carrée, attaque, déclin, courte coupure entre
  les notes) et écrit ~150 ms en avance dans le tampon audio (`audio.h`) : la boucle principale n'attend jamais.
  La note affichée et les LEDs suivent `audio_played()` : ce qu'on entend, pas ce qui est écrit.
