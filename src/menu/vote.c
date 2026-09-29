/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Votes of the audience: an admin badge opens a question (hard-coded list), the badges around vote,
 * the admin badge counts one vote per badge (the last one) and shows the histogram.
 * NET_VOTE_QUESTION [session 2][question][open]: sent every 3 s while the question is open.
 * NET_VOTE_ANSWER [session 2][question][answer]: the id of the badge is in the header of the packet. */

#include <stdio.h>
#include <string.h>

#include "pico/rand.h"

#include "app.h"
#include "net.h"

#define MAX_ANSWERS 5
#define MAX_VOTERS 256
#define QUESTION_PERIOD_MS 3000
#define ANSWER_REPEATS 3  /* A vote is sent 3 times (the room is noisy) */
#define ANSWER_REPEAT_MS 700

typedef struct {
    const char *text;
    uint8_t n;
    const char *answers[MAX_ANSWERS];
} question_t;

static const question_t QUESTIONS[] = {
    {"Ce talk vous a plu ?", 4, {"Oui !", "Assez", "Bof", "Non"}},
    {"Note du talk", 5, {"1", "2", "3", "4", "5"}},
    {"Niveau technique ?", 3, {"Trop simple", "Parfait", "Trop dur"}},
    {"Meilleur talk du jour ?", 4, {"Talk 1", "Talk 2", "Talk 3", "Talk 4"}},
    {"Pause : café ou thé ?", 3, {"Café", "Thé", "Rien"}},
    {"Revenir l'an prochain ?", 2, {"Oui !", "Non"}},
};
#define N_QUESTIONS ((int)(sizeof(QUESTIONS) / sizeof(QUESTIONS[0])))

/* ------ Voter side ------ */

static uint16_t heard_session = 0;  /* The question heard (0 = none) */
static uint8_t heard_question = 0;
static bool heard_open = false;
static absolute_time_t heard_ts = 0;
static int my_answer = -1;  /* Sent for heard_session */
static int choice = 0;
static int answer_repeats = 0;
static absolute_time_t answer_ts = 0;
static bool changed = false;
static bool fresh = false;  /* A new question was heard: notification */

static void handle_question(const net_packet_t *p) {
    if (p->len < 4 || p->data[2] >= N_QUESTIONS)
        return;
    uint16_t session = p->data[0] | p->data[1] << 8;
    bool open = p->data[3];
    if (session != heard_session) {
        my_answer = -1;
        choice = 0;
        answer_repeats = 0;
        printf("vote: question %u \"%s\"\n", p->data[2], QUESTIONS[p->data[2]].text);
        fresh = open;
    }
    if (session != heard_session || open != heard_open)
        changed = true;
    heard_session = session;
    heard_question = p->data[2];
    heard_open = open;
    heard_ts = p->at;
}

/* ------ Admin side ------ */

typedef struct {
    uint32_t id;
    uint8_t answer;
} voter_t;

static voter_t voters[MAX_VOTERS];
static int n_voters = 0;
static uint16_t my_session = 0;  /* Opened by this badge, 0 = none */
static uint8_t my_question = 0;
static absolute_time_t question_ts = 0;
static int admin_sel = 0;

static void handle_answer(const net_packet_t *p) {
    if (p->len < 4 || ! my_session)
        return;
    uint16_t session = p->data[0] | p->data[1] << 8;
    if (session != my_session || p->data[3] >= QUESTIONS[my_question].n)
        return;
    for (int i = 0; i < n_voters; ++i)
        if (voters[i].id == p->src) {
            if (voters[i].answer != p->data[3])
                changed = true;
            voters[i].answer = p->data[3];  /* A new vote replaces the previous one */
            return;
        }
    if (n_voters < MAX_VOTERS) {
        voters[n_voters++] = (voter_t){p->src, p->data[3]};
        printf("vote: %d voter(s)\n", n_voters);
        changed = true;
    }
}

