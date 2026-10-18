/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Chorus of the cicadas: the badges play the same song together, each one a voice (docs/chorus_sync.md).
 * The songs are in the firmware (public domain). A leader (admin menu, or a remote: Princeton key 0xC16A3n)
 * broadcasts "start in 2000 ms": every badge hears the same packet at the same time (reference broadcast) and
 * starts on its own clock; the leader broadcasts its position every 2 s for the badges that join late.
 * NET_SONG [song][kind: 1 start, 2 position, 3 stop][session 2][ms 4][voices] */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "achievements.h"
#include "pico/rand.h"

#include "app.h"
#include "audio.h"
#include "i18n.h"
#include "net.h"
#include "remote.h"

#define RATE 16000
#define START_DELAY_MS 2000
#define START_REPEATS 3
#define POSITION_PERIOD_MS 2000
#define CHUNK 256
#define AHEAD_SAMPLES 1600  /* 100 ms of sound written in advance */
#define MAX_VOICES 4

enum { SONG_START = 1, SONG_POSITION = 2, SONG_STOP = 3 };

typedef struct {
    uint8_t note;  /* MIDI note, 0 = rest */
    uint8_t eighths;  /* Duration in eighths of a beat... in eighth notes (a beat = 2) */
} note_t;

typedef struct {
    const note_t *notes;
    uint16_t n;
    uint8_t repeats;  /* Times the notes are played */
    uint16_t offset;  /* Eighths of silence before (canon) */
} voice_t;

typedef struct {
    const char *name;
    uint16_t bpm;
    uint8_t n_voices;
    voice_t voices[MAX_VOICES];
} song_t;

/* Frère Jacques (traditional), a canon: the voices enter every 2 bars */
static const note_t FRERE_JACQUES[] = {
    {84, 2}, {86, 2}, {88, 2}, {84, 2}, {84, 2}, {86, 2}, {88, 2}, {84, 2},
    {88, 2}, {89, 2}, {91, 4}, {88, 2}, {89, 2}, {91, 4},
    {91, 1}, {93, 1}, {91, 1}, {89, 1}, {88, 2}, {84, 2}, {91, 1}, {93, 1}, {91, 1}, {89, 1}, {88, 2}, {84, 2},
    {84, 2}, {79, 2}, {84, 4}, {84, 2}, {79, 2}, {84, 4},
};
#define N_FJ (sizeof(FRERE_JACQUES) / sizeof(FRERE_JACQUES[0]))

/* Ode to Joy (Beethoven, 9th symphony, 1824): the melody, a third below, the bass */
static const note_t ODE_MELODY[] = {
    {88, 2}, {88, 2}, {89, 2}, {91, 2}, {91, 2}, {89, 2}, {88, 2}, {86, 2},
    {84, 2}, {84, 2}, {86, 2}, {88, 2}, {88, 3}, {86, 1}, {86, 4},
    {88, 2}, {88, 2}, {89, 2}, {91, 2}, {91, 2}, {89, 2}, {88, 2}, {86, 2},
    {84, 2}, {84, 2}, {86, 2}, {88, 2}, {86, 3}, {84, 1}, {84, 4},
};
static const note_t ODE_THIRD[] = {
    {84, 2}, {84, 2}, {86, 2}, {88, 2}, {88, 2}, {86, 2}, {84, 2}, {83, 2},
    {81, 2}, {81, 2}, {83, 2}, {84, 2}, {84, 3}, {83, 1}, {83, 4},
    {84, 2}, {84, 2}, {86, 2}, {88, 2}, {88, 2}, {86, 2}, {84, 2}, {83, 2},
    {81, 2}, {81, 2}, {83, 2}, {84, 2}, {83, 3}, {84, 1}, {84, 4},
};
static const note_t ODE_BASS[] = {
    {72, 8}, {67, 8}, {72, 8}, {67, 8}, {72, 8}, {67, 8}, {72, 8}, {67, 4}, {72, 4},
};
#define N_OF(a) (sizeof(a) / sizeof(a[0]))

/* Korobeiniki (Russian folk song, 1861; the theme of Tetris), A minor: the melody (A A B), a third or a
 * sixth below, the bass in octaves */
