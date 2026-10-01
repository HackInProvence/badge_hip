/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Battleship between two badges ("Bataille navale"), in memory of the shipyards of La Ciotat.
 * A 6 x 6 sea, 3 ships (3, 2 and 2 cells) placed at random. The one who invites shoots first, then each one in turn.
 * No cheating: at the start each badge sends the hash of its fleet (with a random number), at the end it reveals
 * its fleet and the other one checks the hash and every "hit" / "miss" it answered.
 * NET_GAME [session 2][kind][to 4][turn][...], kinds 10 and more (duel.c gives them to battle_handle()):
 * invite [name], accept, fleet [hash 4], shot [cell], result [cell][hit], reveal [fleet 5][nonce 4]. */

#include <stdio.h>
#include <string.h>

#include "achievements.h"
#include "pico/rand.h"

#include "app.h"
#include "net.h"
#include "social.h"

#define SIZE 6
#define CELLS (SIZE * SIZE)
#define SHIP_CELLS 7
#define RESEND_MS 500
#define LOST_MS 60000
#define ALIVE_MS 3000  /* While one aims, the other one must know that it is still there */
#define REVEAL_MS 10000  /* The fleet is revealed again and again for this long at the end (lost packets) */

enum { K_INVITE = 10, K_ACCEPT, K_FLEET, K_SHOT, K_RESULT, K_REVEAL, K_BYE };
enum { S_LOBBY, S_INVITING, S_START, S_AIM, S_SHOOTING, S_WAITING, S_END, S_LOST };

static int state = S_LOBBY;
static uint16_t session = 0;
static uint32_t peer = 0;
static char peer_name[9];
static bool first = false;  /* This badge shoots first */
static uint64_t fleet = 0;  /* Bit per cell: my ships */
static uint32_t nonce = 0;
static uint64_t hit_me = 0;  /* Cells of my sea shot by the peer */
static uint64_t shot_peer = 0, hit_peer = 0;  /* My shots and hits on the peer */
static bool peer_hashed = false;
static uint32_t peer_hash = 0;
static uint64_t peer_answers_hit = 0, peer_answers = 0;  /* What the peer answered (to check at the end) */
static int cursor = 0;
static uint8_t turn = 0;  /* Number of shots so far (both) */
static int pending_shot = -1;  /* My shot waiting for its result */
static int last_shot_in = -1;  /* The last shot of the peer (to answer again) */
static bool last_hit_in = false;
static bool revealed = false, peer_revealed = false, cheat = false;
static absolute_time_t resend_ts = 0, peer_seen = 0, alive_ts = 0, end_ts = 0;
static bool changed = false;
static social_neighbour_t near[SOCIAL_MAX_NEIGHBOURS];
static int n_near = 0, sel = 0;
static bool invited = false;
static uint32_t inviter = 0;
static uint16_t invite_session = 0;
static char inviter_name[9];

static int count(uint64_t v) {
    int n = 0;
    for (; v; v &= v - 1)
        ++n;
    return n;
}

static uint32_t hash_fleet(uint64_t f, uint32_t n, uint32_t id) {
    uint8_t d[16];
    for (int i = 0; i < 8; ++i)
        d[i] = f >> (8 * i);
    net_put_u32(d + 8, n);
    net_put_u32(d + 12, id);
    uint32_t h = 2166136261u;
    for (int i = 0; i < 16; ++i)
        h = (h ^ d[i]) * 16777619u;
    return (h ^ session) * 16777619u;
}

/* Random fleet: a ship of 3 and two of 2, not touching the others */
static void place_fleet(void) {
    static const int LENGTHS[3] = {3, 2, 2};
    do {
        fleet = 0;
        int placed = 0;
        for (int tries = 0; placed < 3 && tries < 200; ++tries) {
            int len = LENGTHS[placed], horizontal = get_rand_32() & 1;
            int x = get_rand_32() % (horizontal ? SIZE - len + 1 : SIZE), y = get_rand_32() % (horizontal ? SIZE : SIZE - len + 1);
            uint64_t ship = 0, around = 0;
            for (int k = 0; k < len; ++k) {
                int cx = x + (horizontal ? k : 0), cy = y + (horizontal ? 0 : k);
                ship |= 1ull << (cy * SIZE + cx);
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        int ax = cx + dx, ay = cy + dy;
                        if (ax >= 0 && ax < SIZE && ay >= 0 && ay < SIZE)
                            around |= 1ull << (ay * SIZE + ax);
                    }
            }
            if (! (around & fleet)) {
                fleet |= ship;
                ++placed;
            }
        }
    } while (count(fleet) != SHIP_CELLS);
    nonce = get_rand_32();
}