static void vote_init(void) {
    static bool done = false;
    if (! done) {
        net_subscribe(NET_VOTE_QUESTION, handle_question);
        net_subscribe(NET_VOTE_ANSWER, handle_answer);
        done = true;
    }
}

/* Called by main.c in the loop: the questions and the answers are sent even when the vote page is closed */
void vote_task(absolute_time_t now) {
    vote_init();
    if (my_session && absolute_time_diff_us(question_ts, now) >= 0) {
        uint8_t data[4] = {my_session, my_session >> 8, my_question, 1};
        net_send(NET_VOTE_QUESTION, data, sizeof(data), NET_LOUD);
        question_ts = delayed_by_ms(now, QUESTION_PERIOD_MS);
    }
    if (answer_repeats && my_answer >= 0 && absolute_time_diff_us(answer_ts, now) >= 0) {
        uint8_t data[4] = {heard_session, heard_session >> 8, heard_question, my_answer};
        if (net_send(NET_VOTE_ANSWER, data, sizeof(data), NET_LOUD | NET_JITTER)) {
            --answer_repeats;
            answer_ts = delayed_by_ms(now, ANSWER_REPEAT_MS);
        }
    }
}

/* A new question was opened (notification) */
bool vote_new(void) {
    bool f = fresh;
    fresh = false;
    return f;
}

/* A question was heard recently and is open */
bool vote_open(void) {
    return heard_session && heard_open && absolute_time_diff_us(heard_ts, get_absolute_time()) < 15000000;
}


/* ------ The voter page ------ */

static void voter_start(absolute_time_t now) {
    (void)now;
    vote_init();
}

static bool voter_buttons(const app_buttons_t *b, absolute_time_t now) {
    if (b->pressed & UI_BTN_A)
        return false;
    if (! vote_open())
        return true;
    int n = QUESTIONS[heard_question].n;
    if (b->pressed & UI_BTN_Y)
        choice = (choice + n - 1) % n;
    if (b->pressed & UI_BTN_X)
        choice = (choice + 1) % n;
    if (b->pressed & UI_BTN_B) {
        my_answer = choice;
        answer_repeats = ANSWER_REPEATS;
        answer_ts = now;
        printf("vote: answer %d\n", choice);
    }
    return true;
}

static bool voter_task(absolute_time_t now) {
    (void)now;
    bool c = changed;
    changed = false;
    return c;
}

static void voter_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s%s", QUESTIONS[heard_question].answers[i], i == my_answer ? "   (mon vote)" : "");
}

static void voter_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    ui_title(fb, "Vote");
    if (! vote_open()) {
        ui_lines(fb, 60, &gfx_font_small, heard_session ? "Le vote est fermé.\nMerci !" :
                 "Aucun vote en cours.\nLes questions des\norganisateurs arrivent ici.");
        ui_footer(fb, "G : retour");
        return;
    }
    char fitted[48];
    ui_fit(&gfx_font_small, fitted, sizeof(fitted), QUESTIONS[heard_question].text, GFX_WIDTH - 6);
    gfx_text(fb, GFX_WIDTH/2, UI_TITLE_H + 3, &gfx_font_small, fitted, GFX_BLACK, GFX_ALIGN_CENTER);
    /* The answers below the question (a list shifted by one row) */
    const question_t *q = &QUESTIONS[heard_question];
    for (int i = 0; i < q->n; ++i) {
        int y = UI_TITLE_H + 24 + i * UI_ROW_H;
        char text[48];
        voter_label(i, text, sizeof(text));
        if (i == choice) {
            gfx_fill_rect(fb, 2, y, GFX_WIDTH - 4, UI_ROW_H - 1, GFX_BLACK);
            gfx_text(fb, 8, y + 1, &gfx_font_small, text, GFX_WHITE, GFX_ALIGN_LEFT);
        } else {
            gfx_text(fb, 8, y + 1, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_LEFT);
        }
    }
    ui_footer(fb, my_answer >= 0 ? "G : retour  D : changer mon vote" : "G : retour  D : voter");
}