static const note_t KORO_MELODY[] = {
    {76, 2}, {71, 1}, {72, 1}, {74, 2}, {72, 1}, {71, 1}, {69, 2}, {69, 1}, {72, 1}, {76, 2},
    {74, 1}, {72, 1}, {71, 3}, {72, 1}, {74, 2}, {76, 2}, {72, 2}, {69, 2}, {69, 2}, {0, 2},
    {74, 3}, {77, 1}, {81, 2}, {79, 1}, {77, 1}, {76, 3}, {72, 1}, {76, 2}, {74, 1}, {72, 1},
    {71, 2}, {71, 1}, {72, 1}, {74, 2}, {76, 2}, {72, 2}, {69, 2}, {69, 2}, {0, 2}, {76, 2},
    {71, 1}, {72, 1}, {74, 2}, {72, 1}, {71, 1}, {69, 2}, {69, 1}, {72, 1}, {76, 2}, {74, 1},
    {72, 1}, {71, 3}, {72, 1}, {74, 2}, {76, 2}, {72, 2}, {69, 2}, {69, 2}, {0, 2}, {74, 3},
    {77, 1}, {81, 2}, {79, 1}, {77, 1}, {76, 3}, {72, 1}, {76, 2}, {74, 1}, {72, 1}, {71, 2},
    {71, 1}, {72, 1}, {74, 2}, {76, 2}, {72, 2}, {69, 2}, {69, 2}, {0, 2}, {76, 4}, {72, 4},
    {74, 4}, {71, 4}, {72, 4}, {69, 4}, {68, 4}, {71, 4}, {76, 4}, {72, 4}, {74, 4}, {71, 4},
    {72, 2}, {76, 2}, {81, 4}, {80, 8},
};
static const note_t KORO_THIRD[] = {
    {68, 2}, {68, 1}, {69, 1}, {71, 2}, {69, 1}, {68, 1}, {64, 2}, {64, 1}, {69, 1}, {72, 2},
    {71, 1}, {69, 1}, {68, 3}, {69, 1}, {71, 2}, {68, 2}, {69, 2}, {64, 2}, {64, 2}, {0, 2},
    {65, 3}, {69, 1}, {77, 2}, {76, 1}, {74, 1}, {72, 3}, {69, 1}, {72, 2}, {71, 1}, {69, 1},
    {68, 2}, {68, 1}, {69, 1}, {71, 2}, {68, 2}, {69, 2}, {64, 2}, {64, 2}, {0, 2}, {68, 2},
    {68, 1}, {69, 1}, {71, 2}, {69, 1}, {68, 1}, {64, 2}, {64, 1}, {69, 1}, {72, 2}, {71, 1},
    {69, 1}, {68, 3}, {69, 1}, {71, 2}, {68, 2}, {69, 2}, {64, 2}, {64, 2}, {0, 2}, {65, 3},
    {69, 1}, {77, 2}, {76, 1}, {74, 1}, {72, 3}, {69, 1}, {72, 2}, {71, 1}, {69, 1}, {68, 2},
    {68, 1}, {69, 1}, {71, 2}, {68, 2}, {69, 2}, {64, 2}, {64, 2}, {0, 2}, {72, 4}, {69, 4},
    {71, 4}, {68, 4}, {69, 4}, {64, 4}, {64, 4}, {68, 4}, {72, 4}, {69, 4}, {71, 4}, {68, 4},
    {69, 2}, {72, 2}, {76, 4}, {76, 8},
};
static const note_t KORO_BASS[] = {
    {52, 1}, {64, 1}, {52, 1}, {64, 1}, {52, 1}, {64, 1}, {52, 1}, {64, 1}, {57, 1}, {69, 1},
    {57, 1}, {69, 1}, {57, 1}, {69, 1}, {57, 1}, {69, 1}, {52, 1}, {64, 1}, {52, 1}, {64, 1},
    {52, 1}, {64, 1}, {52, 1}, {64, 1}, {57, 1}, {69, 1}, {57, 1}, {69, 1}, {57, 1}, {69, 1},
    {57, 1}, {69, 1}, {50, 1}, {62, 1}, {50, 1}, {62, 1}, {50, 1}, {62, 1}, {50, 1}, {62, 1},
    {57, 1}, {69, 1}, {57, 1}, {69, 1}, {57, 1}, {69, 1}, {57, 1}, {69, 1}, {52, 1}, {64, 1},
    {52, 1}, {64, 1}, {52, 1}, {64, 1}, {52, 1}, {64, 1}, {57, 1}, {69, 1}, {57, 1}, {69, 1},
    {57, 1}, {69, 1}, {57, 1}, {69, 1}, {52, 1}, {64, 1}, {52, 1}, {64, 1}, {52, 1}, {64, 1},
    {52, 1}, {64, 1}, {57, 1}, {69, 1}, {57, 1}, {69, 1}, {57, 1}, {69, 1}, {57, 1}, {69, 1},
    {52, 1}, {64, 1}, {52, 1}, {64, 1}, {52, 1}, {64, 1}, {52, 1}, {64, 1}, {57, 1}, {69, 1},
    {57, 1}, {69, 1}, {57, 1}, {69, 1}, {57, 1}, {69, 1}, {50, 1}, {62, 1}, {50, 1}, {62, 1},
    {50, 1}, {62, 1}, {50, 1}, {62, 1}, {57, 1}, {69, 1}, {57, 1}, {69, 1}, {57, 1}, {69, 1},
    {57, 1}, {69, 1}, {52, 1}, {64, 1}, {52, 1}, {64, 1}, {52, 1}, {64, 1}, {52, 1}, {64, 1},
    {57, 1}, {69, 1}, {57, 1}, {69, 1}, {57, 1}, {69, 1}, {57, 1}, {69, 1}, {57, 1}, {69, 1},
    {57, 1}, {69, 1}, {57, 1}, {69, 1}, {57, 1}, {69, 1}, {52, 1}, {64, 1}, {52, 1}, {64, 1},
    {52, 1}, {64, 1}, {52, 1}, {64, 1}, {57, 1}, {69, 1}, {57, 1}, {69, 1}, {57, 1}, {69, 1},
    {57, 1}, {69, 1}, {52, 1}, {64, 1}, {52, 1}, {64, 1}, {52, 1}, {64, 1}, {52, 1}, {64, 1},
    {57, 1}, {69, 1}, {57, 1}, {69, 1}, {57, 1}, {69, 1}, {57, 1}, {69, 1}, {52, 1}, {64, 1},
    {52, 1}, {64, 1}, {52, 1}, {64, 1}, {52, 1}, {64, 1}, {57, 1}, {69, 1}, {57, 1}, {69, 1},
    {57, 1}, {69, 1}, {57, 1}, {69, 1}, {52, 1}, {64, 1}, {52, 1}, {64, 1}, {52, 1}, {64, 1},
    {52, 1}, {64, 1},
};

