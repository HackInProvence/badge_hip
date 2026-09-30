/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Rock paper scissors between two badges ("Pierre-feuille-ciseaux", first to 3 rounds).
 * No cheating: each round, a badge first sends a commitment (hash of its choice and of a random number), and reveals
 * its choice only after it received the commitment of the other one; the reveal is checked against the commitment.
 * The radio loses packets: the current message is sent again every 500 ms until the other badge moves on.
 * NET_GAME [session 2][kind][to 4][round][...]: invite, accept, commit [hash 4], reveal [choice][nonce 4], bye. */

#include <stdio.h>
#include <string.h>

#include "pico/rand.h"

#include "app.h"
#include "net.h"
#include "social.h"

#define WIN_ROUNDS 3
#define RESEND_MS 500
#define LOST_MS 20000
#define RESULT_MS 2500

enum { K_INVITE = 1, K_ACCEPT, K_COMMIT, K_REVEAL, K_BYE };
enum { S_LOBBY, S_INVITING, S_INVITED, S_CHOOSE, S_WAIT, S_RESULT, S_END, S_LOST };
static const char *CHOICES[3] = {"Pierre", "Feuille", "Ciseaux"};

static int state = S_LOBBY;
static uint16_t session = 0;
static uint32_t peer = 0;
static char peer_name[9];
static uint8_t round_n = 0;
static int my_score = 0, peer_score = 0;
static int choice = 0;  /* Selected */
static int my_choice = -1;  /* Committed this round */
static uint32_t my_nonce = 0;
static bool peer_committed = false;
static uint32_t peer_commit = 0;
static int peer_choice = -1;  /* Revealed (and checked) this round */
static absolute_time_t resend_ts = 0, peer_seen = 0, result_ts = 0;
static bool changed = false;
/* Lobby */
static social_neighbour_t near[SOCIAL_MAX_NEIGHBOURS];
static int n_near = 0, sel = 0;
/* An invitation received while not playing */
static bool invited = false;
static uint32_t inviter = 0;
static uint16_t invite_session = 0;
static char inviter_name[9];

static uint32_t commitment(int c, uint32_t nonce, uint32_t id) {
    uint8_t d[12] = {c, round_n, session, session >> 8};
    net_put_u32(d + 4, nonce);
    net_put_u32(d + 8, id);
    uint32_t h = 2166136261u;  /* FNV-1a */
    for (int i = 0; i < 12; ++i)
        h = (h ^ d[i]) * 16777619u;
    return h;
}

static void send_kind(uint8_t kind, const uint8_t *extra, int n) {
    uint8_t d[16] = {session, session >> 8, kind};
    net_put_u32(d + 3, peer);
    d[7] = round_n;
    memcpy(d + 8, extra, n);
    net_send(NET_GAME, d, 8 + n, NET_MEDIUM);
}

static void new_round(void) {
    ++round_n;
    my_choice = -1;
    peer_choice = -1;
    peer_committed = false;
    state = S_CHOOSE;
    changed = true;
}

/* 1: I win, -1: the peer wins, 0: draw */
static int winner(int a, int b) {
    return a == b ? 0 : (a - b + 3) % 3 == 1 ? 1 : -1;
}

static void handle_game(const net_packet_t *p) {
    if (p->len < 8)
        return;
    const uint8_t *d = p->data;
    uint16_t sess = d[0] | d[1] << 8;
    uint8_t kind = d[2], round = d[7];
    if (net_u32(d + 3) != net_id())
        return;  /* Not for this badge */
    if (kind == K_INVITE) {
        if (state == S_LOBBY && ! invited) {
            invited = true;
            inviter = p->src;
            invite_session = sess;
            snprintf(inviter_name, sizeof(inviter_name), "%.*s", p->len > 8 ? p->len - 8 : 0, d + 8);
            printf("duel: invited by %s#%04X\n", inviter_name, (unsigned)(p->src & 0xFFFF));
            changed = true;
        }
        return;
    }
    if (p->src != peer || sess != session)
        return;
    peer_seen = p->at;
    switch (kind) {
    case K_ACCEPT:
        if (state == S_INVITING) {
            printf("duel: accepted\n");
            round_n = 0;
            new_round();
        }
        break;
    case K_COMMIT:
        if (state == S_INVITING) {
            /* The acceptation was lost, but the peer plays: accepted */
            printf("duel: accepted\n");
            round_n = 0;
            new_round();
        }
        if (round == round_n && p->len >= 12 && ! peer_committed) {
            peer_commit = net_u32(d + 8);
            peer_committed = true;
            changed = true;
        }
        break;
    case K_REVEAL:
        if (round == round_n && p->len >= 13 && peer_committed && peer_choice < 0) {
            int c = d[8];
            if (c < 3 && commitment(c, net_u32(d + 9), p->src) == peer_commit) {
                peer_choice = c;
                changed = true;
            } else {
                printf("duel: the reveal doesn't match the commitment!\n");
            }
        }
        break;
    case K_BYE:
        state = S_LOST;
        changed = true;
        break;
    }
}