const app_t app_vote = {
    .name = "Vote",
    .start = voter_start,
    .buttons = voter_buttons,
    .task = voter_task,
    .render = voter_render,
};


/* ------ The admin page: choose, open and close a question, histogram ------ */

static void admin_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s", QUESTIONS[i].text);
}

static void admin_start(absolute_time_t now) {
    (void)now;
    vote_init();
}

static bool admin_buttons(const app_buttons_t *b, absolute_time_t now) {
    if (b->pressed & UI_BTN_A)
        return false;  /* The question stays open (closed with the right wing) */
    if (my_session) {
        if (b->pressed & UI_BTN_B) {
            /* Close: tell it 3 times */
            uint8_t data[4] = {my_session, my_session >> 8, my_question, 0};
            for (int i = 0; i < 3; ++i)
                net_send(NET_VOTE_QUESTION, data, sizeof(data), NET_LOUD);
            printf("vote: closed, %d voter(s)\n", n_voters);
            my_session = 0;
        }
        return true;
    }
    if (b->pressed & UI_BTN_Y)
        admin_sel = (admin_sel + N_QUESTIONS - 1) % N_QUESTIONS;
    if (b->pressed & UI_BTN_X)
        admin_sel = (admin_sel + 1) % N_QUESTIONS;
    if (b->pressed & UI_BTN_B) {
        my_session = (get_rand_32() & 0xFFFE) + 1;
        my_question = admin_sel;
        n_voters = 0;
        question_ts = now;
        printf("vote: opened question %d\n", admin_sel);
    }
    return true;
}

static bool admin_task(absolute_time_t now) {
    (void)now;
    bool c = changed;
    changed = false;
    return c;
}

static void admin_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    if (! my_session) {
        ui_title(fb, "Vote : questions");
        ui_list(fb, N_QUESTIONS, admin_sel, admin_label);
        ui_footer(fb, "G : retour  D : ouvrir le vote");
        return;
    }
    /* Histogram */
    const question_t *q = &QUESTIONS[my_question];
    int counts[MAX_ANSWERS] = {0}, max = 1;
    for (int i = 0; i < n_voters; ++i)
        ++counts[voters[i].answer];
    for (int i = 0; i < q->n; ++i)
        if (counts[i] > max)
            max = counts[i];
    char text[48];
    snprintf(text, sizeof(text), "Vote : %d votant%s", n_voters, n_voters > 1 ? "s" : "");
    ui_title(fb, text);
    ui_fit(&gfx_font_small, text, sizeof(text), q->text, GFX_WIDTH - 6);
    gfx_text(fb, GFX_WIDTH/2, UI_TITLE_H + 3, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
    int row = (UI_FOOTER_Y - 4 - (UI_TITLE_H + 22)) / q->n;
    for (int i = 0; i < q->n; ++i) {
        int y = UI_TITLE_H + 22 + i * row;
        char label[16];
        ui_fit(&gfx_font_small, label, sizeof(label), q->answers[i], 58);
        gfx_text(fb, 3, y + (row - gfx_font_small.height) / 2, &gfx_font_small, label, GFX_BLACK, GFX_ALIGN_LEFT);
        int w = (GFX_WIDTH - 62 - 30) * counts[i] / max;
        gfx_fill_rect(fb, 62, y + 3, w ? w : 1, row - 6, GFX_BLACK);
        snprintf(label, sizeof(label), "%d", counts[i]);
        gfx_text(fb, 62 + w + 3, y + (row - gfx_font_small.height) / 2, &gfx_font_small, label, GFX_BLACK, GFX_ALIGN_LEFT);
    }
    ui_footer(fb, "G : retour  D : fermer le vote");
}

const app_t app_vote_admin = {
    .name = "Vote (admin)",
    .start = admin_start,
    .buttons = admin_buttons,
    .task = admin_task,
    .render = admin_render,
};
