/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The LEDs of all the cicadas driven by an admin badge (Admin > LEDs des cigales): a color (from a list, or R, G,
 * B from 0 to 255) and a mode: fixed, blinking (time on / off) or fading (time to light up / to go black).
 * NET_LEDS [nonce 2][mode][r][g][b][time 1, ms, u16 LE][time 2, ms, u16 LE][brightness %][TTL][origin 4], +10 dBm,
 * sent 5 times over 2 s (the brightness, 1 to 100 %, applies to the color in every mode; absent from the older
 * firmwares: 100 %; the TTL and the origin: the relay from cicada to cicada, relay.h).
 * The cicadas show it instead of their own LED animation until "Rétablir" (mode 0) or a restart; the mute mode
 * still turns the LEDs off, and the pages that drive the LEDs themselves (games, talk badge) keep them. */

#include <stdio.h>
#include <string.h>

#include "pico/rand.h"

#include "app.h"
#include "i18n.h"
#include "leds.h"
#include "net.h"
#include "relay.h"

#define REPEATS 5
#define REPEAT_MS 450
#define PACKET_LEN 16
#define PACKET_LEN_OLD 10  /* Without the brightness */
#define PACKET_LEN_RELAY 16  /* With the TTL and the origin */
#define TIME_MIN_MS 50
#define TIME_MAX_MS 5000
#define TIME_STEP_MS 50

enum { MODE_RESTORE, MODE_FIXED, MODE_BLINK, MODE_FADE, N_MODES };
static const char *MODES[N_MODES] = {N_("Rétablir"), N_("Fixe"), N_("Clignotant"), N_("Fondu")};

typedef struct {
    uint8_t mode;
    uint8_t r, g, b;
    uint16_t t1, t2;  /* Blink: on / off; fade: to the color / to black (ms) */
    uint8_t level;  /* Brightness, 1 to 100 % */
} ledcast_t;

/* The color at its brightness */
static uint32_t level_color(const ledcast_t *l) {
    return LED_RGB(l->r * l->level / 100, l->g * l->level / 100, l->b * l->level / 100);
}

/* ------ Receiving: the LEDs of this badge ------ */

static ledcast_t shown = {MODE_RESTORE, 0, 0, 0, 0, 0, 100};
static bool pending = false;  /* Received: the main loop shows it (ledcast_changed()) */
static uint32_t last_src = 0;
static uint16_t last_nonce = 0;

/* Shows the animation, if an admin badge set one: returns false otherwise (the badge shows its own animation) */
bool ledcast_show(void) {
    uint32_t color = level_color(&shown);
    switch (shown.mode) {
    case MODE_FIXED: leds_anim_fixed(color); return true;
    case MODE_BLINK: leds_anim_blink(color, shown.t1 * 1000ull, shown.t2 * 1000ull); return true;
    case MODE_FADE: leds_anim_fade(color, shown.t1 * 1000ull, shown.t2 * 1000ull); return true;
    default: return false;
    }
}

bool ledcast_changed(void) {
    bool p = pending;
    pending = false;
    return p;
}

static void apply(const ledcast_t *l, const char *from) {
    shown = *l;
    pending = true;
    printf("leds: %s, color %u %u %u at %u %%, times %u / %u ms (from %s)\n", MODES[l->mode < N_MODES ? l->mode : 0],
           l->r, l->g, l->b, l->level, l->t1, l->t2, from);
}

static void handle_leds(const net_packet_t *p) {
    if (p->len < PACKET_LEN_OLD)
        return;
    const uint8_t *d = p->data;
    uint16_t nonce = d[0] | d[1] << 8;
    uint32_t origin = p->src;
    if (p->len >= PACKET_LEN_RELAY) {
        origin = net_u32(d + 12);
        uint8_t again[PACKET_LEN_RELAY];
        memcpy(again, d, PACKET_LEN_RELAY);
        again[11] = d[11] ? d[11] - 1 : 0;
        uint32_t key = relay_hash(0x811C9DC5u, d + 2, 9);  /* The kind: the order (mode, color, times, brightness) */
        relay_offer(NET_LEDS, again, PACKET_LEN_RELAY, p->src, origin, nonce, key, d[11], p->at);
    }
    if (origin == last_src && nonce == last_nonce)
        return;  /* The same order, repeated (or relayed) */
    last_src = origin;
    last_nonce = nonce;
    ledcast_t l = {d[2], d[3], d[4], d[5], d[6] | d[7] << 8, d[8] | d[9] << 8, p->len > PACKET_LEN_OLD ? d[10] : 100};
    if (l.level < 1 || l.level > 100)
        l.level = 100;
    if (l.mode >= N_MODES)
        return;
    if (l.t1 < TIME_MIN_MS) l.t1 = TIME_MIN_MS;
    if (l.t2 < TIME_MIN_MS) l.t2 = TIME_MIN_MS;
    if (l.t1 > TIME_MAX_MS) l.t1 = TIME_MAX_MS;
    if (l.t2 > TIME_MAX_MS) l.t2 = TIME_MAX_MS;
    char from[12];
    snprintf(from, sizeof(from), "%08lX", (unsigned long)origin);
    apply(&l, from);
}

