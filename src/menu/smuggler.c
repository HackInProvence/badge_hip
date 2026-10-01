/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The smuggler cicada ("Contrebande", Social theme): rare virtual goods, traded discreetly between two badges held
 * against each other. See docs/fr/contrebande.md.
 *
 * - The goods (smuggler_goods.c): food and drinks of Provence, spices, treasures and three legendary ones, each with
 *   an icon. The cargo is kept in the store (store_t.cargo, cargo_seeded).
 * - The first opening fills the cargo with a few common goods; then each new cicada met may leave a good in the hold
 *   (smuggler_task(), announced discreetly).
 * - Échanger / Donner: only with the cicadas "à portée de main" (RSSI >= SMUGGLER_TRADE_RSSI), through the protocol
 *   of smuggler_trade.c (two-phase commit: no good is created nor lost by a lost packet). Dark page, no sound.
 * - Cale (the cargo), Collection (every good, the unknown ones as silhouettes), Fortune (value in doublons). */

#include <stdio.h>
#include <string.h>

#include "pico/rand.h"

#include "achievements.h"
#include "app.h"
#include "net.h"
#include "smuggler_goods.h"
#include "smuggler_trade.h"
#include "social.h"
#include "store.h"

/* The peer must be "à portée de main": its beacons (+10 dBm) received at least this strong. ~-70 to -83 dBm at 1 m
 * (social.h): -55 dBm is a few centimeters, to calibrate on site (Social > Radar shows the RSSI). */
#ifndef SMUGGLER_TRADE_RSSI
#define SMUGGLER_TRADE_RSSI (-55)
#endif
#define SMUGGLER_RSSI_MARGIN 10  /* An invitation is still heard a bit weaker (the RSSI varies from packet to packet) */
#define SMUGGLER_SEED_GOODS 4  /* Common goods of the first opening */
#define SMUGGLER_FIND_PERCENT 50  /* Chance of a good in the hold at each new cicada met */
#define NEAR_REFRESH_MS 1000

_Static_assert(SMUGGLER_GOODS <= STORE_CARGO_ITEMS, "store_t.cargo is too small for the goods");

extern const app_t app_smuggler;

static trade_t trade;
static bool ready = false;
static uint16_t last_met = 0;
static bool dirty = false;  /* The page must be redrawn */
static char event[48];  /* A discreet message for the main loop (smuggler_event()) */
static bool event_pending = false;
static uint32_t notified = 0;  /* The invitation already notified */

static uint8_t *cargo(void) {
    return store_get()->cargo;
}

static void set_event(const char *fmt, const char *what) {
    snprintf(event, sizeof(event), fmt, what);
    event_pending = true;
}


/* ------ The services: radio, goods found in the hold ------ */

static void send_packet(const uint8_t *data, uint8_t len) {
    if (! net_send(NET_TRADE, data, len, NET_LOUD))
        printf("smuggler: error: radio queue full\n");
}

static void handle_trade(const net_packet_t *p) {
    trade_receive(&trade, p->src, p->data, p->len, p->rssi, to_ms_since_boot(p->at));
}

static void cargo_changed(void) {
    store_changed();
    dirty = true;
}

/* A good arrived in the cargo: the achievements */
static void obtained(uint8_t good) {
    if (good < SMUGGLER_GOODS && smuggler_goods[good].rarity == RARITY_LEGENDARY)
        achv_unlock(ACHV_TRADE_RARE);
    if (cargo_seen_kinds(cargo()) == SMUGGLER_GOODS)
        achv_unlock(ACHV_CARGO_FULL);
}

static void finished(trade_t *t, uint8_t gave, uint8_t got) {
    (void)gave;
    achv_unlock(ACHV_TRADE);
    achv_add(ACHV_CNT_TRADES, 1);
    obtained(got);
    if (app_current() != &app_smuggler && got < SMUGGLER_GOODS)
        set_event("Reçu en douce : %s", smuggler_goods[got].name);  /* A sealed trade ended in the background */
    (void)t;
}

void smuggler_init(void) {
    if (ready)
        return;
    trade_init(&trade, net_id(), cargo(), send_packet, cargo_changed, finished);
    trade.invite_rssi_min = SMUGGLER_TRADE_RSSI - SMUGGLER_RSSI_MARGIN;
    net_subscribe(NET_TRADE, handle_trade);
    last_met = social_met_count();
    ready = true;
}