/* Pachelbel, Canon in D (around 1700): the ground bass, and the same line in canon on 3 voices, 2 bars apart */
static const note_t CANON_BASS[] = {
    {62, 2}, {57, 2}, {59, 2}, {54, 2}, {55, 2}, {50, 2}, {55, 2}, {57, 2},
};
static const note_t CANON_LINE[] = {
    {78, 2}, {76, 2}, {74, 2}, {73, 2}, {71, 2}, {69, 2}, {71, 2}, {73, 2}, {74, 2}, {73, 2},
    {71, 2}, {69, 2}, {67, 2}, {66, 2}, {67, 2}, {64, 2}, {62, 1}, {66, 1}, {69, 1}, {67, 1},
    {66, 1}, {62, 1}, {66, 1}, {64, 1}, {62, 1}, {59, 1}, {62, 1}, {69, 1}, {67, 1}, {71, 1},
    {69, 1}, {67, 1},
};

/* Grieg, In the Hall of the Mountain King (Peer Gynt, 1875), B minor: the theme, the same an octave up
 * entering 4 bars later, the bass */
static const note_t KING_THEME[] = {
    {59, 1}, {61, 1}, {62, 1}, {64, 1}, {66, 1}, {62, 1}, {66, 2}, {65, 1}, {61, 1}, {65, 2},
    {64, 1}, {60, 1}, {64, 2}, {59, 1}, {61, 1}, {62, 1}, {64, 1}, {66, 1}, {62, 1}, {66, 1},
    {71, 1}, {69, 1}, {66, 1}, {62, 1}, {66, 1}, {69, 4},
};
static const note_t KING_HIGH[] = {
    {71, 1}, {73, 1}, {74, 1}, {76, 1}, {78, 1}, {74, 1}, {78, 2}, {77, 1}, {73, 1}, {77, 2},
    {76, 1}, {72, 1}, {76, 2}, {71, 1}, {73, 1}, {74, 1}, {76, 1}, {78, 1}, {74, 1}, {78, 1},
    {83, 1}, {81, 1}, {78, 1}, {74, 1}, {78, 1}, {81, 4},
};
static const note_t KING_BASS[] = {
    {47, 1}, {0, 1}, {54, 1}, {0, 1}, {47, 1}, {0, 1}, {54, 1}, {0, 1}, {49, 1}, {0, 1},
    {54, 1}, {0, 1}, {48, 1}, {0, 1}, {52, 1}, {0, 1}, {47, 1}, {0, 1}, {54, 1}, {0, 1},
    {47, 1}, {0, 1}, {54, 1}, {0, 1}, {54, 1}, {0, 1}, {50, 1}, {0, 1}, {54, 2}, {0, 2},
};