void duel_init(void) {
    net_subscribe(NET_GAME, handle_game);
}

/* An invitation arrived: the main loop notifies it */
bool duel_invited(char *buf, int len) {
    static uint32_t notified = 0;
    if (! invited || notified == invite_session)
        return false;
    notified = invite_session;
    snprintf(buf, len, "%s vous défie : Social > Duel", inviter_name);
    return true;
}


/* ------ The page ------ */

static void near_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s#%04X  %d dBm", near[i].name, (unsigned)(near[i].id & 0xFFFF), near[i].rssi);
}

static void duel_start(absolute_time_t now) {
    (void)now;
    if (state != S_LOBBY && state != S_END && state != S_LOST)
        return;  /* A game in progress goes on */
    state = S_LOBBY;
    sel = 0;
    n_near = social_neighbours(near, SOCIAL_MAX_NEIGHBOURS);
}

static void start_game(uint32_t with, const char *name, uint16_t sess, absolute_time_t now) {
    peer = with;
    snprintf(peer_name, sizeof(peer_name), "%s", name);
    session = sess;
    my_score = peer_score = 0;
    round_n = 0;
    peer_seen = now;
    resend_ts = now;
}

static bool duel_buttons(const app_buttons_t *b, absolute_time_t now) {
    if (b->pressed & UI_BTN_A) {
        if (state != S_LOBBY && state != S_END && state != S_LOST) {
            send_kind(K_BYE, NULL, 0);
            printf("duel: left\n");
        }
        invited = false;
        state = S_LOBBY;
        return false;
    }
    switch (state) {
    case S_LOBBY:
        if (invited && (b->pressed & UI_BTN_B)) {
            /* Accept the invitation */
            start_game(inviter, inviter_name, invite_session, now);
            invited = false;
            send_kind(K_ACCEPT, NULL, 0);
            new_round();
            printf("duel: accepted the invitation\n");
            break;
        }
        if (n_near && (b->pressed & UI_BTN_Y))
            sel = (sel + n_near - 1) % n_near;
        if (n_near && (b->pressed & UI_BTN_X))
            sel = (sel + 1) % n_near;
        if (n_near && (b->pressed & UI_BTN_B)) {
            start_game(near[sel].id, near[sel].name, (get_rand_32() & 0xFFFE) + 1, now);
            state = S_INVITING;
            printf("duel: inviting %s\n", peer_name);
        }
        break;
    case S_CHOOSE:
        if (b->pressed & UI_BTN_Y)
            choice = (choice + 2) % 3;
        if (b->pressed & UI_BTN_X)
            choice = (choice + 1) % 3;
        if (b->pressed & UI_BTN_B) {
            my_choice = choice;
            my_nonce = get_rand_32();
            state = S_WAIT;
            resend_ts = now;
        }
        break;
    case S_END:
    case S_LOST:
        if (b->pressed & UI_BTN_B)
            duel_start(now);
        break;
    }
    return true;
}