/* A good in the hold, at each new cicada met (social_met_count() increases) */
static void meetings(void) {
    uint16_t met = social_met_count();
    if (met < last_met)
        last_met = met;  /* Reset (Remise à zéro) */
    while (last_met < met) {
        ++last_met;
        if (get_rand_32() % 100 >= SMUGGLER_FIND_PERCENT)
            continue;
        int good = smuggler_random_good(get_rand_32(), get_rand_32(), RARITY_LEGENDARY);
        if (! cargo_add(cargo(), good))
            continue;
        store_changed();
        dirty = true;
        printf("smuggler: found %s (%s) in the hold\n", smuggler_goods[good].name,
               smuggler_rarity_name(smuggler_goods[good].rarity));
        set_event("Dans la cale : %s", smuggler_goods[good].name);
        obtained(good);
    }
}

void smuggler_task(absolute_time_t now) {
    if (! ready)
        return;
    trade_task(&trade, to_ms_since_boot(now));
    if (trade.changed) {
        trade.changed = false;
        dirty = true;
    }
    meetings();
}

/* An invitation arrived (once each): the main loop notifies it */
bool smuggler_invited(char *buf, int len) {
    if (! ready || ! trade.invited || trade_busy(&trade) || notified == trade.inv_id)
        return false;
    notified = trade.inv_id;
    snprintf(buf, len, "Psst... %s : Contrebande", trade.inv_name);
    return true;
}

/* A discreet message (a good found in the hold, a sealed trade ended), once: a status, without sound */
bool smuggler_event(char *buf, int len) {
    if (! event_pending)
        return false;
    event_pending = false;
    snprintf(buf, len, "%s", event);
    return true;
}

/* The first opening: a few common goods to start with */
static void seed(void) {
    store_t *s = store_get();
    if (s->cargo_seeded == 1)
        return;
    char names[96] = "";
    for (int i = 0; i < SMUGGLER_SEED_GOODS; ++i) {
        int good = smuggler_random_good(get_rand_32(), get_rand_32(), RARITY_COMMON);
        cargo_add(s->cargo, good);
        snprintf(names + strlen(names), sizeof(names) - strlen(names), "%s%s", i ? ", " : "",
                 smuggler_goods[good].name);
    }
    s->cargo_seeded = 1;
    store_changed();
    printf("smuggler: seeded the cargo: %s\n", names);
}


/* ------ The pages ------ */

enum { P_HOME, P_CARGO, P_COLLECTION, P_DETAIL, P_FORTUNE, P_GIFT_PICK, P_NEAR, P_TRADE };
static const char *PAGE_NAMES[] = {"home", "cargo", "collection", "detail", "fortune", "gift", "near", "trade"};
enum { H_CARGO, H_TRADE, H_GIFT, H_COLLECTION, H_FORTUNE, H_ROWS };

static int page = P_HOME;
static int home_sel = 0, grid_sel = 0;
static int detail_good = 0, detail_back = P_HOME;
static uint8_t near_mode = TRADE_EXCHANGE, gift_good = SMUGGLER_NO_GOOD;
static social_neighbour_t near[SOCIAL_MAX_NEIGHBOURS];
static int n_near = 0, n_far = 0, near_sel = 0;
static absolute_time_t near_ts = 0;

static void set_page(int p) {
    if (p != page)
        printf("smuggler: page %s\n", PAGE_NAMES[p]);
    page = p;
    dirty = true;
}

static void refresh_near(void) {
    social_neighbour_t all[SOCIAL_MAX_NEIGHBOURS];
    int n = social_neighbours(all, SOCIAL_MAX_NEIGHBOURS);
    uint32_t selected = near_sel < n_near ? near[near_sel].id : 0;
    int before = n_near;
    n_near = n_far = 0;
    for (int i = 0; i < n; ++i) {
        if (all[i].rssi >= SMUGGLER_TRADE_RSSI)
            near[n_near++] = all[i];
        else
            ++n_far;
    }
    near_sel = 0;
    for (int i = 0; i < n_near; ++i)
        if (near[i].id == selected)
            near_sel = i;
    if (n_near != before) {
        printf("smuggler: %d at hand, %d farther\n", n_near, n_far);
        for (int i = 0; i < n_near; ++i)
            printf("smuggler: at hand %s#%04X %d dBm\n", near[i].name, (unsigned)(near[i].id & 0xFFFF), near[i].rssi);
    }
}