/* Row, row, row your boat (traditional), a round in 6/8: the voices enter every 2 bars */
static const note_t ROW_BOAT[] = {
    {72, 3}, {72, 3}, {72, 2}, {74, 1}, {76, 3}, {76, 2}, {74, 1}, {76, 2}, {77, 1}, {79, 6},
    {84, 1}, {84, 1}, {84, 1}, {79, 1}, {79, 1}, {79, 1}, {76, 1}, {76, 1}, {76, 1}, {72, 1},
    {72, 1}, {72, 1}, {79, 2}, {77, 1}, {76, 2}, {74, 1}, {72, 6},
};

/* Jingle Bells (James Lord Pierpont, 1857; "Vive le vent" in French): the chorus, a third below, the bass */
static const note_t JINGLE_MELODY[] = {
    {76, 2}, {76, 2}, {76, 4}, {76, 2}, {76, 2}, {76, 4}, {76, 2}, {79, 2}, {72, 3}, {74, 1},
    {76, 8}, {77, 2}, {77, 2}, {77, 3}, {77, 1}, {77, 2}, {76, 2}, {76, 2}, {76, 1}, {76, 1},
    {76, 2}, {74, 2}, {74, 2}, {76, 2}, {74, 4}, {79, 4}, {76, 2}, {76, 2}, {76, 4}, {76, 2},
    {76, 2}, {76, 4}, {76, 2}, {79, 2}, {72, 3}, {74, 1}, {76, 8}, {77, 2}, {77, 2}, {77, 3},
    {77, 1}, {77, 2}, {76, 2}, {76, 2}, {76, 1}, {76, 1}, {79, 2}, {79, 2}, {77, 2}, {74, 2},
    {72, 8},
};
static const note_t JINGLE_THIRD[] = {
    {72, 2}, {72, 2}, {72, 4}, {72, 2}, {72, 2}, {72, 4}, {72, 2}, {76, 2}, {67, 3}, {71, 1},
    {72, 8}, {69, 2}, {69, 2}, {69, 3}, {69, 1}, {69, 2}, {72, 2}, {72, 2}, {72, 1}, {72, 1},
    {72, 2}, {71, 2}, {71, 2}, {72, 2}, {71, 4}, {71, 4}, {72, 2}, {72, 2}, {72, 4}, {72, 2},
    {72, 2}, {72, 4}, {72, 2}, {76, 2}, {67, 3}, {71, 1}, {72, 8}, {69, 2}, {69, 2}, {69, 3},
    {69, 1}, {69, 2}, {72, 2}, {72, 2}, {72, 1}, {72, 1}, {71, 2}, {71, 2}, {69, 2}, {71, 2},
    {72, 8},
};
static const note_t JINGLE_BASS[] = {
    {60, 2}, {55, 2}, {60, 2}, {55, 2}, {60, 2}, {55, 2}, {60, 2}, {55, 2}, {60, 2}, {55, 2},
    {60, 2}, {55, 2}, {60, 2}, {55, 2}, {60, 2}, {55, 2}, {53, 2}, {60, 2}, {53, 2}, {60, 2},
    {60, 2}, {55, 2}, {60, 2}, {55, 2}, {62, 2}, {57, 2}, {55, 2}, {62, 2}, {55, 2}, {62, 2},
    {55, 2}, {59, 2}, {60, 2}, {55, 2}, {60, 2}, {55, 2}, {60, 2}, {55, 2}, {60, 2}, {55, 2},
    {60, 2}, {55, 2}, {60, 2}, {55, 2}, {60, 2}, {55, 2}, {60, 2}, {55, 2}, {53, 2}, {60, 2},
    {53, 2}, {60, 2}, {60, 2}, {55, 2}, {60, 2}, {55, 2}, {55, 2}, {62, 2}, {55, 2}, {59, 2},
    {60, 8},
};

