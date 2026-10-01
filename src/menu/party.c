/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The lobby of the group games, see party.h. */

#include <stdio.h>
#include <string.h>

#include "pico/rand.h"

#include "net.h"
#include "party.h"
#include "social.h"

#define HEADER 8  /* game, session 2, kind, to 4 */
#define K_OPEN 1
#define K_JOIN 2
#define K_ROSTER 3
#define K_START 4
#define K_LEAVE PARTY_KIND_LEAVE
#define PER_PAGE 3  /* Players (id + name) in a ROSTER packet */
#define PAGES_PER_ROUND 4  /* ROSTER packets sent each second */
#define PERIOD_MS 1000
#define FOUND_TIMEOUT_MS 5000
#define START_REPEATS 5
#define START_GAP_MS 300
#define ROSTER_AFTER_START_MS 10000

static party_state_t state = PARTY_IDLE;
static uint8_t game = 0;
static uint16_t session = 0;
static uint32_t host = 0;
static bool host_plays = true;
static uint8_t max_players = PARTY_MAX;
static uint8_t flags = 0;
static uint32_t my_key = 0;
static party_player_t roster[PARTY_MAX];
static int n_roster = 0;
static uint64_t pages_got = 0;  /* Player: the ROSTER pages received */
static uint8_t pages_total = 0;
static absolute_time_t next_ts = 0, start_ts = 0, started_at = 0;
static int start_left = 0;
static int next_page = 0;
static uint32_t seed = 0;
static party_handler_t handler = NULL;

typedef struct {
    party_open_t pub;
    uint8_t game;
    absolute_time_t seen;
} found_t;
static found_t found[PARTY_MAX_OPEN];


uint32_t party_mask(uint32_t key, uint8_t salt) {
    uint32_t h = 2166136261u;  /* FNV-1a */
    uint8_t d[5] = {key, key >> 8, key >> 16, key >> 24, salt};
    for (int i = 0; i < 5; ++i)
        h = (h ^ d[i]) * 16777619u;
    return h;
}

static bool send_raw(uint8_t kind, uint32_t to, const void *data, uint8_t len) {
    uint8_t d[HEADER + PARTY_PAYLOAD] = {game, session, session >> 8, kind};
    net_put_u32(d + 4, to);
    if (len > PARTY_PAYLOAD)
        len = PARTY_PAYLOAD;
    memcpy(d + HEADER, data, len);
    return net_send(NET_PARTY, d, HEADER + len, NET_LOUD);
}

bool party_send(uint8_t kind, uint32_t to, const void *data, uint8_t len) {
    return send_raw(kind, to, data, len);
}

static int find(uint32_t id) {
    for (int i = 0; i < n_roster; ++i)
        if (roster[i].id == id)
            return i;
    return -1;
}

static void add(uint32_t id, const char *name, uint32_t key) {
    int i = find(id);
    if (i < 0) {
        if (n_roster >= max_players || n_roster >= PARTY_MAX)
            return;
        i = n_roster++;
        roster[i].id = id;
        printf("party: %s joined (%d players)\n", name, n_roster);
    }
    memcpy(roster[i].name, name, 8);
    roster[i].name[8] = 0;
    roster[i].key = key;
}

static void reset(void) {
    n_roster = 0;
    pages_got = 0;
    pages_total = 0;
    start_left = 0;
    next_page = 0;
}

/* ------ Host ------ */

void party_host(uint8_t g, bool plays, uint8_t max, uint8_t f) {
    party_leave();
    reset();
    game = g;
    session = get_rand_32();
    host = net_id();
    host_plays = plays;
    max_players = max > PARTY_MAX ? PARTY_MAX : max;
    flags = f;
    my_key = get_rand_32();
    if (plays)
        add(host, social_name(), my_key);
    state = PARTY_HOSTING;
    next_ts = get_absolute_time();
    printf("party: hosting game %u, session %04x\n", game, session);
}

void party_set_flags(uint8_t f) {
    flags = f;
}

void party_kick(uint32_t id) {
    int i = find(id);
    if (state != PARTY_HOSTING || i < 0 || id == host)
        return;
    memmove(&roster[i], &roster[i + 1], (n_roster - i - 1) * sizeof(roster[0]));
    --n_roster;
}

void party_start(uint16_t delay_ms) {
    if (state != PARTY_HOSTING)
        return;
    absolute_time_t now = get_absolute_time();
    seed = get_rand_32();
    start_ts = delayed_by_ms(now, delay_ms);
    started_at = now;
    start_left = START_REPEATS;
    next_ts = now;
    state = PARTY_STARTED;
    printf("party: start, %d players, seed %08lx\n", n_roster, (unsigned long)seed);
}