static bool showing_invite(void) {
    return trade.invited && ! trade_busy(&trade);
}

/* The good of the cell \p i of a grid of the current page */
static int grid_good(int i) {
    return page == P_COLLECTION || page == P_DETAIL ? i : cargo_nth(cargo(), i);
}

static int grid_count(void) {
    return page == P_COLLECTION ? SMUGGLER_GOODS : cargo_kinds(cargo());
}

static void start(absolute_time_t now) {
    (void)now;
    smuggler_init();  /* Done at boot, or here */
    seed();
    if (trade_busy(&trade) || trade_final(&trade)) {
        set_page(P_TRADE);
    } else {
        page = -1;
        set_page(P_HOME);
    }
    home_sel = 0;
    dirty = true;
}

/* Leaving: a trade in progress is cancelled (a sealed one goes on in the background) */
static void stop(void) {
    if (trade_cancellable(&trade))
        trade_cancel(&trade, to_ms_since_boot(get_absolute_time()));
}

static void move(int *sel, int n, const app_buttons_t *b) {
    if (n <= 0)
        return;
    if (b->pressed & UI_BTN_Y)
        *sel = (*sel + n - 1) % n;
    if (b->pressed & UI_BTN_X)
        *sel = (*sel + 1) % n;
    if (*sel >= n)
        *sel = n - 1;
}

static void open_detail(int good, int back) {
    detail_good = good;
    detail_back = back;
    set_page(P_DETAIL);
}

static void trade_buttons(const app_buttons_t *b, bool back, uint32_t ms) {
    bool ok = b->pressed & UI_BTN_B;
    switch (trade.state) {
    case TS_CHOOSE: {
        int n = cargo_kinds(cargo());
        move(&grid_sel, n, b);
        if (ok && n)
            trade_offer(&trade, cargo_nth(cargo(), grid_sel), ms);
        if (back)
            trade_cancel(&trade, ms);
        break;
    }
    case TS_REVIEW:
        if (ok && ! trade_confirm(&trade, ms))
            printf("smuggler: error: cannot confirm\n");
        if (back)
            trade_cancel(&trade, ms);
        break;
    case TS_INVITING:
    case TS_OFFERED:
    case TS_CONFIRMED:
        if (back) {
            if (trade_cancellable(&trade))
                trade_cancel(&trade, ms);
            else
                set_page(P_HOME);  /* Sealed: the trade ends in the background */
        }
        break;
    case TS_IDLE:
        if (back || ok)
            set_page(P_HOME);
        break;
    default:  /* The end */
        if (back || ok) {
            trade_close(&trade);
            set_page(P_HOME);
        }
        break;
    }
}