/* Minuet in G (Christian Petzold, Notebook for Anna Magdalena Bach, 1725), 3/4: the melody, the bass */
static const note_t MINUET_MELODY[] = {
    {74, 2}, {67, 1}, {69, 1}, {71, 1}, {72, 1}, {74, 2}, {67, 2}, {67, 2}, {76, 2}, {72, 1},
    {74, 1}, {76, 1}, {78, 1}, {79, 2}, {67, 2}, {67, 2}, {72, 2}, {74, 1}, {72, 1}, {71, 1},
    {69, 1}, {71, 2}, {72, 1}, {71, 1}, {69, 1}, {67, 1}, {66, 2}, {67, 1}, {69, 1}, {71, 1},
    {67, 1}, {69, 6}, {74, 2}, {67, 1}, {69, 1}, {71, 1}, {72, 1}, {74, 2}, {67, 2}, {67, 2},
    {76, 2}, {72, 1}, {74, 1}, {76, 1}, {78, 1}, {79, 2}, {67, 2}, {67, 2}, {72, 2}, {74, 1},
    {72, 1}, {71, 1}, {69, 1}, {71, 2}, {72, 1}, {71, 1}, {69, 1}, {67, 1}, {69, 2}, {71, 1},
    {69, 1}, {67, 1}, {66, 1}, {67, 6},
};
static const note_t MINUET_BASS[] = {
    {55, 4}, {57, 2}, {59, 6}, {60, 6}, {59, 6}, {57, 6}, {55, 6}, {62, 2}, {59, 2}, {55, 2},
    {62, 2}, {50, 2}, {60, 2}, {59, 4}, {57, 2}, {59, 4}, {55, 2}, {60, 6}, {59, 4}, {55, 2},
    {57, 6}, {55, 6}, {62, 2}, {50, 2}, {54, 2}, {55, 6},
};

static const song_t SONGS[] = {
    {N_("Frère Jacques"), 100, 4, {{FRERE_JACQUES, N_FJ, 2, 0}, {FRERE_JACQUES, N_FJ, 2, 16},
                                            {FRERE_JACQUES, N_FJ, 2, 32}, {FRERE_JACQUES, N_FJ, 2, 48}}},
    {N_("Ode à la joie"), 110, 3, {{ODE_MELODY, N_OF(ODE_MELODY), 1, 0}, {ODE_THIRD, N_OF(ODE_THIRD), 1, 0},
                                   {ODE_BASS, N_OF(ODE_BASS), 1, 0}}},
    {N_("Tetris (Korobeiniki)"), 144, 3, {{KORO_MELODY, N_OF(KORO_MELODY), 1, 0},
                                          {KORO_THIRD, N_OF(KORO_THIRD), 1, 0}, {KORO_BASS, N_OF(KORO_BASS), 1, 0}}},
    {N_("Canon de Pachelbel"), 66, 4, {{CANON_BASS, N_OF(CANON_BASS), 6, 0}, {CANON_LINE, N_OF(CANON_LINE), 1, 16},
                                       {CANON_LINE, N_OF(CANON_LINE), 1, 32}, {CANON_LINE, N_OF(CANON_LINE), 1, 48}}},
    {N_("Roi de la montagne"), 120, 3, {{KING_THEME, N_OF(KING_THEME), 4, 0}, {KING_HIGH, N_OF(KING_HIGH), 3, 32},
                                           {KING_BASS, N_OF(KING_BASS), 4, 0}}},
    {N_("Row your boat"), 100, 4, {{ROW_BOAT, N_OF(ROW_BOAT), 2, 0}, {ROW_BOAT, N_OF(ROW_BOAT), 2, 12},
                                           {ROW_BOAT, N_OF(ROW_BOAT), 2, 24}, {ROW_BOAT, N_OF(ROW_BOAT), 2, 36}}},
    {N_("Vive le vent"), 140, 3, {{JINGLE_MELODY, N_OF(JINGLE_MELODY), 1, 0}, {JINGLE_THIRD, N_OF(JINGLE_THIRD), 1, 0},
                                  {JINGLE_BASS, N_OF(JINGLE_BASS), 1, 0}}},
    {N_("Menuet en sol"), 112, 2, {{MINUET_MELODY, N_OF(MINUET_MELODY), 1, 0},
                                   {MINUET_BASS, N_OF(MINUET_BASS), 1, 0}}},
};
#define N_SONGS ((int)(sizeof(SONGS) / sizeof(SONGS[0])))

