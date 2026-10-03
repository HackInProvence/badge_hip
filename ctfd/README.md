# CTFd du badge SecSea

Un [CTFd](https://ctfd.io/) prêt à collecter les flags du badge. Les joueurs lisent un flag `SECSEA{...}` sur leur
badge (quand ils gagnent un jeu, battent un record, résolvent un défi…) et le saisissent sur le CTFd.

**Scoring statique (« sans décote »)** : chaque épreuve est de type *standard* (points fixes), les points ne
baissent pas quand beaucoup la résolvent.

## 1. Lancer CTFd

```bash
docker compose -f ctfd/docker-compose.yml up -d
```

Ouvrir http://localhost:8000 et faire l'assistant de première configuration (nom de l'événement, compte admin).
La base est persistée dans `ctfd/data/` (SQLite, suffisant pour un petit événement ; `ctfd/data/` et `ctfd/logs/`
sont ignorés par git).

## 2. Importer les épreuves et leurs flags

1. Dans CTFd : **Settings → Access Tokens → Generate** (copier le jeton, type `ctfd_...`).
2. Lancer l'import (aucun paquet Python à installer, `urllib` suffit) :

```bash
CTFD_URL=http://localhost:8000 CTFD_TOKEN=ctfd_xxx python ctfd/import_challenges.py
```

Le script crée une épreuve par flag (catégories *Jeux solo*, *Jeux multi*, *Boutons*, *Crypto*, *Morse*), chacune
avec son flag statique. Relancer le script est sans danger : les épreuves déjà présentes sont ignorées.

## 3. Source unique des flags

Tout vient de [`tools/ctf_flags.py`](../tools/ctf_flags.py), qui génère **à la fois** la table du firmware
(`src/menu/ctf.c`, flags obfusqués) **et** l'import CTFd — mêmes flags des deux côtés.

- Voir tous les flags (organisateurs) : `python tools/ctf_flags.py --answers`
- Voir le JSON envoyé à CTFd : `python tools/ctf_flags.py --ctfd`
- Après avoir modifié un flag : `python tools/ctf_flags.py --update` (met à jour `ctf.c`), recompiler le badge,
  puis réimporter dans un CTFd neuf (ou corriger le flag dans l'UI CTFd).

> ⚠️ `tools/ctf_flags.py` contient les flags en clair : c'est le fichier des organisateurs, à ne pas projeter.
> Les flags sont **statiques et identiques sur tous les badges** : le partage entre joueurs est inévitable et
> assumé (voir [docs/fr/ctf_plan.md](../docs/fr/ctf_plan.md)).