static bool buttons(const app_buttons_t *b, absolute_time_t now) {
    uint32_t ms = to_ms_since_boot(now);
    bool back = b->released_short & UI_BTN_A;
    bool ok = b->pressed & UI_BTN_B;
    dirty = true;
    if (b->long_pressed & UI_BTN_A)
        return false;  /* stop() cancels a trade in progress */
    if (showing_invite()) {
        if (ok && trade_accept(&trade, ms)) {
            grid_sel = 0;
            set_page(P_TRADE);
        } else if (back) {
            trade_refuse(&trade);
        }
        return true;
    }
    switch (page) {
    case P_HOME:
        move(&home_sel, H_ROWS, b);
        if (back)
            return false;
        if (ok) {
            grid_sel = 0;
            near_sel = 0;
            switch (home_sel) {
            case H_CARGO: set_page(P_CARGO); break;
            case H_TRADE: near_mode = TRADE_EXCHANGE; n_near = 0; set_page(P_NEAR); refresh_near(); break;
            case H_GIFT: set_page(P_GIFT_PICK); break;
            case H_COLLECTION: set_page(P_COLLECTION); break;
            default: set_page(P_FORTUNE); break;
            }
        }
        break;
    case P_CARGO:
    case P_COLLECTION:
        move(&grid_sel, grid_count(), b);
        if (ok && grid_count())
            open_detail(grid_good(grid_sel), page);
        if (back)
            set_page(P_HOME);
        break;
    case P_DETAIL:
        if (back || ok)
            set_page(detail_back);
        break;
    case P_GIFT_PICK:
        move(&grid_sel, grid_count(), b);
        if (ok && grid_count()) {
            gift_good = cargo_nth(cargo(), grid_sel);
            near_mode = TRADE_GIFT;
            n_near = 0;
            set_page(P_NEAR);
            refresh_near();
        }
        if (back)
            set_page(P_HOME);
        break;
    case P_NEAR:
        move(&near_sel, n_near, b);
        if (ok && n_near) {
            snprintf(trade.my_name, sizeof(trade.my_name), "%.8s", social_name());
            uint32_t id = get_rand_32();
            if (trade_invite(&trade, near[near_sel].id, near[near_sel].name, near_mode, gift_good, id ? id : 1, ms))
                set_page(P_TRADE);
        }
        if (back)
            set_page(near_mode == TRADE_GIFT ? P_GIFT_PICK : P_HOME);
        break;
    case P_FORTUNE:
        if (back || ok)
            set_page(P_HOME);
        break;
    default:
        trade_buttons(b, back, ms);
        break;
    }
    return true;
}

static bool task(absolute_time_t now) {
    smuggler_task(now);
    if (page == P_NEAR && absolute_time_diff_us(near_ts, now) > NEAR_REFRESH_MS * 1000ll) {
        near_ts = now;
        int before = n_near;
        uint32_t id = n_near ? near[near_sel].id : 0;
        refresh_near();
        if (n_near != before || (n_near && near[near_sel].id != id))
            dirty = true;
    }
    if (page == P_TRADE && trade.state == TS_CHOOSE && grid_sel >= cargo_kinds(cargo()))
        grid_sel = 0;
    bool d = dirty;
    dirty = false;
    return d;
}

static bool calm(void) {
    return ! (page == P_TRADE && trade_busy(&trade)) && ! showing_invite();
}


/* ------ Drawing ------ */

#define GRID_COLS 5
#define GRID_ROWS 3
#define CELL_W 40
#define CELL_H 38
#define GRID_Y (UI_TITLE_H + 2)
#define INFO_Y (GRID_Y + GRID_ROWS * CELL_H + 2)

/* An icon, \p scale times bigger; \p ghost: a gray silhouette (a good never owned) */
static void draw_icon(uint8_t *fb, int x, int y, int good, int scale, gfx_color_t ink, bool ghost) {
    const uint8_t *icon = smuggler_goods[good].icon;
    for (int r = 0; r < SMUGGLER_ICON_SIZE; ++r)
        for (int c = 0; c < SMUGGLER_ICON_SIZE; ++c)
            if ((icon[r * SMUGGLER_ICON_SIZE / 8 + c / 8] & (0x80 >> (c % 8))) && ! (ghost && (r + c) % 2))
                gfx_fill_rect(fb, x + c * scale, y + r * scale, scale, scale, ink);
}

static void fitted_text(uint8_t *fb, int y, const gfx_font_t *font, const char *text, gfx_color_t color) {
    char fit[64];
    ui_fit(font, fit, sizeof(fit), text, GFX_WIDTH - 4);
    gfx_text(fb, GFX_WIDTH / 2, y, font, fit, color, GFX_ALIGN_CENTER);
}