/* ------ State ------ */

static int song = -1;  /* Scheduled or playing */
static uint16_t session = 0;
static uint32_t leader = 0;  /* Id of the leader (net_id() when this badge leads) */
static absolute_time_t start_ts = 0;
static bool playing = false;
static int voice = 0;
static bool joined = true;  /* Setting of the page: take part in the choruses */
/* Synthesis of the voice */
static uint32_t written = 0;  /* Samples written since the start */
static uint16_t note_i = 0;
static uint8_t repeat_i = 0;
static uint32_t note_left = 0;  /* Samples left in the current note */
static uint32_t note_len = 0;
static uint32_t period = 0;  /* Samples per period of the current note, 0 = rest */
static bool offset_done = false;
/* Leader */
static int starts_left = 0;
static absolute_time_t next_send = 0;

static uint32_t eighth_samples(void) {
    return RATE * 60 / SONGS[song].bpm / 2;
}

static uint32_t voice_samples(const voice_t *v) {
    uint32_t e = v->offset;
    for (uint16_t i = 0; i < v->n; ++i)
        e += v->notes[i].eighths * v->repeats;
    return e * eighth_samples();
}

static uint32_t song_samples(void) {
    uint32_t longest = 0;
    for (int i = 0; i < SONGS[song].n_voices; ++i) {
        uint32_t s = voice_samples(&SONGS[song].voices[i]);
        if (s > longest)
            longest = s;
    }
    return longest;
}

static void next_note(void) {
    const voice_t *v = &SONGS[song].voices[voice];
    if (! offset_done) {
        offset_done = true;
        if (v->offset) {
            note_left = note_len = v->offset * eighth_samples();
            period = 0;
            return;
        }
    }
    if (note_i >= v->n) {
        note_i = 0;
        ++repeat_i;
    }
    if (repeat_i >= v->repeats) {
        note_left = note_len = RATE;  /* Silence until the other voices end */
        period = 0;
        return;
    }
    const note_t *n = &v->notes[note_i++];
    note_left = note_len = n->eighths * eighth_samples();
    period = n->note ? (uint32_t)(RATE / (440.0f * powf(2.0f, (n->note - 69) / 12.0f))) : 0;
}

/* Advances the synthesis to the sample \p pos (late join) */
static void seek(uint32_t pos) {
    written = 0;
    note_i = repeat_i = 0;
    note_left = 0;
    offset_done = false;
    while (written < pos) {
        if (! note_left)
            next_note();
        uint32_t k = pos - written < note_left ? pos - written : note_left;
        written += k;
        note_left -= k;
    }
}

