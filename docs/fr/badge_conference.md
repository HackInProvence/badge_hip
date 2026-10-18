# Badge conférence

**Badge > Badge conférence** affiche le programme de n'importe quel événement, lu sur la carte SD : les infos, les
conférences, les workshops et les exposants. Le badge sert ainsi à d'autres conférences que SecSea, sans reflasher.

## Sur la carte SD

Un dossier `CONFERENCES` à la racine, avec un sous-dossier par événement :

```
CONFERENCES/
  SecSea2026/
    infos.txt
    conferences.txt
    workshops.txt
    exposants.txt
  LeHack2026/
    ...
```

Chaque fichier est facultatif : seuls ceux présents (et non vides) sont proposés. Exemples complets pour dix
événements français dans [docs/sd/CONFERENCES](../sd/CONFERENCES) (à copier tels quels sur la carte).

## Format des fichiers

Texte UTF-8, des **blocs** de lignes `clé: valeur` séparés par **une ligne vide**. Les lignes qui commencent par `#`
sont des commentaires. Une clé inconnue est ignorée, une clé absente n'est pas affichée.

| Fichier | Clés |
|---|---|
| `infos.txt` (un seul bloc) | `nom` (le nom dans la liste), `dates`, `lieu`, `adresse`, `site` (QR code), `description`, `source` (non affichée) |
| `conferences.txt`, `workshops.txt` | `jour` (AAAA-MM-JJ), `heure` (HH:MM), `fin`, `salle`, `titre`, `orateur`, `resume`, `lien` (QR code) |
| `exposants.txt` | `nom`, `stand`, `description`, `lien` (QR code) |

```
jour: 2026-10-16
heure: 14:00
fin: 14:45
salle: L'Eden
titre: 1 an de hack
orateur: Miaou
resume: Un an de projets ...
lien: https://secsea.org/
```

Sans `infos.txt` ni `nom`, l'événement prend le nom de son dossier. Les blocs sont montrés dans l'ordre du fichier :
classez-les par jour et heure.

## Limites

- 16 événements, 100 blocs par fichier (les suivants sont ignorés).
- 480 octets par ligne, 1 280 octets par bloc (le reste du bloc n'est pas affiché).
- Seuls les caractères des polices du badge s'affichent (ASCII, accents français et quelques autres).
- Un lien trop long pour un QR code affiche « (QR code trop grand) ».

## Sur le badge

- La liste des événements : Y / X choisir, B ouvrir, A retour.
- Un événement : Infos, Conférences (n), Workshops (n), Exposants (n).
- Une liste : le jour, l'heure et le titre (ou le nom de l'exposant) ; la ligne choisie défile si elle est longue.
- Une fiche : le titre, l'orateur, la salle, le résumé (qui défile ligne à ligne) ; Y / X la fiche précédente ou
  suivante, B le QR code du lien.
