# Contrebande — the smuggler cicada

*Social > Contrebande* (smuggling): rare virtual goods, to collect and to trade **on the quiet** between two badges
held against each other. Theme: pirate smuggling in Provence (food, rum, spices, treasures), with three legendary
goods.

*Version française : [contrebande.md](../fr/contrebande.md).*

## 1. For the player

### The pages

| Page | Content | Buttons |
|---|---|---|
| Home | Cale (hold, number), Échanger en douce (trade on the quiet), Donner (give), Collection (x / 26), Fortune (doubloons) | flanks: choose, R: open, L: back |
| Cale (hold) | Grid of the icons owned, with their number; under the grid: name, rarity, value | flanks: choose, R: details, L: back |
| Collection | All the goods; those never owned as a grey silhouette with "?" | same |
| Details | Large icon, rarity, value, number in the hold, a short story | L: back |
| Fortune | Total value in doubloons, rank (Mousse (cabin boy), Matelot (sailor), Contrebandier (smuggler), Capitaine (captain), Roi de la contrebande (king of smuggling)), goods, different ones, deals closed | L: back |
| À portée de main (within reach) | The cicadas close enough to trade (name, RSSI) | flanks: choose, R: offer, L: back |
| Trade (dark page) | Offer, choice of the offer, both offers side by side, closing | see below |

R (*D* on the badge) is the right wing, L (*G*) the left wing.
A long press on L leaves the page (a deal in progress is cancelled, unless your good is already sealed).

### Getting goods

- **First opening**: the hold gets 4 common goods at random (`cargo_seeded`).
- **Encounters**: at each new cicada met (the counter of the cicada network goes up), a one-in-two chance of finding
  a good "in the hold", drawn according to its rarity: common 75 %, rare 22 %, legendary 3 %.
  The announcement is discreet: a message at the bottom of the screen, no sound.
- **Trades and gifts** with the other cicadas.

### Trading on the quiet

1. Home > *Échanger en douce*: the list of the cicadas **à portée de main** (badges touching, see § 3).
2. R on a cicada: it receives "Psst... *name* propose une affaire en douce" (*name* offers a deal on the quiet);
   R accepts, L refuses.
3. Each one chooses in its hold the good it offers (dark grid, R: offer).
4. Both badges show both offers ("Votre X contre Y": your X for Y): R closes the deal, L cancels.
5. When both have closed, the trade happens on both badges: "Affaire conclue" (deal closed), with the good received.

The one who was invited can no longer cancel after closing: his good is **sealed** (taken out of his hold) until
the decision of the other one. If he leaves the page, the deal ends in the background, and a discreet message
announces the good received ("Reçu en douce : ...", received on the quiet).

### Giving

Home > *Donner*: choose the good, then the cicada within reach. It sees "*name* vous offre :" (*name* offers you)
with the icon, R accepts (the good goes from one hold to the other), L refuses.

### Admin: adding a good

Admin > Contrebande (admin): the 26 goods one by one (flanks), with their rarity and the number in the cargo; the
right wing adds one to the cargo of this badge (saved at once), with the achievements of a real acquisition
("Trésor" for a legendary one, "Collectionneur"). To unlock a rare good, put it in play, or prepare a demonstration
badge. Trace: `smuggler: admin added <name> (<count>)`.

### Achievements

| Achievement | When |
|---|---|
| Contrebandier (smuggler) | a deal closed (trade or gift, given or received); each deal adds 1 to the counter `ACHV_CNT_TRADES` |
| Trésor (treasure) | a legendary good obtained (trade, gift or found in the hold) |
| Collectionneur (collector) | all the goods owned at least once (the Collection is complete) |

## 2. The goods

26 goods (`smuggler_goods.c`), at most `STORE_CARGO_ITEMS` = 32, each one with a 32 × 32 icon drawn in ASCII art in
[tools/smuggler_icons.py](../../tools/smuggler_icons.py).