static void send_roster_pages(int count) {
    int pages = (n_roster + PER_PAGE - 1) / PER_PAGE;
    if (! pages)
        return;
    for (int k = 0; k < count && k < pages; ++k) {
        int page = next_page++ % pages;
        uint8_t d[3 + PER_PAGE * 12] = {page, pages, n_roster};
        int n = 0;
        for (int i = page * PER_PAGE; i < n_roster && n < PER_PAGE; ++i, ++n) {
            net_put_u32(d + 3 + n * 12, roster[i].id);
            memcpy(d + 3 + n * 12 + 4, roster[i].name, 8);
        }
        send_raw(K_ROSTER, 0, d, 3 + n * 12);
    }
}

/* ------ Player ------ */

void party_scan(uint8_t g) {
    party_leave();
    reset();
    game = g;
    memset(found, 0, sizeof(found));
    state = PARTY_SCANNING;
}

int party_found(party_open_t *out, int max) {
    absolute_time_t now = get_absolute_time();
    int n = 0;
    for (int i = 0; i < PARTY_MAX_OPEN && n < max; ++i)
        if (found[i].pub.host && found[i].game == game
                && absolute_time_diff_us(found[i].seen, now) < FOUND_TIMEOUT_MS * 1000ll)
            out[n++] = found[i].pub;
    for (int i = 1; i < n; ++i)  /* Closest first */
        for (int j = i; j > 0 && out[j].rssi > out[j - 1].rssi; --j) {
            party_open_t t = out[j];
            out[j] = out[j - 1];
            out[j - 1] = t;
        }
    return n;
}

void party_join(uint32_t h) {
    for (int i = 0; i < PARTY_MAX_OPEN; ++i)
        if (found[i].pub.host == h) {
            host = h;
            session = found[i].pub.session;
            flags = found[i].pub.flags;
            my_key = get_rand_32();
            reset();
            state = PARTY_JOINING;
            next_ts = get_absolute_time();
            printf("party: joining %s\n", found[i].pub.name);
            return;
        }
}

void party_leave(void) {
    if (state == PARTY_HOSTING || state == PARTY_JOINING || state == PARTY_JOINED || state == PARTY_STARTED)
        send_raw(K_LEAVE, 0, NULL, 0);
    state = PARTY_IDLE;
}

/* ------ Both ------ */

party_state_t party_state(void) { return state; }
uint8_t party_game(void) { return game; }
uint8_t party_flags(void) { return flags; }
bool party_is_host(void) { return host == net_id() && state != PARTY_IDLE; }
uint32_t party_host_id(void) { return host; }
int party_count(void) { return n_roster; }
int party_index(uint32_t id) { return find(id); }
uint32_t party_key(void) { return my_key; }
absolute_time_t party_start_time(void) { return start_ts; }
uint32_t party_seed(void) { return seed; }
void party_set_handler(party_handler_t h) { handler = h; }

int party_players(party_player_t *out, int max) {
    int n = n_roster < max ? n_roster : max;
    memcpy(out, roster, n * sizeof(roster[0]));
    return n;
}

const char *party_name(uint32_t id) {
    int i = find(id);
    return i >= 0 && roster[i].name[0] ? roster[i].name : "?";
}