/* The cells of the goods (cargo, collection, choice of an offer or a gift), and the selected one below */
static void draw_grid(uint8_t *fb, bool dark, bool collection, int sel) {
    gfx_color_t fg = dark ? GFX_WHITE : GFX_BLACK, bg = dark ? GFX_BLACK : GFX_WHITE;
    int n = collection ? SMUGGLER_GOODS : cargo_kinds(cargo());
    int rows = (n + GRID_COLS - 1) / GRID_COLS;
    int first = sel / GRID_COLS - 1;
    if (first > rows - GRID_ROWS)
        first = rows - GRID_ROWS;
    if (first < 0)
        first = 0;
    for (int i = first * GRID_COLS; i < n && i < (first + GRID_ROWS) * GRID_COLS; ++i) {
        int x = (i % GRID_COLS) * CELL_W, y = GRID_Y + (i / GRID_COLS - first) * CELL_H;
        int good = collection ? i : cargo_nth(cargo(), i);
        bool known = cargo_seen(cargo(), good);
        gfx_color_t ink = fg, back = bg;
        if (i == sel) {
            gfx_fill_rect(fb, x + 1, y, CELL_W - 2, CELL_H - 1, fg);
            ink = bg;
            back = fg;
        }
        draw_icon(fb, x + 4, y + 3, good, 1, ink, ! known);
        if (! known) {
            gfx_fill_rect(fb, x + 14, y + 9, 12, 20, back);
            gfx_text(fb, x + 20, y + 10, &gfx_font_medium, "?", ink, GFX_ALIGN_CENTER);
        }
        int count = cargo_count(cargo(), good);
        if (count > 1) {
            char c[12];
            snprintf(c, sizeof(c), "%d", count);
            int w = gfx_text_width(&gfx_font_small, c);
            gfx_fill_rect(fb, x + CELL_W - w - 5, y + CELL_H - 15, w + 4, 14, ink);
            gfx_text(fb, x + CELL_W - 3, y + CELL_H - 15, &gfx_font_small, c, back, GFX_ALIGN_RIGHT);
        }
    }
    /* More rows above / below */
    if (first > 0)
        gfx_fill_rect(fb, GFX_WIDTH / 2 - 10, GRID_Y - 2, 20, 1, fg);
    if (first + GRID_ROWS < rows)
        gfx_fill_rect(fb, GFX_WIDTH / 2 - 10, GRID_Y + GRID_ROWS * CELL_H, 20, 1, fg);
    if (! n)
        return;
    int good = collection ? sel : cargo_nth(cargo(), sel);
    char text[64];
    if (! cargo_seen(cargo(), good)) {
        fitted_text(fb, INFO_Y, &gfx_font_small, "???", fg);
        snprintf(text, sizeof(text), "Inconnu  (%d / %d)", sel + 1, n);
    } else {
        fitted_text(fb, INFO_Y, &gfx_font_small, smuggler_goods[good].name, fg);
        snprintf(text, sizeof(text), "x%d  %s  %u doublon%s", cargo_count(cargo(), good),
                 smuggler_rarity_name(smuggler_goods[good].rarity), smuggler_goods[good].value,
                 smuggler_goods[good].value > 1 ? "s" : "");
    }
    fitted_text(fb, INFO_Y + 15, &gfx_font_small, text, fg);
}

/* The dark pages of the trade: discreet */
static void dark_page(uint8_t *fb, const char *header, const char *footer) {
    gfx_fill_rect(fb, 0, 0, GFX_WIDTH, GFX_HEIGHT, GFX_BLACK);
    if (header) {
        fitted_text(fb, 5, &gfx_font_small, header, GFX_WHITE);
        gfx_fill_rect(fb, 30, UI_TITLE_H - 4, GFX_WIDTH - 60, 1, GFX_WHITE);
    }
    gfx_fill_rect(fb, 0, UI_FOOTER_Y - 2, GFX_WIDTH, 1, GFX_WHITE);
    gfx_text(fb, GFX_WIDTH / 2, UI_FOOTER_Y, &gfx_font_small, footer, GFX_WHITE, GFX_ALIGN_CENTER);
}

/* White lines on the dark page, centered */
static int dark_lines(uint8_t *fb, int y, const gfx_font_t *font, const char *text) {
    char line[64];
    while (*text) {
        const char *nl = strchr(text, '\n');
        int n = nl ? nl - text : (int)strlen(text);
        snprintf(line, sizeof(line), "%.*s", n, text);
        fitted_text(fb, y, font, line, GFX_WHITE);
        y += font->height + 2;
        text += n + (nl != NULL);
    }
    return y;
}

