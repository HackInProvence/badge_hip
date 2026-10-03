# Gamebooks: the books in which you are the hero

The badge holds **gamebooks** ("Gamebooks", Livres-jeux): a story cut into numbered sections, and at the end of each
section, you choose what happens next. Some choices lead to victory, others to a less glorious end...

One book comes with the badge, **Le Trésor du capitaine Cigalon** (Captain Cigalon's treasure, in French: a hacker
cicada, the Ant, a treasure in the calanques of La Ciotat), and you can add as many as you like on the SD card, or
write your own.

Menu: **Media > Gamebooks** (Médias > Livres-jeux).

*Version française : [livres_jeux.md](../fr/livres_jeux.md).*


## 1. Reading a book

### 1.1 The list of the books

- The book built into the badge, then the books of the `LIVRES` folder of the SD card (16 at most).
- **Continue: ...** (Continuer : ...) at the top of the list: resumes the last book where you left it.
- Sides: choose a book, **R** (right wing): open it, **L** (left wing): quit.

### 1.2 The menu of a book

The title, the author, then:

| Row | Effect |
|---|---|
| Start (Commencer) | The beginning of the story. |
| Resume reading (Reprendre la lecture) | The section where you were. |
| Start over (Recommencer) | Back to the beginning, without the items picked up. |
| Items (Objets) | The items you carry (if the book has any). |
| Other books (Autres livres) | Back to the list. |

### 1.3 The reading page

```
┌─────────────────────────────┐
│ 12   Le Trésor du ca...  1/3│  number of the section, title, page
│   The text of the section,  │
│ cut to the width of the     │
│ screen...                   │
│                             │
│ > Open the door             │  the selected choice is in black
│ > Turn back                 │
├─────────────────────────────┤
│   R: choose  L: menu        │
└─────────────────────────────┘
```

| Button | Effect |
|---|---|
| Right side | Next page, then next choice. |
| Left side | Previous choice, then previous page. |
| R (right wing) | Take the selected choice (or next page when no choice is on the screen yet). |
| L (left wing) | The menu of the book. |
| L held | Quit the gamebooks. |

- When the book rolls a **die**, the result is shown ("The die rolls... 4.", Le dé roule... et donne 4.) and
  only the matching choices are offered.
- An **item** won or lost is announced ("You get: antenna.", Vous obtenez : ...); some choices only show up if
  you have (or do not have) an item.
- At an **ending**: "~ END ~" (~ FIN ~), won or lost, then *Start over* or *Other books*. Each ending reached counts for the
  achievements (**Hero**, Héros, at the first ending).
- The progress (book, section, items) is **saved** at each section, even if the badge is switched off.
  One book at a time: starting another book replaces the saved progress.


## 2. Adding books on the SD card

Copy the `.txt` files into the `LIVRES` folder at the root of the card:

```
SD card
└── LIVRES/
    ├── tresor_cigalon.txt
    └── my_book.txt
```

The repository holds books ready to copy in `docs/sd/LIVRES/`, each in French and in English:

| Book | Files | Sections, endings |
|---|---|---|
| Le Trésor du capitaine Cigalon (the book built into the badge) | `tresor_cigalon.txt` | |
| CTF Panic / Panique au CTF: the CTF scoreboard hacked the night before the final | `ctf_panic.txt`, `panique_ctf.txt` | 66 sections, 13 endings |
| The Mistral Beacon / La Balise du Mistral: a 433 MHz radio foxhunt in the calanques | `mistral_beacon.txt`, `balise_mistral.txt` | 69 sections, 12 endings |
| The Deep-Sea Cicada / La Cigale des profondeurs: by submarine to the old telegraph cable to Algiers | `deep_sea_cicada.txt`, `cigale_profondeurs.txt` | 71 sections, 9 endings |
| The Market Robot / Le Robot du marché provençal: the market robot gone haywire before the mayor's speech | `market_robot.txt`, `robot_marche.txt` | 66 sections, 12 endings |

> Respect copyright: only use your own texts or works free of rights.


## 3. Writing a book

A book is a plain text file, written with any editor (Notepad, VS Code...), preferably in **UTF-8** (the
Windows-1252 of old editors is accepted too). The keywords of the format are French: `FIN` (end), `gagné` (won),
`perdu` (lost), `dé` (die).

### 3.1 A complete example