static bool duel_task(absolute_time_t now) {
    if (state == S_LOBBY) {
        static absolute_time_t list_ts = 0;
        if (absolute_time_diff_us(list_ts, now) > 2000000) {
            list_ts = now;
            n_near = social_neighbours(near, SOCIAL_MAX_NEIGHBOURS);
            changed = true;
        }
    }
    if (state != S_LOBBY && state != S_END && state != S_LOST && state != S_INVITED
            && absolute_time_diff_us(peer_seen, now) > LOST_MS * 1000ll) {
        state = S_LOST;
        changed = true;
    }
    /* The current message, again and again */
    if (absolute_time_diff_us(resend_ts, now) >= 0) {
        resend_ts = delayed_by_ms(now, RESEND_MS);
        if (state == S_INVITING) {
            send_kind(K_INVITE, (const uint8_t *)social_name(), strnlen(social_name(), 8));
        } else if (state == S_CHOOSE && round_n == 1) {
            send_kind(K_ACCEPT, NULL, 0);  /* The inviter may have missed it */
        } else if (state == S_WAIT) {
            uint8_t c[4];
            net_put_u32(c, commitment(my_choice, my_nonce, net_id()));
            send_kind(K_COMMIT, c, 4);
            if (peer_committed) {
                uint8_t r[5] = {my_choice};
                net_put_u32(r + 1, my_nonce);
                send_kind(K_REVEAL, r, 5);
            }
        } else if (state == S_RESULT && my_choice >= 0) {
            uint8_t r[5] = {my_choice};  /* The peer may not have our reveal yet */
            net_put_u32(r + 1, my_nonce);
            send_kind(K_REVEAL, r, 5);
        }
    }
    /* Both revealed: the result of the round */
    if (state == S_WAIT && peer_choice >= 0) {
        int w = winner(my_choice, peer_choice);
        my_score += w > 0;
        peer_score += w < 0;
        printf("duel: round %u, %s vs %s: %s (%d-%d)\n", round_n, CHOICES[my_choice], CHOICES[peer_choice],
               w > 0 ? "won" : w < 0 ? "lost" : "draw", my_score, peer_score);
        app_tone(w > 0 ? 1319 : w < 0 ? 330 : 659, 150);
        state = S_RESULT;
        result_ts = now;
        changed = true;
    }
    if (state == S_RESULT && absolute_time_diff_us(result_ts, now) > RESULT_MS * 1000ll) {
        if (my_score >= WIN_ROUNDS || peer_score >= WIN_ROUNDS) {
            state = S_END;
            printf("duel: %s %d-%d\n", my_score > peer_score ? "victory" : "defeat", my_score, peer_score);
            changed = true;
        } else {
            new_round();
        }
    }
    bool c = changed;
    changed = false;
    return c;
}

static void duel_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    char text[64];
    if (state == S_LOBBY) {
        ui_title(fb, "Pierre-feuille-ciseaux");
        if (invited) {
            snprintf(text, sizeof(text), "%s vous défie !", inviter_name);
            ui_lines(fb, 60, &gfx_font_medium, text);
            ui_footer(fb, "G : refuser  D : accepter");
        } else if (n_near) {
            ui_list(fb, n_near, sel, near_label);
            ui_footer(fb, "G : retour  D : défier");
        } else {
            ui_lines(fb, 60, &gfx_font_small, "Aucune cigale à portée.\nApprochez-vous d'un autre\nbadge pour le défier.");
            ui_footer(fb, "G : retour");
        }
        return;
    }
    snprintf(text, sizeof(text), "Vous %d - %d %s", my_score, peer_score, peer_name);
    ui_title(fb, text);
    switch (state) {
    case S_INVITING:
        snprintf(text, sizeof(text), "Invitation envoyée\nà %s...", peer_name);
        ui_lines(fb, 60, &gfx_font_small, text);
        break;
    case S_CHOOSE:
        snprintf(text, sizeof(text), "Manche %u : votre choix ?", round_n);
        ui_lines(fb, UI_TITLE_H + 6, &gfx_font_small, text);
        for (int i = 0; i < 3; ++i) {
            int y = 62 + i * 34;
            if (i == choice) {
                gfx_fill_rect(fb, 20, y, GFX_WIDTH - 40, 30, GFX_BLACK);
                gfx_text(fb, GFX_WIDTH/2, y + 2, &gfx_font_large, CHOICES[i], GFX_WHITE, GFX_ALIGN_CENTER);
            } else {
                gfx_text(fb, GFX_WIDTH/2, y + 2, &gfx_font_large, CHOICES[i], GFX_BLACK, GFX_ALIGN_CENTER);
            }
        }
        break;
    case S_WAIT:
        snprintf(text, sizeof(text), "%s\n\nEn attente de %s...", CHOICES[my_choice], peer_name);
        ui_lines(fb, 60, &gfx_font_small, text);
        break;
    case S_RESULT: {
        int w = winner(my_choice, peer_choice);
        snprintf(text, sizeof(text), "%s contre %s", CHOICES[my_choice], CHOICES[peer_choice]);
        ui_lines(fb, 50, &gfx_font_small, text);
        gfx_text(fb, GFX_WIDTH/2, 85, &gfx_font_large, w > 0 ? "Gagné !" : w < 0 ? "Perdu..." : "Egalité",
                 GFX_BLACK, GFX_ALIGN_CENTER);
        break;
    }
    case S_END:
        ui_box(fb, my_score > peer_score ? "Victoire !" : "Défaite...");
        break;
    default:
        snprintf(text, sizeof(text), "%s est parti\n(ou hors de portée).", peer_name);
        ui_lines(fb, 60, &gfx_font_small, text);
        break;
    }
    ui_footer(fb, state == S_CHOOSE ? "Flancs : choix  D : jouer" : state >= S_END ? "G : quitter  D : rejouer" :
              "G : abandonner");
}

const app_t app_duel = {
    .name = "Duel",
    .start = duel_start,
    .buttons = duel_buttons,
    .task = duel_task,
    .render = duel_render,
};