| Rarity | Goods (value in doubloons) |
|---|---|
| Common | Biscuit de mer (ship's biscuit, 1), Fromage (cheese, 3), Poisson salé (salted fish, 2), Citrons (lemons, 2), Olives (2), Figues (figs, 3), Navettes (Marseille biscuits, 3), Bouteille de rhum (bottle of rum, 5), Pastis (4), Saucisson (3), Lavande (lavender, 2), Savon de Marseille (Marseille soap, 3) |
| Rare | Tonneau de rhum (barrel of rum, 20), Calissons (12), Safran (saffron, 30), Poivre (pepper, 15), Vanille (vanilla, 20), Boussole (compass, 18), Longue-vue (spyglass, 20), Carte au trésor (treasure map, 35), Clé du coffre (key of the chest, 25), Perle noire (black pearl, 40), Bourse de doublons (purse of doubloons, 30) |
| Legendary | La cigale d'or (the golden cicada, 250), Le crâne de cristal (the crystal skull, 180), Le perroquet savant (the learned parrot, 150) |

To change an icon: edit the ASCII art (`#` = black, `.` = white), then

    python tools/smuggler_icons.py              # generates the C arrays in src/menu/smuggler_goods.c again
    python tools/smuggler_icons.py --png a.png  # contact sheet (zoom 4)

The host test `smuggler` checks that `smuggler_goods.c` is up to date (`--check`).
To add a good: an icon in the tool, a row in the table of `smuggler_goods.c`, and `SMUGGLER_GOODS` (at most 32); the
order of the table is the index saved in the hold: never reorder nor remove a good once the badges are handed out
(add at the end).

**The hold**: `store_t.cargo[32]`, one byte per good: bits 0-6 = number (99 at most), bit 7 = already owned
(the Collection). `cargo_seeded` = 1 after the first opening. These fields are set to 0 when the store is initialized
(`v2_magic`).

## 3. "À portée de main": the RSSI threshold

A trade is only possible with a cicada whose beacons (+10 dBm) arrive with an RSSI ≥ `SMUGGLER_TRADE_RSSI`
(**-55 dBm** by default, in `smuggler.c`, can be changed at build time: `-DSMUGGLER_TRADE_RSSI=-60`).
As a reminder, at 1 m the RSSI is -70 to -83 dBm (social.h): -55 dBm means badges touching or a few centimetres
apart. **To calibrate on site**: *Social > Radar des cigales* shows the RSSI; the *À portée de main* page too, and
the serial link prints `smuggler: at hand <name>#<id> <rssi> dBm`.

An invitation received is accepted down to `SMUGGLER_TRADE_RSSI - 10` dBm (`SMUGGLER_RSSI_MARGIN`), because the RSSI
varies from one packet to the next. Once the deal has started, the RSSI is no longer checked.

## 4. The radio protocol (NET_TRADE = 0x07)

Code: [smuggler_trade.c](../../src/menu/smuggler_trade.c) (pure logic, no SDK, tested on the PC);
[smuggler.c](../../src/menu/smuggler.c) connects the radio, the page and the achievements.

Packet (after the header of the network): `[trade id 4][type][recipient 4][data]`, sent at +10 dBm.
The id is drawn at random by the one who invites.

| Type | Data | Direction |
|---|---|---|
| INVITE (1) | mode (0 trade, 1 gift), good of the gift, name (8) | inviter → guest, repeated every 500 ms |
| ACCEPT (2) | — | guest → inviter (repeated while choosing the offer) |
| REFUSE (3) | — | guest → inviter |
| ALIVE (4) | — | inviter, while choosing its offer |
| OFFER (5) | good | both, repeated |
| CONFIRM (6) | my good, its good | both, repeated |
| DONE (7) | good of the inviter, of the guest | inviter → guest, repeated until ACK (20 s at most) |
| ACK (8) | — | guest → inviter |
| ABORT (9) | — | both |

### Two-phase commit

The **inviter decides**:

1. When the guest closes, its good leaves its hold (**sealed**) and it sends CONFIRM until it knows the decision.
   It can no longer cancel.
2. The inviter, when it has closed **and** received the CONFIRM of the guest (with the same offers), **commits**: its
   hold changes at once (it gives its good, receives the guest's one), the deal is recorded as "closed", it sends DONE.
3. The guest receives DONE: it receives the good of the inviter (its own is already gone), records the deal, answers
   ACK.
4. As long as it has not committed, the inviter can cancel (button, or silence of the other one for 20 s): the deal
   is recorded as "cancelled", it sends ABORT; the sealed guest then gets its good back.

**Lost or duplicated packets**: each badge keeps the last 16 deals decided (id, peer, outcome, goods) and always
answers them the same way: a CONFIRM repeated after the commit gets DONE, a DONE repeated gets ACK (and is only
applied once), any packet of a cancelled deal gets ABORT. The offers never change during a deal. The gift follows the
same path: the guest accepts with CONFIRM (it gives nothing), the inviter commits and sends DONE.

**Separation**: the sealed guest never gives up alone; if the inviter goes away, it asks again every 3 s, in the
background (even with the page closed), and the deal ends when the badges meet again.

**Worst case**: the inviter is switched off (or has decided 16 other deals since) before the guest knows the
decision, then the guest is switched off too: the sealed good is **lost**, never duplicated.
The hold is written to the flash **at once** at each change due to a deal (`store_save_now()`, store.h): a badge
switched off just after a trade does not find its old hold again (which could duplicate a good). The goods of the
first opening and the finds of the encounters are written 5 s later (`store_changed()`): a badge switched off within
these 5 s loses them.

The host test simulates 3000 deals over a radio that loses up to 70 % of the packets, duplicates up to 40 %, out of
order, with cancellations and separations: no good is ever created, and in the end each deal is either done on both
badges, or on none, without loss.

### Log (serial link)

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
    smuggler: confirmed, Safran sealed          (guest)
    smuggler: Marius confirmed / smuggler: confirmed   (inviter)
    smuggler: done trade 1A2B3C4D inviter gave=1 got=14 (Fromage -> Safran)
    smuggler: done trade 1A2B3C4D guest gave=14 got=1 (Safran -> Fromage)
    smuggler: trade 1A2B3C4D acknowledged
    smuggler: trade ... cancelled | refused | cancelled by the peer | lost (peer silent)
    smuggler: Safran back in the cargo
    smuggler: error: ...

## 5. Files and tests

| File | Role |
|---|---|
| `src/menu/smuggler.c` | the application (pages, radio, finds, achievements), `smuggler_init()`, `smuggler_task()`, `smuggler_invited()`, `smuggler_event()` |
| `src/menu/smuggler_goods.c/.h` | the goods, their icons (generated), the hold |
| `src/menu/smuggler_trade.c/.h` | the trade protocol (pure logic) |
| `tools/smuggler_icons.py` | the ASCII art of the icons, the generation of the C code, the PNG sheet |
| `src/tests/host/test_smuggler.c` | host test: the hold, the draws, the protocol over a lossy radio |
| `tools/test_smuggler.py` | two-badge test |

Tests:

    python src/tests/host/run_tests.py smuggler
    python tools/test_smuggler.py --ports COM9 COM11

The two-badge test (badges touching) opens *Social > Contrebande* on both, then goes through: a trade (offers seen on
both sides, goods swapped), a gift, a refusal, and a cancellation by the inviter after the guest sealed its good (it
comes back to it). The screenshots are in `smuggler_<date>/A` and `/B`.