static void render_invite(uint8_t *fb) {
    char text[64];
    dark_page(fb, "Psst...", "G : refuser  D : accepter");
    if (trade.inv_mode == TRADE_GIFT) {
        snprintf(text, sizeof(text), "%s vous offre :", trade.inv_name);
        dark_lines(fb, UI_TITLE_H + 4, &gfx_font_small, text);
        draw_icon(fb, GFX_WIDTH / 2 - 32, UI_TITLE_H + 24, trade.inv_good, 2, GFX_WHITE, false);
        dark_lines(fb, UI_TITLE_H + 94, &gfx_font_medium, smuggler_goods[trade.inv_good].name);
        dark_lines(fb, UI_TITLE_H + 118, &gfx_font_small, smuggler_rarity_name(smuggler_goods[trade.inv_good].rarity));
    } else {
        snprintf(text, sizeof(text), "%s\npropose une affaire\nen douce.", trade.inv_name);
        dark_lines(fb, 60, &gfx_font_medium, text);
    }
}

static void render_trade(uint8_t *fb) {
    char header[40], text[96];
    snprintf(header, sizeof(header), "En douce avec %s", trade.peer_name);
    bool gift = trade.mode == TRADE_GIFT;
    switch (trade.state) {
    case TS_INVITING:
        dark_page(fb, header, "G : annuler");
        if (gift) {
            draw_icon(fb, GFX_WIDTH / 2 - 32, UI_TITLE_H + 6, trade.my_good, 2, GFX_WHITE, false);
            snprintf(text, sizeof(text), "%s\nproposé à %s...", smuggler_goods[trade.my_good].name, trade.peer_name);
            dark_lines(fb, UI_TITLE_H + 78, &gfx_font_small, text);
        } else {
            snprintf(text, sizeof(text), "Proposition envoyée\nà %s...", trade.peer_name);
            dark_lines(fb, 60, &gfx_font_medium, text);
        }
        dark_lines(fb, 136, &gfx_font_small, "Gardez les badges\nl'un contre l'autre.");
        break;
    case TS_CHOOSE:
        snprintf(header, sizeof(header), "Votre offre pour %s ?", trade.peer_name);
        if (! cargo_kinds(cargo())) {
            dark_page(fb, header, "G : annuler");
            dark_lines(fb, 70, &gfx_font_medium, "Cale vide :\nrien à offrir.");
        } else {
            dark_page(fb, header, "G : annuler  D : offrir");
            draw_grid(fb, true, false, grid_sel);
        }
        break;
    case TS_OFFERED:
        dark_page(fb, header, "G : annuler");
        draw_icon(fb, GFX_WIDTH / 2 - 32, UI_TITLE_H + 6, trade.my_good, 2, GFX_WHITE, false);
        dark_lines(fb, UI_TITLE_H + 78, &gfx_font_small, smuggler_goods[trade.my_good].name);
        snprintf(text, sizeof(text), "En attente de l'offre\nde %s...", trade.peer_name);
        dark_lines(fb, UI_TITLE_H + 104, &gfx_font_small, text);
        break;
    case TS_REVIEW:
    case TS_CONFIRMED:
        if (gift) {
            /* The guest of a gift: it waits for the good */
            dark_page(fb, header, "G : retour");
            draw_icon(fb, GFX_WIDTH / 2 - 32, UI_TITLE_H + 6, trade.peer_good, 2, GFX_WHITE, false);
            snprintf(text, sizeof(text), "%s\nRéception en cours...", smuggler_goods[trade.peer_good].name);
            dark_lines(fb, UI_TITLE_H + 78, &gfx_font_small, text);
            break;
        }
        dark_page(fb, header, trade.state == TS_REVIEW ? "G : annuler  D : conclure" :
                  trade_cancellable(&trade) ? "G : annuler" : "G : retour (scellé)");
        draw_icon(fb, 12, UI_TITLE_H + 2, trade.my_good, 2, GFX_WHITE, false);
        draw_icon(fb, GFX_WIDTH - 76, UI_TITLE_H + 2, trade.peer_good, 2, GFX_WHITE, false);
        gfx_text(fb, GFX_WIDTH / 2, UI_TITLE_H + 22, &gfx_font_medium, "<>", GFX_WHITE, GFX_ALIGN_CENTER);
        snprintf(text, sizeof(text), "Votre %s", smuggler_goods[trade.my_good].name);
        dark_lines(fb, UI_TITLE_H + 70, &gfx_font_small, text);
        snprintf(text, sizeof(text), "contre %s", smuggler_goods[trade.peer_good].name);
        dark_lines(fb, UI_TITLE_H + 86, &gfx_font_small, text);
        if (trade.state == TS_CONFIRMED)
            snprintf(text, sizeof(text), trade.inviter ? "En attente de %s..." : "Scellé. En attente\nde %s...",
                     trade.peer_name);
        else if (trade.peer_confirmed)
            snprintf(text, sizeof(text), "%s a conclu.", trade.peer_name);
        else
            text[0] = 0;
        dark_lines(fb, UI_TITLE_H + 108, &gfx_font_small, text);
        break;
    case TS_DONE: {
        dark_page(fb, NULL, "D : continuer");
        bool gave = gift && trade.inviter;
        uint8_t good = gave ? trade.my_good : trade.peer_good;
        dark_lines(fb, 8, &gfx_font_medium, gave ? "Cadeau remis" : "Affaire conclue");
        draw_icon(fb, GFX_WIDTH / 2 - 32, 36, good, 2, GFX_WHITE, false);
        snprintf(text, sizeof(text), "%s %s", gave ? "Remis :" : "Reçu :", smuggler_goods[good].name);
        dark_lines(fb, 108, &gfx_font_small, text);
        dark_lines(fb, 126, &gfx_font_small, smuggler_rarity_name(smuggler_goods[good].rarity));
        break;
    }
    case TS_REFUSED:
        dark_page(fb, header, "D : continuer");
        snprintf(text, sizeof(text), "%s\nrefuse l'affaire.", trade.peer_name);
        dark_lines(fb, 70, &gfx_font_medium, text);
        break;
    case TS_LOST:
        dark_page(fb, header, "D : continuer");
        snprintf(text, sizeof(text), "%s est\nhors de portée.\nAffaire annulée.", trade.peer_name);
        dark_lines(fb, 56, &gfx_font_medium, text);
        break;
    default:
        dark_page(fb, header, "D : continuer");
        dark_lines(fb, 70, &gfx_font_medium, "Affaire annulée.");
        break;
    }
}

