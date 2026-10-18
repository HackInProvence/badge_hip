# Conference badge

**Badge > Badge conférence** (Conference badge) shows the program of any event, read from the SD card: the
information, the talks, the workshops and the exhibitors. The badge can be used at other conferences than SecSea,
without flashing it again.

## On the SD card

A `CONFERENCES` folder at the root, with a sub-folder per event:

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

Copy through the USB, without taking the card out: `python tools/sd_files.py put docs/sd/CONFERENCES CONFERENCES`.

Every file is optional: only the ones present (and not empty) are shown. Full examples for ten French events in
[docs/sd/CONFERENCES](../sd/CONFERENCES) (copy them as they are to the card).

## File format

UTF-8 text, **blocks** of `key: value` lines separated by **an empty line**. Lines starting with `#` are comments.
An unknown key is ignored, a missing key is not shown. The keys are in French:

| File | Keys |
|---|---|
| `infos.txt` (one block) | `nom` (name, shown in the list), `dates`, `debut` (start, YYYY-MM-DD: the order of the list), `lieu` (place), `adresse` (address), `site` (QR code), `description`, `source` (not shown) |
| `conferences.txt`, `workshops.txt` | `jour` (day, YYYY-MM-DD), `heure` (time, HH:MM), `fin` (end), `salle` (room), `titre` (title), `orateur` (speaker), `resume` (summary), `lien` (link, QR code) |
| `exposants.txt` (exhibitors) | `nom` (name), `stand` (booth), `description`, `lien` (link, QR code) |

```
jour: 2026-10-16
heure: 14:00
fin: 14:45
salle: L'Eden
titre: 1 an de hack
orateur: Miaou
resume: One year of projects ...
lien: https://secsea.org/
```

Without `infos.txt` or `nom`, the event is named after its folder. The blocks are shown in the order of the file:
sort them by day and time.

## Limits

- Any number of events (by pages of 12), 80 blocks per file (the next ones are ignored).
- 480 bytes per line, 1,280 bytes per block (the rest of the block is not shown).
- Only the characters of the fonts of the badge are drawn (ASCII, French accents and a few others).
- A link too long for a QR code shows "(QR code trop grand)".

## On the badge

- The list of the events: **SecSea always first**, then the others by start date (`debut` of `infos.txt`; without
  it, at the end); Y / X choose, A back, B open.
- An event: Infos, Talks (n), Workshops (n), Exhibitors (n).
- A list: the day, the time and the title (or the name of the exhibitor); the selected row scrolls when long.
- An item: the title, the speaker, the room, the summary (scrolling a line at a time); Y / X the previous or next
  item, B the QR code of the link.