static void send_kind(uint8_t kind, const uint8_t *extra, int n) {
    uint8_t d[20] = {session, session >> 8, kind};
    net_put_u32(d + 3, peer);
    d[7] = turn;
    memcpy(d + 8, extra, n);
    net_send(NET_GAME, d, 8 + n, NET_LOUD);
}

static bool my_turn(void) {
    return (turn % 2 == 0) == first;
}

static void finish(void) {
    state = S_END;
    end_ts = get_absolute_time();
    printf("battle: %s (%d-%d)\n", count(hit_peer) == SHIP_CELLS ? "victory" : "defeat", count(hit_peer),
           count(hit_me & fleet));
    if (count(hit_peer) == SHIP_CELLS) {
        achv_unlock(ACHV_BATTLE_WIN);
        achv_add(ACHV_CNT_WINS, 1);
    }
    changed = true;
}

void battle_handle(const net_packet_t *p) {
    const uint8_t *d = p->data;
    uint16_t sess = d[0] | d[1] << 8;
    uint8_t kind = d[2], t = d[7];
    if (kind == K_INVITE) {
        if (state == S_LOBBY && ! invited) {
            invited = true;
            inviter = p->src;
            invite_session = sess;
            snprintf(inviter_name, sizeof(inviter_name), "%.*s", p->len > 8 ? p->len - 8 : 0, d + 8);
            printf("battle: invited by %s\n", inviter_name);
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
            state = S_START;
            changed = true;
        }
        break;
    case K_FLEET:
        if (state == S_INVITING)
            state = S_START;
        if (! peer_hashed && p->len >= 12) {
            peer_hash = net_u32(d + 8);
            peer_hashed = true;
            if (state == S_START)
                state = my_turn() ? S_AIM : S_WAITING;
            changed = true;
        }
        break;
    case K_SHOT:
        if (p->len >= 9 && d[8] < CELLS) {
            int cell = d[8];
            if (t == turn && ! my_turn() && state != S_END) {
                /* The peer shoots: answer, then it's my turn */
                last_shot_in = cell;
                last_hit_in = (fleet >> cell) & 1;
                hit_me |= 1ull << cell;
                ++turn;
                if (state == S_START)
                    state = S_WAITING;
                if (count(hit_me & fleet) == SHIP_CELLS)
                    finish();
                else
                    state = S_AIM;
                changed = true;
            }
            if (cell == last_shot_in) {
                uint8_t r[2] = {cell, last_hit_in};
                send_kind(K_RESULT, r, 2);
            }
        }
        break;
    case K_RESULT:
        if (p->len >= 10 && pending_shot >= 0 && d[8] == pending_shot) {
            shot_peer |= 1ull << pending_shot;
            peer_answers |= 1ull << pending_shot;
            if (d[9]) {
                hit_peer |= 1ull << pending_shot;
                peer_answers_hit |= 1ull << pending_shot;
            }
            app_tone(d[9] ? 1319 : 330, 120);
            printf("battle: shot %d %s\n", pending_shot, d[9] ? "hit" : "miss");
            pending_shot = -1;
            ++turn;
            for (int k = 1; k <= CELLS && ((shot_peer >> cursor) & 1); ++k)
                cursor = (cursor + 1) % CELLS;  /* The aim goes to the next cell not shot */
            if (count(hit_peer) == SHIP_CELLS)
                finish();
            else
                state = S_WAITING;
            changed = true;
        }
        break;
    case K_REVEAL:
        if (p->len >= 17 && ! peer_revealed) {
            uint64_t f = 0;
            for (int i = 0; i < 5; ++i)
                f |= (uint64_t)d[8 + i] << (8 * i);
            uint32_t n = net_u32(d + 13);
            /* The fleet matches its hash, and every answer was right */
            cheat = hash_fleet(f, n, p->src) != peer_hash || (f & peer_answers) != peer_answers_hit
                    || count(f) != SHIP_CELLS;
            peer_revealed = true;
            printf("battle: the fleet of %s is %s\n", peer_name, cheat ? "WRONG (cheat!)" : "checked");
            changed = true;
        }
        break;
    case K_BYE:
        state = S_LOST;
        changed = true;
        break;
    }
}

/* ------ The page ------ */

static void near_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s#%04X  %d dBm", near[i].name, (unsigned)(near[i].id & 0xFFFF), near[i].rssi);
}