static void handle(const net_packet_t *p) {
    if (p->len < HEADER || p->src == (net_id() ^ NET_TWIN))
        return;
    const uint8_t *d = p->data;
    uint8_t g = d[0], kind = d[3];
    uint16_t s = d[1] | d[2] << 8;
    uint32_t to = net_u32(d + 4);
    const uint8_t *x = d + HEADER;
    int n = p->len - HEADER;
    if (to && to != net_id())
        return;

    if (kind == K_OPEN && state == PARTY_SCANNING && g == game && n >= 11) {
        found_t *f = NULL, *oldest = &found[0];
        for (int i = 0; i < PARTY_MAX_OPEN && ! f; ++i) {
            if (found[i].pub.host == p->src)
                f = &found[i];
            else if (! found[i].pub.host || absolute_time_diff_us(found[i].seen, oldest->seen) > 0)
                oldest = &found[i];
        }
        if (! f)
            f = oldest;
        f->pub.host = p->src;
        f->pub.session = s;
        f->pub.players = x[0];
        f->pub.max = x[1];
        f->pub.flags = x[2];
        memcpy(f->pub.name, x + 3, 8);
        f->pub.name[8] = 0;
        f->pub.rssi = p->rssi;
        f->game = g;
        f->seen = p->at;
        return;
    }
    if (g != game || s != session || state == PARTY_IDLE || state == PARTY_SCANNING)
        return;

    switch (kind) {
    case K_JOIN:
        if (state == PARTY_HOSTING && n >= 12) {
            char name[9];
            memcpy(name, x + 4, 8);
            name[8] = 0;
            add(p->src, name, net_u32(x));
        }
        return;
    case K_ROSTER:
        if (p->src != host || state == PARTY_HOSTING || n < 3)
            return;
        if (x[1] && x[1] <= 64) {
            if (x[1] != pages_total || x[2] != n_roster) {  /* The roster changed: take it again */
                pages_total = x[1];
                pages_got = 0;
                n_roster = x[2] > PARTY_MAX ? PARTY_MAX : x[2];
                memset(roster, 0, sizeof(roster));
            }
            pages_got |= 1ull << x[0];
            for (int k = 0; k < PER_PAGE && 3 + (k + 1) * 12 <= n; ++k) {
                int i = x[0] * PER_PAGE + k;
                if (i >= n_roster)
                    break;
                roster[i].id = net_u32(x + 3 + k * 12);
                memcpy(roster[i].name, x + 3 + k * 12 + 4, 8);
                roster[i].name[8] = 0;
            }
            if (state == PARTY_JOINING && find(net_id()) >= 0) {
                state = PARTY_JOINED;
                printf("party: joined, %d players\n", n_roster);
            }
        }
        return;
    case K_START:
        if (p->src != host || n < 7 || (state != PARTY_JOINED && state != PARTY_JOINING))
            return;
        if (state == PARTY_JOINING && find(net_id()) < 0) {
            state = PARTY_CANCELLED;  /* Started without this badge */
            printf("party: started without this badge\n");
            return;
        }
        start_ts = delayed_by_ms(p->at, x[1] | x[2] << 8);
        seed = net_u32(x + 3);
        started_at = p->at;
        state = PARTY_STARTED;
        printf("party: started, %d players, seed %08lx\n", n_roster, (unsigned long)seed);
        return;
    case K_LEAVE:
        if (state == PARTY_STARTED) {
            if (handler)
                handler(kind, p->src, to, x, n, p->rssi);  /* The game decides (a player, or the host, left) */
        } else if (p->src == host && state != PARTY_HOSTING) {
            state = PARTY_CANCELLED;  /* The lobby closed before the start */
            printf("party: cancelled by the host\n");
        } else if (state == PARTY_HOSTING) {
            party_kick(p->src);
        }
        return;
    default:
        if (kind >= PARTY_KIND_GAME && handler && state == PARTY_STARTED)
            handler(kind, p->src, to, x, n, p->rssi);
        return;
    }
}

void party_task(absolute_time_t now) {
    if (state == PARTY_STARTED && start_left && absolute_time_diff_us(next_ts, now) >= 0) {
        int64_t left = absolute_time_diff_us(now, start_ts) / 1000;
        uint8_t d[7] = {n_roster, left > 0 ? left : 0, left > 0 ? left >> 8 : 0};
        net_put_u32(d + 3, seed);
        if (send_raw(K_START, 0, d, sizeof(d))) {
            --start_left;
            next_ts = delayed_by_ms(now, START_GAP_MS);
        }
        return;
    }
    if (absolute_time_diff_us(next_ts, now) < 0)
        return;
    next_ts = delayed_by_ms(now, PERIOD_MS);
    if (state == PARTY_HOSTING) {
        uint8_t d[11] = {n_roster, max_players, flags};
        memcpy(d + 3, social_name(), 8);
        send_raw(K_OPEN, 0, d, sizeof(d));
        send_roster_pages(PAGES_PER_ROUND);
    } else if (state == PARTY_STARTED && party_is_host()) {
        /* For the players who missed pages: all of them at first, then one per second during the game */
        send_roster_pages(absolute_time_diff_us(started_at, now) < ROSTER_AFTER_START_MS * 1000ll ? PAGES_PER_ROUND : 1);
        int64_t left = absolute_time_diff_us(now, start_ts) / 1000;  /* START again: a player may have missed all 5 */
        uint8_t d[7] = {n_roster, left > 0 ? left : 0, left > 0 ? left >> 8 : 0};
        net_put_u32(d + 3, seed);
        send_raw(K_START, 0, d, sizeof(d));
    } else if (state == PARTY_JOINING || (state == PARTY_JOINED && find(net_id()) < 0)) {
        uint8_t d[12];
        net_put_u32(d, my_key);
        memset(d + 4, ' ', 8);
        memcpy(d + 4, social_name(), strnlen(social_name(), 8));
        send_raw(K_JOIN, host, d, sizeof(d));
    }
}

void party_init(void) {
    reset();
    net_subscribe(NET_PARTY, handle);
}