static void home_label(int i, char *buf, size_t len) {
    const uint8_t *c = cargo();
    switch (i) {
    case H_CARGO: snprintf(buf, len, "Cale : %d marchandise%s", cargo_total(c), cargo_total(c) > 1 ? "s" : ""); break;
    case H_TRADE: snprintf(buf, len, "Échanger en douce"); break;
    case H_GIFT: snprintf(buf, len, "Donner"); break;
    case H_COLLECTION: snprintf(buf, len, "Collection : %d / %d", cargo_seen_kinds(c), SMUGGLER_GOODS); break;
    default: snprintf(buf, len, "Fortune : %lu doublons", (unsigned long)cargo_value(c)); break;
    }
}

static void near_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s#%04X  %d dBm", near[i].name, (unsigned)(near[i].id & 0xFFFF), near[i].rssi);
}

static const char *rank(uint32_t value) {
    return value < 20 ? "Mousse" : value < 60 ? "Matelot" : value < 150 ? "Contrebandier" : value < 400 ? "Capitaine" :
           "Roi de la contrebande";
}

static void render_detail(uint8_t *fb) {
    const smuggler_good_t *g = &smuggler_goods[detail_good];
    bool known = cargo_seen(cargo(), detail_good);
    char text[48];
    ui_fit(&gfx_font_medium, text, sizeof(text), known ? g->name : "Marchandise inconnue", GFX_WIDTH - 4);
    ui_title(fb, text);
    draw_icon(fb, 6, UI_TITLE_H + 6, detail_good, 2, GFX_BLACK, ! known);
    if (! known) {
        gfx_text(fb, 38, UI_TITLE_H + 26, &gfx_font_large, "?", GFX_BLACK, GFX_ALIGN_CENTER);
        ui_text(fb, 80, UI_TITLE_H + 14, &gfx_font_small, "Jamais vue\ndans votre cale.");
        ui_lines(fb, UI_TITLE_H + 84, &gfx_font_small, "Échangez avec les autres\ncigales pour la découvrir.");
    } else {
        static const char *RARITY_TITLES[RARITIES] = {"Commun", "Rare", "Légendaire !"};
        gfx_text(fb, 80, UI_TITLE_H + 10, &gfx_font_medium, RARITY_TITLES[g->rarity], GFX_BLACK, GFX_ALIGN_LEFT);
        snprintf(text, sizeof(text), "%u doublon%s", g->value, g->value > 1 ? "s" : "");
        gfx_text(fb, 80, UI_TITLE_H + 32, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_LEFT);
        snprintf(text, sizeof(text), "En cale : %d", cargo_count(cargo(), detail_good));
        gfx_text(fb, 80, UI_TITLE_H + 50, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_LEFT);
        ui_lines(fb, UI_TITLE_H + 84, &gfx_font_small, g->story);
    }
    ui_footer(fb, "G : retour");
}