void ledcast_init(void) {
    net_subscribe(NET_LEDS, handle_leds);
}

/* ------ Sending: the admin page ------ */

typedef struct {
    const char *name;
    uint8_t r, g, b;
} preset_t;

static const preset_t PRESETS[] = {
    {N_("Rouge"), 255, 0, 0}, {N_("Orange"), 255, 80, 0}, {N_("Jaune"), 255, 200, 0}, {N_("Vert"), 0, 255, 0},
    {N_("Cyan"), 0, 255, 255}, {N_("Bleu"), 0, 0, 255}, {N_("Violet"), 160, 0, 255}, {N_("Rose"), 255, 0, 120},
    {N_("Blanc"), 255, 255, 255},
};
#define N_PRESETS ((int)(sizeof(PRESETS) / sizeof(PRESETS[0])))

enum { ROW_COLOR, ROW_R, ROW_G, ROW_B, ROW_LEVEL, ROW_MODE, ROW_SEND, ROW_RESTORE, N_ROWS };

/* The brightnesses offered (%): the full power of the LEDs is dazzling up close */
static const uint8_t LEVELS[] = {5, 10, 25, 50, 75, 100};
#define N_LEVELS ((int)(sizeof(LEVELS) / sizeof(LEVELS[0])))

static ledcast_t order = {MODE_FIXED, 255, 0, 0, 500, 500, 50};
static int preset = 0;  /* -1: custom (R, G, B changed) */
static int row = 0;
static bool timing = false;  /* The page of the times of the mode */
static int timing_row = 0;
static uint8_t packet[PACKET_LEN];
static int sends_left = 0;
static absolute_time_t send_ts = 0, repeat_ts[2] = {0, 0};
static char status[40] = "";

static void preview(void) {
    uint32_t color = level_color(&order);
    switch (order.mode) {
    case MODE_BLINK: leds_anim_blink(color, order.t1 * 1000ull, order.t2 * 1000ull); break;
    case MODE_FADE: leds_anim_fade(color, order.t1 * 1000ull, order.t2 * 1000ull); break;
    default: leds_anim_fixed(color); break;
    }
}

static void send(uint8_t mode, absolute_time_t now) {
    uint16_t nonce = get_rand_32();
    ledcast_t l = order;
    l.mode = mode;
    uint8_t d[PACKET_LEN] = {nonce, nonce >> 8, l.mode, l.r, l.g, l.b, l.t1, l.t1 >> 8, l.t2, l.t2 >> 8, l.level,
                             relay_admin_ttl()};
    net_put_u32(d + 12, net_id());
    memcpy(packet, d, sizeof(packet));
    sends_left = REPEATS;
    send_ts = now;
    apply(&l, "this badge");  /* The admin badge too */
    snprintf(status, sizeof(status), mode == MODE_RESTORE ? N_("LEDs rétablies") : N_("Envoyé aux cigales"));
}

static void ledcast_start(absolute_time_t now) {
    (void)now;
    row = 0;
    timing = false;
    status[0] = 0;
    preview();
}

static void ledcast_stop(void) {
    app_leds(0, 0, 0);  /* Back to the animation of the badge (or the one received) */
}

/* +1 / -1, faster while held */
static int change(const app_buttons_t *b, absolute_time_t now) {
    for (int w = 0; w < 2; ++w) {
        uint8_t bit = w ? UI_BTN_B : UI_BTN_A;
        int sign = w ? 1 : -1;
        if (b->pressed & bit) {
            repeat_ts[w] = delayed_by_ms(now, 400);
            return sign;
        }
        if ((b->held & bit) && absolute_time_diff_us(repeat_ts[w], now) >= 0) {
            repeat_ts[w] = delayed_by_ms(now, 60);
            return sign * (app_held_ms(b, bit) > 2000 ? 10 : 1);
        }
    }
    return 0;
}