```
La Cigale et le Code perdu
Une cigale anonyme
// Lines that start with // are comments.

== 1
You arrive in front of the door of the room. It is locked
by a keypad.

An ant walks by with a bunch of keys.

-> 2 : Ask the ant for the key
-> 3 : Try 1234 on the keypad

== 2
"I don't lend", says the ant. But she drops
a key as she leaves.
+[key]
-> 4 [key] : Open the door with the key

== 3
The keypad beeps. Three times. Then the alarm goes off.
-> 5 [dé 1-3] : Run!
-> 6 [dé 4-6] : Stay calm

== 4 FIN gagné
The door opens: welcome to the hackerspace!

== 5 FIN perdu
You run straight into the guard.

== 6
The guard recognizes you and lets you in.
-> 4
```

### 3.2 The rules

| Line | Meaning |
|---|---|
| 1st line | The **title** of the book (shown in the list). |
| 2nd line | The **author** (optional). The following lines, up to the first section, are ignored. |
| `// text` | A comment, ignored everywhere. |
| `== 12` | The start of **section 12** (from 1 to 65535). The **first section of the file** is the beginning of the story. |
| `== 12 FIN` | A section that is an **ending**. `== 12 FIN gagné`: a victory, `== 12 FIN perdu`: a defeat. |
| text | The text of the section. Lines that follow each other make a paragraph; an **empty line** starts a new paragraph; a line that starts with a dialogue dash (`—`, `–` or `- `) goes to a new line. |
| `-> 34 : Open the door` | A **choice** that leads to section 34. Without text (`-> 34`), the choice is called "Continue" (Continuer). |
| `-> 34 [key] : ...` | Choice offered only if you have the item `key`. |
| `-> 34 [!key] : ...` | Choice offered only if you do **not** have the item `key`. |
| `-> 34 [dé 1-3] : ...` | The badge rolls a 6-sided die when arriving in the section: this choice is only offered if the die gives 1, 2 or 3 (`[dé 6]`: only 6). Plan a choice for each value. |
| `+[key]` | You **win** the item `key` when arriving in the section (`-[key]`: you **lose** it). Several on a line: `+[key] -[map]`. |

A few details:
- The choices and the items can be placed anywhere in the section; the badge always shows the text, then the items
  and the die, then the choices.
- A section **without any choice** is an ending (better write it with `FIN`).
- The names of the items ignore the case (`Key` = `key`). A book has **8 items at most**.
- Windows, Mac or Linux line endings are accepted, as well as the UTF-8 header (BOM) of Notepad.
- The quotes « », the typographic apostrophes ’, the ellipsis … and the dashes — are converted for the fonts of the
  badge (" ' ... -), as well as œ (oe). The French accented letters are shown; emojis and the other characters
  become "?".

### 3.3 The limits of the badge

The badge never loads the whole book into memory: it only reads the section shown.

| Limit | Value |
|---|---|
| Sections per book | 400 |
| Text of a section | about 2,000 bytes (≈ 1,900 characters); beyond, the text is cut with "..." |
| Choices per section | 8 |
| Text of a choice | 95 bytes |
| Items per book | 8 |
| Books on the card | 16 |

For a comfortable reading, aim at sections of **2 or 3 pages** (300 to 500 characters): a page of the screen holds
8 lines of about 30 characters.

### 3.4 Checking your book

Before copying it to the card, check the book with the script of the repository (Python 3):

```bash
python tools/gamebook_check.py LIVRES/my_book.txt
python tools/gamebook_check.py LIVRES/my_book.txt --pages   # size and number of pages of each section
python tools/gamebook_check.py LIVRES/my_book.txt --play    # play it in the terminal
```

It reports (its messages are in French):
- **errors**: no section, a section twice, a choice to a section that does not exist, too many sections, choices or
  items;
- **warnings**: sections never reached, sections from which no ending is possible (loop with no way out), a die
  whose values do not all have a choice, an item tested but never given, a text too long, characters missing from
  the fonts.

On the badge, a missing section does not crash the reading: a page "Section 34 is missing: the book is
incomplete." (La section 34 est introuvable) offers to go back or to start over.

### 3.5 Changing the book built into the firmware

The built-in book is generated from the example file of the SD card:

```bash
python tools/gamebook_check.py docs/sd/LIVRES/tresor_cigalon.txt --c src/menu/gamebook_builtin.c
```

The test `python src/tests/host/run_tests.py gamebook` checks that both are identical, that all the links are valid
and that thousands of random games all reach an ending.