static void render_fortune(uint8_t *fb) {
    const uint8_t *c = cargo();
    char text[128];
    ui_title(fb, "Fortune");
    draw_icon(fb, 14, UI_TITLE_H + 8, 22, 1, GFX_BLACK, false);  /* The purse of doublons */
    snprintf(text, sizeof(text), "%lu", (unsigned long)cargo_value(c));
    gfx_text(fb, 120, UI_TITLE_H + 6, &gfx_font_large, text, GFX_BLACK, GFX_ALIGN_CENTER);
    gfx_text(fb, 120, UI_TITLE_H + 32, &gfx_font_small, "doublons", GFX_BLACK, GFX_ALIGN_CENTER);
    snprintf(text, sizeof(text), "Rang : %s\nMarchandises : %d\nDifférentes : %d / %d\nAffaires conclues : %u",
             rank(cargo_value(c)), cargo_total(c), cargo_kinds(c), SMUGGLER_GOODS,
             store_get()->achv_counters[ACHV_CNT_TRADES]);
    ui_text(fb, 10, UI_TITLE_H + 56, &gfx_font_small, text);
    ui_footer(fb, "G : retour");
}

static void render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    if (showing_invite()) {
        render_invite(fb);
        return;
    }
    switch (page) {
    case P_HOME:
        ui_title(fb, "Contrebande");
        ui_list(fb, H_ROWS, home_sel, home_label);
        ui_lines(fb, UI_TITLE_H + 3 + H_ROWS * UI_ROW_H + 10, &gfx_font_small, "Badge contre badge...");
        ui_footer(fb, "G : retour  D : choisir");
        break;
    case P_CARGO:
    case P_GIFT_PICK:
        ui_title(fb, page == P_CARGO ? "Cale" : "Que donner ?");
        if (! cargo_kinds(cargo())) {
            ui_lines(fb, 70, &gfx_font_small, "La cale est vide.\nRencontrez des cigales :\nelles laissent des vivres.");
            ui_footer(fb, "G : retour");
        } else {
            draw_grid(fb, false, false, grid_sel);
            ui_footer(fb, page == P_CARGO ? "G : retour  D : détails" : "G : retour  D : donner");
        }
        break;
    case P_COLLECTION:
        ui_title(fb, "Collection");
        draw_grid(fb, false, true, grid_sel);
        ui_footer(fb, "G : retour  D : détails");
        break;
    case P_DETAIL:
        render_detail(fb);
        break;
    case P_FORTUNE:
        render_fortune(fb);
        break;
    case P_NEAR:
        ui_title(fb, near_mode == TRADE_GIFT ? "Donner à qui ?" : "À portée de main");
        if (n_near) {
            ui_list(fb, n_near, near_sel, near_label);
            ui_footer(fb, near_mode == TRADE_GIFT ? "G : retour  D : donner" : "G : retour  D : proposer");
        } else {
            char text[96];
            snprintf(text, sizeof(text), "Personne à portée de main.\nCollez votre badge contre\ncelui d'une autre "
                     "cigale.%s", n_far ? "\n\nTrop loin :" : "");
            int y = ui_lines(fb, 50, &gfx_font_small, text);
            if (n_far) {
                snprintf(text, sizeof(text), "%d cigale%s", n_far, n_far > 1 ? "s" : "");
                ui_lines(fb, y, &gfx_font_small, text);
            }
            ui_footer(fb, "G : retour");
        }
        break;
    default:
        render_trade(fb);
        break;
    }
}

const app_t app_smuggler = {
    .name = "Contrebande",
    .start = start,
    .buttons = buttons,
    .task = task,
    .render = render,
    .calm = calm,
    .stop = stop,
};