static int clamp(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

static bool ledcast_buttons(const app_buttons_t *b, absolute_time_t now) {
    if (timing) {
        /* The times of the mode: flanks choose, wings -/+, long press on a wing: back */
        if (b->long_pressed & (UI_BTN_A | UI_BTN_B)) {
            timing = false;
            return true;
        }
        if (b->pressed & (UI_BTN_X | UI_BTN_Y))
            timing_row ^= 1;
        int c = change(b, now);
        if (c) {
            uint16_t *t = timing_row ? &order.t2 : &order.t1;
            *t = clamp(*t + c * TIME_STEP_MS, TIME_MIN_MS, TIME_MAX_MS);
            preview();
        }
        return true;
    }
    if (b->pressed & UI_BTN_Y)
        row = (row + N_ROWS - 1) % N_ROWS;
    if (b->pressed & UI_BTN_X)
        row = (row + 1) % N_ROWS;
    if (b->long_pressed & UI_BTN_A)
        return false;
    switch (row) {
    case ROW_SEND:
    case ROW_RESTORE:
        if (b->pressed & UI_BTN_A)
            return false;
        if (b->pressed & UI_BTN_B)
            send(row == ROW_SEND ? order.mode : MODE_RESTORE, now);
        break;
    case ROW_MODE:
        if ((b->long_pressed & UI_BTN_B) && order.mode != MODE_FIXED) {
            timing = true;  /* The times of the blinking / fading */
            timing_row = 0;
            break;
        }
        if (b->released_short & UI_BTN_B)
            order.mode = order.mode % (N_MODES - 1) + 1;  /* Fixe, Clignotant, Fondu */
        if (b->pressed & UI_BTN_A)
            order.mode = (order.mode + N_MODES - 3) % (N_MODES - 1) + 1;
        preview();
        break;
    case ROW_LEVEL:
        if (b->pressed & (UI_BTN_A | UI_BTN_B)) {
            int k = 0;
            while (k < N_LEVELS - 1 && LEVELS[k] < order.level)
                ++k;
            k = (k + ((b->pressed & UI_BTN_B) ? 1 : N_LEVELS - 1)) % N_LEVELS;
            order.level = LEVELS[k];
            preview();
        }
        break;
    case ROW_COLOR:
        if (b->pressed & (UI_BTN_A | UI_BTN_B)) {
            preset = ((preset < 0 ? 0 : preset) + ((b->pressed & UI_BTN_B) ? 1 : N_PRESETS - 1)) % N_PRESETS;
            order.r = PRESETS[preset].r;
            order.g = PRESETS[preset].g;
            order.b = PRESETS[preset].b;
            preview();
        }
        break;
    default: {
        int c = change(b, now);
        if (c) {
            uint8_t *v = row == ROW_R ? &order.r : row == ROW_G ? &order.g : &order.b;
            *v = clamp(*v + c, 0, 255);
            preset = -1;
            preview();
        }
        break;
    }
    }
    return true;
}

static bool ledcast_task(absolute_time_t now) {
    if (sends_left && absolute_time_diff_us(send_ts, now) >= 0 && net_send(NET_LEDS, packet, PACKET_LEN, NET_LOUD)) {
        --sends_left;
        send_ts = delayed_by_ms(now, REPEAT_MS);
    }
    return false;
}

static void row_label(int i, char *buf, size_t len) {
    switch (i) {
    case ROW_COLOR:
        snprintf(buf, len, _("Couleur : %s"), preset < 0 ? _("personnalisée") : tr(PRESETS[preset].name));
        break;
    case ROW_R: snprintf(buf, len, _("Rouge (R) : %u"), order.r); break;
    case ROW_G: snprintf(buf, len, _("Vert (G) : %u"), order.g); break;
    case ROW_B: snprintf(buf, len, _("Bleu (B) : %u"), order.b); break;
    case ROW_LEVEL: snprintf(buf, len, _("Luminosité : %u %%"), order.level); break;
    case ROW_MODE: snprintf(buf, len, _("Mode : %s"), tr(MODES[order.mode])); break;
    case ROW_SEND: snprintf(buf, len, N_("> Envoyer aux cigales")); break;
    default: snprintf(buf, len, N_("> Rétablir leurs LEDs")); break;
    }
}

static void ledcast_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    char text[48];
    if (timing) {
        ui_title(fb, order.mode == MODE_BLINK ? N_("Clignotement") : N_("Fondu"));
        const char *names[2][2] = {{N_("Allumé"), N_("Éteint")}, {N_("Vers la couleur"), N_("Vers le noir")}};
        int m = order.mode == MODE_BLINK ? 0 : 1;
        for (int i = 0; i < 2; ++i) {
            snprintf(text, sizeof(text), _("%s : %u ms"), tr(names[m][i]), i ? order.t2 : order.t1);
            int y = UI_TITLE_H + 20 + i * 30;
            if (i == timing_row) {
                gfx_fill_rect(fb, 4, y - 2, GFX_WIDTH - 8, 22, GFX_BLACK);
                gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, text, GFX_WHITE, GFX_ALIGN_CENTER);
            } else {
                gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
            }
        }
        ui_lines(fb, UI_TITLE_H + 90, &gfx_font_small, N_("Flancs : choisir\nAiles : - / +  (maintenir : vite)"));
        ui_footer(fb, N_("Aile longue : retour"));
        return;
    }
    ui_title(fb, N_("LEDs des cigales"));
    ui_list(fb, N_ROWS, row, row_label);
    const char *help = row == ROW_SEND || row == ROW_RESTORE ? N_("G : retour  D : valider")
                       : row == ROW_MODE ? (order.mode == MODE_FIXED ? N_("Ailes : mode")
                                                                     : N_("Ailes : mode  D long : temps"))
                       : N_("Ailes : -  +");
    ui_footer(fb, status[0] && (row == ROW_SEND || row == ROW_RESTORE) ? status : help);
}

const app_t app_ledcast = {
    .name = N_("LEDs des cigales"),
    .start = ledcast_start,
    .buttons = ledcast_buttons,
    .task = ledcast_task,
    .render = ledcast_render,
    .stop = ledcast_stop,
    .owns_leds = true,  /* The preview of the color */
};
