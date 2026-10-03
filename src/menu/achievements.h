/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file achievements.h
 *
 * \brief The achievements ("Succès") and the level of the cicada, like the level of the dolphin of the Flipper Zero:
 * each achievement gives experience points (XP), the meetings too; the level goes from 1 (Oeuf) to 10
 * (Cigale d'or). Shown in Badge > Succès and sent in the beacon (the radar shows the level of the others: "N3").
 *
 * The features call achv_unlock() when the achievement is reached; achv_add() counts (games played and won,
 * trades...: statistics saved for later achievements). A new achievement is announced in the footer, with a chime.
 * */

#ifndef _ACHIEVEMENTS_H
#define _ACHIEVEMENTS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ACHV_FIRST_BOOT,  /* Premiers pas: the badge started */
    ACHV_MEET_1,  /* Bonjour !: 1 meeting */
    ACHV_MEET_10,  /* Sociable: 10 meetings */
    ACHV_MEET_50,  /* Star du réseau: 50 meetings */
    ACHV_MESSAGE,  /* Facteur: a message sent */
    ACHV_CONTACT,  /* Carte de visite: a contact received */
    ACHV_VOTE,  /* Citoyen: a vote */
    ACHV_CHORUS,  /* Choriste: sang in the chorus */
    ACHV_INFECTED,  /* Patient: caught the virus */
    ACHV_CURED,  /* Remède: cured */
    ACHV_DUEL_WIN,  /* Duelliste: won a rock paper scissors */
    ACHV_BATTLE_WIN,  /* Amiral: won a sea battle */
    ACHV_WEREWOLF_PLAY,  /* Pleine lune: played loup-garou */
    ACHV_WEREWOLF_WIN,  /* Survivant: won at loup-garou */
    ACHV_ASSASSIN_KILL,  /* Ombre: eliminated a target */
    ACHV_ASSASSIN_WIN,  /* Dernier debout: won the assassin */
    ACHV_TUG_WIN,  /* Costaud: won the tug of war */
    ACHV_BOOK_END,  /* Héros: reached an end of a gamebook */
    ACHV_RTTTL,  /* Mélomane: played a ringtone */
    ACHV_TRADE,  /* Contrebandier: first trade */
    ACHV_TRADE_RARE,  /* Trésor: got a legendary good */
    ACHV_CARGO_FULL,  /* Collectionneur: every kind of good */
    ACHV_CTF_FLAG,  /* Hacker: a flag of the CTF */
    ACHV_CRYPTO,  /* Cryptographe: a crypto challenge */
    ACHV_HOTCOLD,  /* Fin limier: found the hot / cold beacon */
    ACHV_HUNT433,  /* Chasseur d'ondes: heard a 433 MHz remote */
    ACHV_RECORD,  /* Recordman: a record in a game */
    ACHV_VIDEO,  /* Cinéphile: a whole video */
    ACHV_IMAGE_SENT,  /* Photographe: an image sent by radio */
    ACHV_SKILLS,  /* Expert: skills checked */
    ACHV_SKILL_MATCH,  /* Âmes sœurs: met a cicada sharing a skill */
    ACHV_BABBLE,  /* Cigale bavarde: decoded the Morse code challenge */
    ACHV_ALL,  /* Platine: all the others */
    ACHV_COUNT,
} achv_id_t;

/* Counters (store_t.achv_counters): achv_add() unlocks the achievements at their threshold */
typedef enum {
    ACHV_CNT_TRADES,
    ACHV_CNT_KILLS,
    ACHV_CNT_GAMES,  /* Group games played */
    ACHV_CNT_WINS,  /* Games won (all kinds) */
    ACHV_CNT_BOOK_ENDS,
    ACHV_CNT_RINGTONES,
} achv_counter_t;

void achievements_init(void);

/** \brief Unlocks an achievement (once): announced, and saved. */
void achv_unlock(achv_id_t id);
bool achv_unlocked(achv_id_t id);

/** \brief Adds to a counter. \return the new value */
uint16_t achv_add(achv_counter_t counter, uint16_t n);

/** \brief Experience points and level (1..ACHV_LEVELS) of this cicada. */
uint32_t achv_xp(void);
uint8_t achv_level(void);
const char *achv_level_name(uint8_t level);
#define ACHV_LEVELS 10

/** \brief The last achievement unlocked, once (e.g. "Succès : Sociable"). */
bool achv_event(char *msg, int len);

/** \brief Checks the achievements that come from the state of the badge (meetings...), to call now and then. */
void achv_task(void);

#endif /* _ACHIEVEMENTS_H */