static void start_game(uint32_t with, const char *name, uint16_t sess, bool me_first, absolute_time_t now) {
    peer = with;
    snprintf(peer_name, sizeof(peer_name), "%s", name);
    session = sess;
    first = me_first;
    place_fleet();
    hit_me = shot_peer = hit_peer = peer_answers = peer_answers_hit = 0;
    peer_hashed = revealed = peer_revealed = cheat = false;
    turn = 0;
    cursor = 0;
    pending_shot = last_shot_in = -1;
    peer_seen = now;
    resend_ts = now;
}

static void battle_start(absolute_time_t now) {
    (void)now;
    if (state != S_LOBBY && state != S_END && state != S_LOST)
        return;
    state = S_LOBBY;
    n_near = social_neighbours(near, SOCIAL_MAX_NEIGHBOURS);
    sel = 0;
}

static bool battle_buttons(const app_buttons_t *b, absolute_time_t now) {
    if (b->pressed & UI_BTN_A) {
        if (state != S_LOBBY && state != S_END && state != S_LOST)
            send_kind(K_BYE, NULL, 0);
        invited = false;
        state = S_LOBBY;
        return false;
    }
    switch (state) {
    case S_LOBBY:
        if (invited && (b->pressed & UI_BTN_B)) {
            start_game(inviter, inviter_name, invite_session, false, now);
            invited = false;
            send_kind(K_ACCEPT, NULL, 0);
            state = S_START;
            printf("battle: accepted\n");
            break;
        }
        if (n_near && (b->pressed & UI_BTN_Y))
            sel = (sel + n_near - 1) % n_near;
        if (n_near && (b->pressed & UI_BTN_X))
            sel = (sel + 1) % n_near;
        if (n_near && (b->pressed & UI_BTN_B)) {
            start_game(near[sel].id, near[sel].name, (get_rand_32() & 0xFFFE) + 1, true, now);
            state = S_INVITING;
            printf("battle: inviting %s\n", peer_name);
        }
        break;
    case S_AIM:
        /* The flanks move the aim (left / right, the rows follow), the right wing fires */
        if (b->pressed & UI_BTN_Y)
            cursor = (cursor + CELLS - 1) % CELLS;
        if (b->pressed & UI_BTN_X)
            cursor = (cursor + 1) % CELLS;
        if ((b->pressed & UI_BTN_B) && ! ((shot_peer >> cursor) & 1)) {
            pending_shot = cursor;
            state = S_SHOOTING;
            resend_ts = now;
        }
        break;
    case S_END:
    case S_LOST:
        if (b->pressed & UI_BTN_B)
            battle_start(now);
        break;
    }
    return true;
}

static bool battle_task(absolute_time_t now) {
    if (state == S_LOBBY) {
        static absolute_time_t list_ts = 0;
        if (absolute_time_diff_us(list_ts, now) > 2000000) {
            list_ts = now;
            n_near = social_neighbours(near, SOCIAL_MAX_NEIGHBOURS);
            changed = true;
        }
    } else if (state != S_END && state != S_LOST && absolute_time_diff_us(peer_seen, now) > LOST_MS * 1000ll) {
        state = S_LOST;
        changed = true;
    }
    if (absolute_time_diff_us(resend_ts, now) >= 0) {
        resend_ts = delayed_by_ms(now, RESEND_MS);
        uint8_t h[4];
        switch (state) {
        case S_INVITING:
            send_kind(K_INVITE, (const uint8_t *)social_name(), strnlen(social_name(), 8));
            break;
        case S_START:
        case S_AIM:
        case S_WAITING:
            if (! peer_hashed || turn < 2 || absolute_time_diff_us(alive_ts, now) >= 0) {
                net_put_u32(h, hash_fleet(fleet, nonce, net_id()));
                send_kind(K_FLEET, h, 4);  /* Until the game really started, then to say "still there" */
                alive_ts = delayed_by_ms(now, ALIVE_MS);
            }
            if (state == S_START && peer_hashed)
                state = my_turn() ? S_AIM : S_WAITING;
            if (last_shot_in >= 0 && state != S_AIM) {
                uint8_t r[2] = {last_shot_in, last_hit_in};  /* The peer may not have my answer */
                send_kind(K_RESULT, r, 2);
            }
            break;
        case S_SHOOTING: {
            uint8_t c = pending_shot;
            send_kind(K_SHOT, &c, 1);
            break;
        }
        case S_END:
            if (absolute_time_diff_us(end_ts, now) < REVEAL_MS * 1000ll) {
                uint8_t r[9];
                for (int i = 0; i < 5; ++i)
                    r[i] = fleet >> (8 * i);
                net_put_u32(r + 5, nonce);
                send_kind(K_REVEAL, r, 9);
                revealed = true;
                if (last_shot_in >= 0) {
                    uint8_t a[2] = {last_shot_in, last_hit_in};
                    send_kind(K_RESULT, a, 2);
                }
            }
            break;
        }
    }
    bool c = changed;
    changed = false;
    return c;
}

