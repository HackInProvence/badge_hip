# Traduction du badge

*English version: [translation](../en/translation.md).*

Le badge affiche ses textes en **français** (la langue du code) ou dans une autre langue : **anglais** pour
commencer. Le système est modulaire : ajouter une langue, c'est ajouter un fichier, sans toucher au code.

## Pour l'utilisateur

- **Choisir la langue** : Réglages > Langue. Chaque langue est écrite dans sa propre langue (« English »,
  « Français ») et le titre de la page est dans toutes (« Langue / Language ») : D choisit, G revient.
  Le choix est gardé (même après une mise à jour du firmware).
- **Retour à l'anglais** (badge resté dans une langue illisible) :
  1. appuyer **5 fois sur l'aile gauche (G)** : on revient au menu principal, quelle que soit la page ;
  2. puis les flancs en alternance, **gauche, droit, gauche, droit, gauche, droit, gauche, droit** (8 appuis en
     moins de 8 secondes, sans toucher aux ailes).
  Le badge passe en anglais et affiche « Language: English ».
- **Par le port série** (USB) : la touche `E` met le badge en anglais, `N` passe à la langue suivante.
- Les textes reçus d'autres badges (messages prédéfinis, annonces...) sont affichés dans la langue de chaque badge
  quand ils font partie des textes traduits ; les noms, les messages libres, les fichiers de la carte SD restent
  tels quels.

## Ce qui est traduit (et ce qui ne l'est pas)

Tous les textes de l'interface : menus, titres, pieds de page, messages, aides, cartes du loup-garou, succès,
compétences, erreurs. Restent en français, car ce sont des contenus dont la réponse est en français :
les mots du pendu, les énigmes à réponse tapée, les défis de cryptographie et les drapeaux du CTF, le livre-jeu
intégré. Les médias de la carte SD (livres, textes, sonneries) sont dans la langue où ils ont été écrits.

## Comment ça marche

- Dans le code (`src/menu`), les textes sont écrits en français et **marqués** :
  - `N_("Réglages")` : le texte tel quel (tables, libellés...). Il est traduit **au moment où il est dessiné** :
    `gfx_text()`, `gfx_text_width()` et les fonctions `ui_*()` cherchent la traduction de ce qu'elles dessinent.
    Un texte stocké ou envoyé par radio reste donc en français, et chaque badge l'affiche dans sa langue.
  - `_("Sonnerie %d / %d")` : traduit tout de suite, pour un **format** passé à `snprintf()` (le résultat formaté
    ne serait pas trouvé), ou un texte inséré par `%s` dans un tel format.
  - Les traces du port série (`printf`) ne sont pas traduites : les outils du PC les lisent.
- Les traductions sont dans `src/menu/lang/<code>.po` (format gettext : `msgid` = le français, `msgstr` = la
  traduction ; vide = pas traduit, le français est affiché).
- [`tools/i18n.py`](../../tools/i18n.py) extrait les textes marqués, met à jour les `.po` et génère
  `src/menu/i18n_table.c` : la table du firmware (hachage FNV-1a du texte français, recherche dichotomique, puis
  comparaison du texte). La langue choisie est gardée dans le store (`store_t.lang`).
- Les tests sur PC (`python src/tests/host/run_tests.py i18n`) vérifient que la table est à jour, que les formats
  (`%d`, `%s`...) de chaque traduction sont les mêmes que ceux du français, que tous les caractères existent dans les
  polices du badge, puis la recherche dans chaque langue.

## Ajouter ou modifier un texte

1. Écrire le texte en français dans le code, marqué `N_("...")` (ou `_("...")` pour un format).
2. `python tools/i18n.py update` : les nouveaux textes sont ajoutés aux `.po`, sans traduction.
3. Traduire les `msgstr` vides (`python tools/i18n.py check` les liste).
4. `python tools/i18n.py gen`, puis compiler le firmware.

## Ajouter une langue

1. Copier `src/menu/lang/en.po` en `src/menu/lang/<code>.po` (`de.po`, `es.po`...).
2. Dans son en-tête, mettre `Language: <code>` et `Language-Name: <nom dans la langue>` (« Deutsch »).
3. Traduire tous les `msgstr` (partir de l'anglais ou du français).
4. `python tools/i18n.py gen`, compiler : la langue apparaît dans Réglages > Langue.
5. **Les polices** n'ont que l'ASCII et les lettres accentuées du français (`à â ç é è ê ë î ï ô ù û ü À É È Ê Ç`) :
   `tools/i18n.py check` signale les caractères manquants. Pour une langue qui en demande d'autres (ä ö ß ñ...), les
   ajouter à `COMPOSED` (lettre de base + accent) dans [`src/gfx/gen_fonts.py`](../../src/gfx/gen_fonts.py) et à
   `FONT_EXTRA` dans `tools/i18n.py`, puis régénérer les polices.

## Conseils de traduction

- L'écran fait 200 × 200 pixels : une traduction ne doit pas être plus longue que le français (titre : environ
  16 caractères, pied de page : environ 30). `tools/badge_screens.py` avec la touche `U` signale les textes coupés.
- Garder les mêmes formats (`%d`, `%s`, `%02u`...) dans le même ordre, et les mêmes retours à la ligne (`\n`).
- Les boutons : G = aile gauche, D = aile droite, flancs = côtés (en anglais : L, R, sides).
- Les captures d'écran d'une langue : `python tools/badge_screens.py --lang en` (voir le [guide avancé](guide_avance.md)).