static void synthesize(void) {
    uint8_t buf[CHUNK];
    while (audio_queued() < AHEAD_SAMPLES && audio_free() > CHUNK) {
        for (int i = 0; i < CHUNK; ++i) {
            if (! note_left)
                next_note();
            uint8_t v = 128;
            if (period) {
                /* Square wave, attack 3 ms then a slow decay, a short gap at the end of the note (articulation) */
                uint32_t t = note_len - note_left;
                int amp = t < 48 ? t * 2 : 96 - (int)(t * 48 / (note_len + 1));
                if (note_left < 400)
                    amp = 0;
                v = 128 + ((t % period) < period / 2 ? amp : -amp);
            }
            buf[i] = v;
            --note_left;
        }
        audio_write(buf, CHUNK);
        written += CHUNK;
    }
}

static void stop_playing(void) {
    if (playing)
        audio_close();
    playing = false;
    song = -1;
    starts_left = 0;
    remote_pause_windows(false);
    printf("chorus: stopped\n");
}

/* Schedules a start (the earliest start wins, then the smallest id of the leader) */
static void schedule(int s, uint16_t sess, uint32_t from, absolute_time_t start) {
    if (song >= 0 && sess != session) {
        bool later = absolute_time_diff_us(start_ts, start) > 0;
        if (later || (! later && absolute_time_diff_us(start, start_ts) == 0 && from > leader))
            return;
    }
    if (song < 0)
        remote_pause_windows(true);  /* Hear all the packets of the leader */
    if (playing)
        audio_close();
    playing = false;
    song = s;
    session = sess;
    leader = from;
    start_ts = start;
    /* The leader sings the first voice, the others share the other voices */
    int n = SONGS[s].n_voices;
    voice = from == net_id() || n < 2 ? 0 : 1 + (int)(net_id() % (n - 1));
    printf("chorus: \"%s\" voice %d in %lld ms\n", SONGS[s].name, voice + 1,
           absolute_time_diff_us(get_absolute_time(), start) / 1000);
}

static void handle_song(const net_packet_t *p) {
    if (p->len < 9 || p->data[0] >= N_SONGS || ! joined)
        return;
    uint16_t sess = p->data[2] | p->data[3] << 8;
    uint32_t ms = net_u32(p->data + 4);
    switch (p->data[1]) {
    case SONG_START:
        if (sess != session || song < 0)
            schedule(p->data[0], sess, p->src, delayed_by_ms(p->at, ms));
        break;
    case SONG_POSITION:
        /* Join late: the song started ms ago */
        if (sess != session || song < 0)
            schedule(p->data[0], sess, p->src, delayed_by_ms(p->at, 0) - (uint64_t)ms * 1000);
        break;
    case SONG_STOP:
        if (sess == session)
            stop_playing();
        break;
    }
}

/* This badge leads: start the song \p s for everybody */
void chorus_lead(int s) {
    if (s < 0 || s >= N_SONGS)
        return;
    uint16_t sess = (get_rand_32() & 0xFFFE) + 1;
    schedule(s, sess, net_id(), delayed_by_ms(get_absolute_time(), START_DELAY_MS));
    starts_left = START_REPEATS;
    next_send = get_absolute_time();
}

static void chorus_remote(uint8_t arg) {
    chorus_lead(arg);
}

void chorus_init(void) {
    net_subscribe(NET_SONG, handle_song);
    remote_subscribe(REMOTE_SONG, chorus_remote);
}

static void send_song(uint8_t kind, uint32_t ms) {
    uint8_t d[9] = {song, kind, session, session >> 8};
    net_put_u32(d + 4, ms);
    d[8] = SONGS[song].n_voices;
    net_send(NET_SONG, d, sizeof(d), NET_LOUD);
}

void chorus_task(absolute_time_t now) {
    if (song < 0)
        return;
    /* The leader: the start, 3 times with the delay left (the same instant), then the position */
    if (leader == net_id() && absolute_time_diff_us(next_send, now) >= 0) {
        int64_t left = absolute_time_diff_us(now, start_ts) / 1000;
        if (starts_left && left > 100) {
            send_song(SONG_START, left);
            --starts_left;
            next_send = delayed_by_ms(now, 300);
        } else if (playing) {
            send_song(SONG_POSITION, absolute_time_diff_us(start_ts, now) / 1000);
            next_send = delayed_by_ms(now, POSITION_PERIOD_MS);
        }
    }
    int64_t since = absolute_time_diff_us(start_ts, now);
    if (! playing && since >= 0) {
        if (audio_is_open()) {
            printf("chorus: the sound is busy, not playing\n");
            song = -1;
            remote_pause_windows(false);
            return;
        }
        audio_open(RATE);
        seek(since * RATE / 1000000);  /* 0 at the start, more when joining late */
        playing = true;
        printf("chorus: playing voice %d\n", voice + 1);
        achv_unlock(ACHV_CHORUS);
    }
    if (playing) {
        if (written >= song_samples() && audio_queued() == 0) {
            if (leader == net_id())
                send_song(SONG_STOP, 0);
            stop_playing();
            return;
        }
        if (written < song_samples())
            synthesize();
    }
}