static void draw_sea(uint8_t *fb, int x0, int y0, int cell, uint64_t ships, uint64_t shots, uint64_t hits, int aim) {
    gfx_rect(fb, x0 - 1, y0 - 1, SIZE * cell + 2, SIZE * cell + 2, GFX_BLACK);
    for (int i = 0; i < CELLS; ++i) {
        int x = x0 + (i % SIZE) * cell, y = y0 + (i / SIZE) * cell;
        gfx_rect(fb, x, y, cell, cell, GFX_BLACK);
        if ((ships >> i) & 1)
            gfx_fill_rect(fb, x + 2, y + 2, cell - 4, cell - 4, GFX_BLACK);
        if ((hits >> i) & 1) {
            /* Hit: a cross */
            for (int k = 2; k < cell - 2; ++k) {
                gfx_pixel(fb, x + k, y + k, GFX_INVERT);
                gfx_pixel(fb, x + cell - 1 - k, y + k, GFX_INVERT);
            }
            if (! ((ships >> i) & 1))
                gfx_fill_rect(fb, x + 3, y + 3, cell - 6, cell - 6, GFX_BLACK);
        } else if ((shots >> i) & 1) {
            gfx_fill_rect(fb, x + cell / 2 - 1, y + cell / 2 - 1, 3, 3, GFX_BLACK);  /* Miss: a dot */
        }
        if (i == aim)
            gfx_rect(fb, x + 1, y + 1, cell - 2, cell - 2, GFX_INVERT);
    }
}

static void battle_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    char text[48];
    if (state == S_LOBBY) {
        ui_title(fb, "Bataille navale");
        if (invited) {
            snprintf(text, sizeof(text), "%s vous défie !", inviter_name);
            ui_lines(fb, 60, &gfx_font_medium, text);
            ui_footer(fb, "G : refuser  D : accepter");
        } else if (n_near) {
            ui_list(fb, n_near, sel, near_label);
            ui_footer(fb, "G : retour  D : défier");
        } else {
            ui_lines(fb, 60, &gfx_font_small, "Aucune cigale à portée.");
            ui_footer(fb, "G : retour");
        }
        return;
    }
    snprintf(text, sizeof(text), "Touchés %d - %d", count(hit_peer), count(hit_me & fleet));
    ui_title(fb, text);
    /* The sea of the peer (where I shoot), and mine smaller */
    draw_sea(fb, 4, UI_TITLE_H + 6, 21, 0, shot_peer, hit_peer, state == S_AIM ? cursor : -1);
    draw_sea(fb, 136, UI_TITLE_H + 6, 10, fleet, hit_me, hit_me & fleet, -1);
    ui_text(fb, 136, UI_TITLE_H + 72, &gfx_font_small, "Ma flotte");
    const char *status = state == S_INVITING ? "Invitation..." : state == S_START ? "Préparation..." :
                         state == S_AIM ? "A vous !" : state == S_SHOOTING ? "Tir..." :
                         state == S_WAITING ? "Il vise..." : state == S_LOST ? "Adversaire parti" :
                         count(hit_peer) == SHIP_CELLS ? "Victoire !" : "Défaite...";
    ui_text(fb, 136, UI_TITLE_H + 92, &gfx_font_small, status);
    if (state == S_END && peer_revealed)
        ui_text(fb, 136, UI_TITLE_H + 110, &gfx_font_small, cheat ? "TRICHE !" : "Flotte OK");
    ui_footer(fb, state == S_AIM ? "Flancs : viser  D : tirer" : state >= S_END ? "G : quitter  D : rejouer" :
              "G : abandonner");
}

/* The invitation, for the notification */
bool battle_invited(char *buf, int len) {
    static uint16_t notified = 0;
    if (! invited || notified == invite_session)
        return false;
    notified = invite_session;
    snprintf(buf, len, "%s : bataille navale ?", inviter_name);
    return true;
}

const app_t app_battle = {
    .name = "Bataille navale",
    .start = battle_start,
    .buttons = battle_buttons,
    .task = battle_task,
    .render = battle_render,
};
