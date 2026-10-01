# Sonneries (lecteur RTTTL)

Menu **Médias > Sonneries** : le badge joue des sonneries au format RTTTL, celui des sonneries des vieux téléphones
Nokia. Une douzaine de mélodies du domaine public sont dans le badge (Lettre à Élise, Ode à la joie, Frère Jacques,
Au clair de la lune, La Marseillaise, Korobeiniki, Greensleeves...), et on peut en ajouter autant qu'on veut sur la
carte SD.

## Utilisation

- **La liste** : les sonneries du badge, puis celles de la carte SD, avec le nom du fichier entre parenthèses.
  Flancs : monter / descendre (maintenus : défilement), aile droite (D) : jouer, aile gauche (G) : retour.
  Une sonnerie marquée `(!)` contient une erreur : D affiche la ligne, la colonne et la raison.
- **Pendant la lecture** : le nom, la note jouée (ex. `La#5`, `Silence`), une barre de progression et le temps.
  Les LEDs s'allument à chaque note, d'une couleur par note (Do rouge, Ré orange, Mi vert clair, Sol cyan, La bleu...).
  G : arrêter, flancs : sonnerie précédente / suivante, D : reprendre au début.
- Le **mode muet** est respecté : pas de son ni de LEDs (la lecture continue en silence).
- Jouer une sonnerie débloque le succès **Mélomane**.

## Ajouter des sonneries sur la carte SD

Créer un dossier `SONNERIES` à la racine de la carte SD et y mettre des fichiers texte `.txt`, `.rtttl` ou `.rtx` :

```
carte SD
├── SONNERIES/
│   ├── classique.txt
│   └── exemples.rtttl
└── ...
```

- **une sonnerie par ligne** (un fichier peut en contenir plusieurs) ;
- les lignes vides et celles qui commencent par `#` sont ignorées (commentaires) ;
- texte en UTF-8 (les accents des noms s'affichent) ou ASCII ; lignes de 2048 caractères au plus ;
- 24 fichiers et 128 sonneries au plus.

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

Exemples :

```
# Une gamme avec les réglages par défaut
Gamme::c,d,e,f,g,a,b,c7
# Croche pointée, double croche, dièse, silence, blanche de l'octave 6
Exemple:d=8,o=5,b=100:c.,16d,f#,p,2g6
```

## Les erreurs

Une ligne invalide reste dans la liste, marquée `(!)` ; D affiche sa ligne, sa colonne (en octets depuis le début
de la ligne) et la raison : `':' manquant après le nom`, `':' manquant avant les notes`, `réglage d, o ou b invalide`,
`durée invalide`, `note invalide`, `octave invalide`, `',' attendue après la note`, `aucune note`,
`ligne trop longue`. Le port série (USB) affiche aussi les détails : lignes `rtttl: ...`.

## Pour les développeurs

- `src/menu/rtttl_parse.c/.h` : l'analyseur RTTTL, en C pur, et les sonneries du badge (`RTTTL_BUILTIN`) ;
  testé sur PC par `python src/tests/host/run_tests.py rtttl` (y compris les fichiers de `docs/sd/SONNERIES`).
- `src/menu/rtttl.c` : l'application. Le son est synthétisé (onde carrée, attaque, déclin, courte coupure entre
  les notes) et écrit ~150 ms en avance dans le tampon audio (`audio.h`) : la boucle principale n'attend jamais.
  La note affichée et les LEDs suivent `audio_played()` : ce qu'on entend, pas ce qui est écrit.