/* ------ Pages: the chorus (everybody), start a song (admin) ------ */

static int sel = 0;

static void song_label(int i, char *buf, size_t len) {
    snprintf(buf, len, _("%s (%d voix)"), tr(SONGS[i].name), SONGS[i].n_voices);
}

static void chorus_start(absolute_time_t now) {
    (void)now;
}

static bool chorus_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_A)
        return false;
    if (b->pressed & UI_BTN_B) {
        joined = ! joined;
        if (! joined && song >= 0)
            stop_playing();
    }
    return true;
}

static bool chorus_task_page(absolute_time_t now) {
    static absolute_time_t ts = 0;
    if (absolute_time_diff_us(ts, now) < 1000000)
        return false;
    ts = now;
    return true;
}

static void chorus_render(uint8_t *fb, absolute_time_t now) {
    char text[64];
    ui_title(fb, N_("Choeur des cigales"));
    if (song < 0) {
        ui_lines(fb, 40, &gfx_font_small,
                 joined ? N_("En attente d'un chef de\nchoeur : quand il lance un\n"
                             "morceau, chaque cigale\nchante sa voix.") :
                 N_("Vous ne participez pas\naux choeurs."));
    } else {
        ui_lines(fb, 36, &gfx_font_medium, SONGS[song].name);
        snprintf(text, sizeof(text), _("Voix %d / %d%s"), voice + 1, SONGS[song].n_voices,
                 leader == net_id() ? _("  (chef)") : "");
        ui_lines(fb, 62, &gfx_font_small, text);
        int64_t since = absolute_time_diff_us(start_ts, now) / 1000;
        if (since < 0) {
            snprintf(text, sizeof(text), _("Départ dans %lld s"), (-since + 999) / 1000);
            ui_lines(fb, 90, &gfx_font_large, text);
        } else {
            uint32_t total = song_samples() / (RATE / 1000);
            ui_gauge(fb, 14, 96, GFX_WIDTH - 28, 16, since, total);
        }
    }
    ui_footer(fb, joined ? N_("A : retour  B : ne plus chanter") : N_("A : retour  B : participer"));
}

const app_t app_chorus = {
    .name = N_("Choeur"),
    .start = chorus_start,
    .buttons = chorus_buttons,
    .task = chorus_task_page,
    .render = chorus_render,
};

static void lead_start(absolute_time_t now) {
    (void)now;
}

static bool lead_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_A)
        return false;
    if (b->pressed & UI_BTN_Y)
        sel = (sel + N_SONGS - 1) % N_SONGS;
    if (b->pressed & UI_BTN_X)
        sel = (sel + 1) % N_SONGS;
    if (b->pressed & UI_BTN_B) {
        if (song >= 0 && leader == net_id()) {
            send_song(SONG_STOP, 0);
            stop_playing();
        } else {
            chorus_lead(sel);
        }
    }
    return true;
}

static void lead_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    ui_title(fb, N_("Choeur : lancer"));
    ui_list(fb, N_SONGS, sel, song_label);
    ui_lines(fb, 90, &gfx_font_small,
             N_("Les cigales autour chantent\nchacune une voix (départ\nsynchronisé par radio)."));
    ui_footer(fb, song >= 0 && leader == net_id() ? N_("A : retour  B : arrêter") : N_("A : retour  B : lancer"));
}

const app_t app_chorus_lead = {
    .name = N_("Choeur : lancer"),
    .start = lead_start,
    .buttons = lead_buttons,
    .render = lead_render,
};
