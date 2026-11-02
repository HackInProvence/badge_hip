/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The Flipper Zero theme: see flipper.h. The format of the .FZA files: tools/flipper_anims.py. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/rand.h"

#include "achievements.h"
#include "app.h"
#include "battery.h"
#include "ff.h"
#include "flipper.h"
#include "gfx.h"
#include "i18n.h"
#include "sd.h"
#include "shared_buf.h"
#include "social.h"
#include "store.h"
#include "ui.h"

#define HEADER_MAX 1024  /* HEADER_MAX of tools/flipper_anims.py */
#define FIXED_SIZE 18  /* "FZA1", 12 bytes, the offset of the bitmaps */
#define FRAME_MAX (16 * 64)  /* 128 x 64 in 1 bit */
#define MAX_ANIMS 48
#define NAME_LEN 40
#define SLEEP_ANIM "L1_Sleep.FZA"  /* The screensaver */
#define BATTERY_ANIM "L1_BadBattery.FZA"  /* When the battery is low */

/* Where the animation (128 x 64 on the Flipper) is drawn on the page: zoomed 1.5 times, above the footer */
#define ANIM_W 192
#define ANIM_H 96
#define ANIM_X ((GFX_WIDTH - ANIM_W) / 2)
#define ANIM_Y (UI_FOOTER_Y - 2 - ANIM_H)

typedef struct {
    FIL f;
    uint8_t head[HEADER_MAX];
    uint8_t frame[FRAME_MAX];
} anim_io_t;

/* The desktop, in shared_buf while it is open */
typedef struct {
    anim_io_t io;
    char names[MAX_ANIMS][NAME_LEN];
} desk_buf_t;

_Static_assert(sizeof(desk_buf_t) <= SHARED_BUF_SIZE, "shared_buf too small");
_Static_assert(sizeof(anim_io_t) <= FLIPPER_SAVER_SCRATCH, "FLIPPER_SAVER_SCRATCH too small");

#define DESK ((desk_buf_t *)shared_buf)

/* The fixed part of the header of a .FZA file */
typedef struct {
    uint8_t w, h, fps, min_level, max_level, weight, n_bitmaps, passive, active, cycles, cooldown, n_bubbles;
    uint16_t offset;
} fza_t;

#define STORE_THEME_FLIPPER 0xF1

/* ------ The theme ------ */

void flipper_init(void) {
    ui_flipper = flipper_theme();
}

bool flipper_theme(void) {
    return store_get()->theme == STORE_THEME_FLIPPER;
}

void flipper_set_theme(bool on) {
    store_get()->theme = on ? STORE_THEME_FLIPPER : 0;
    store_changed();
    ui_flipper = on;
    printf("flipper: theme %s\n", on ? "on" : "off");
}

/* ------ The .FZA files ------ */

/* Opens FLIPPER/<name> and reads its header into io->head: the file stays open (fza_close()) */
static bool fza_open(anim_io_t *io, const char *name, fza_t *a) {
    char path[sizeof(FLIPPER_DIR) + NAME_LEN + 1];
    snprintf(path, sizeof(path), FLIPPER_DIR "/%s", name);
    if (sd_mount() != FR_OK || f_open(&io->f, path, FA_READ) != FR_OK)
        return false;
    UINT n = 0;
    const uint8_t *p = io->head;
    if (f_read(&io->f, io->head, HEADER_MAX, &n) == FR_OK && n >= FIXED_SIZE && ! memcmp(p, "FZA1", 4)) {
        a->w = p[4];
        a->h = p[5];
        a->fps = p[6];
        a->min_level = p[7];
        a->max_level = p[8];
        a->weight = p[9];
        a->n_bitmaps = p[10];
        a->passive = p[11];
        a->active = p[12];
        a->cycles = p[13];
        a->cooldown = p[14];
        a->n_bubbles = p[15];
        a->offset = p[16] | p[17] << 8;
        if (a->w && a->w <= 128 && a->h && a->h <= 64 && a->n_bitmaps && a->passive + a->active > 0
                && a->offset <= n && FIXED_SIZE + a->passive + a->active <= a->offset)
            return true;
    }
    f_close(&io->f);
    return false;
}

static void fza_close(anim_io_t *io) {
    f_close(&io->f);
}

static unsigned fza_frame_bytes(const fza_t *a) {
    return (a->w + 7) / 8 * a->h;
}

/* Reads the bitmap \p i into io->frame */
static bool fza_read_frame(anim_io_t *io, const fza_t *a, int i) {
    UINT n = 0;
    unsigned size = fza_frame_bytes(a);
    if (i >= a->n_bitmaps)
        return false;
    return f_lseek(&io->f, a->offset + (FSIZE_t)i * size) == FR_OK && f_read(&io->f, io->frame, size, &n) == FR_OK
           && n == size;
}

/* The bitmap of the frame \p pos of the order (passive then active) */
static int fza_order(const anim_io_t *io, const fza_t *a, int pos) {
    return io->head[FIXED_SIZE + pos];
}

/* A bubble of the header */
typedef struct {
    uint8_t slot, x, y, align, start, end, len;
    const char *text;
} bubble_t;

static bool fza_bubble(const anim_io_t *io, const fza_t *a, int i, bubble_t *b) {
    unsigned p = FIXED_SIZE + a->passive + a->active;
    for (int k = 0; k <= i; ++k) {
        if (p + 7 > a->offset)
            return false;
        const uint8_t *e = io->head + p;
        b->slot = e[0];
        b->x = e[1];
        b->y = e[2];
        b->align = e[3];
        b->start = e[4];
        b->end = e[5];
        b->len = e[6];
        b->text = (const char *)e + 7;
        p += 7 + b->len;
        if (p > a->offset)
            return false;
    }
    return true;
}

/* ------ Drawing ------ */

/* The quarters of a circle: 1 top left, 2 top right, 4 bottom left, 8 bottom right */
static void arc(uint8_t *fb, int cx, int cy, int r, int quads) {
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        if (quads & 1) {
            gfx_pixel(fb, cx - x, cy - y, GFX_BLACK);
            gfx_pixel(fb, cx - y, cy - x, GFX_BLACK);
        }
        if (quads & 2) {
            gfx_pixel(fb, cx + x, cy - y, GFX_BLACK);
            gfx_pixel(fb, cx + y, cy - x, GFX_BLACK);
        }
        if (quads & 4) {
            gfx_pixel(fb, cx - x, cy + y, GFX_BLACK);
            gfx_pixel(fb, cx - y, cy + x, GFX_BLACK);
        }
        if (quads & 8) {
            gfx_pixel(fb, cx + x, cy + y, GFX_BLACK);
            gfx_pixel(fb, cx + y, cy + x, GFX_BLACK);
        }
        ++y;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            --x;
            err += 2 * (y - x) + 1;
        }
    }
}

static void round_rect(uint8_t *fb, int x, int y, int w, int h, int r) {
    gfx_fill_rect(fb, x + r, y, w - 2 * r, 1, GFX_BLACK);
    gfx_fill_rect(fb, x + r, y + h - 1, w - 2 * r, 1, GFX_BLACK);
    gfx_fill_rect(fb, x, y + r, 1, h - 2 * r, GFX_BLACK);
    gfx_fill_rect(fb, x + w - 1, y + r, 1, h - 2 * r, GFX_BLACK);
    arc(fb, x + r, y + r, r, 1);
    arc(fb, x + w - 1 - r, y + r, r, 2);
    arc(fb, x + r, y + h - 1 - r, r, 4);
    arc(fb, x + w - 1 - r, y + h - 1 - r, r, 8);
}

static void line(uint8_t *fb, int x0, int y0, int x1, int y1, gfx_color_t c) {
    int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        gfx_pixel(fb, x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/* A frame of the animation, zoomed 1.5 times, at the bottom of its place as on the Flipper */
static void draw_frame(uint8_t *fb, const anim_io_t *io, const fza_t *a) {
    int stride = (a->w + 7) / 8, w = a->w * 3 / 2, h = a->h * 3 / 2;
    int x0 = ANIM_X + (ANIM_W - w) / 2, y0 = ANIM_Y + ANIM_H - h;
    for (int y = 0; y < h; ++y) {
        const uint8_t *row = io->frame + (y * 2 / 3) * stride;
        for (int x = 0; x < w; ++x) {
            int sx = x * 2 / 3;
            if (row[sx / 8] & (0x80 >> (sx % 8)))
                gfx_pixel(fb, x0 + x, y0 + y, GFX_BLACK);
        }
    }
}

#ifdef FLIPPER_TEST
int flipper_bubbles_drawn;
#endif

/* A bubble of text as the Flipper draws it (elements_bubble_str()): its top left corner at (x, y) of the screen of
 * the Flipper, its tail on the side given by the alignment (horizontal: 0 left, 1 center, 2 right; vertical << 4:
 * 0 top, 1 center, 2 bottom) */
static void draw_bubble(uint8_t *fb, const char *text, int len, int x, int y, uint8_t align) {
    char lines[4][40];
    int n = 0, w = 0;
    const char *p = text, *end = text + len;
    while (p < end && n < 4) {
        const char *nl = memchr(p, '\n', end - p);
        int l = (nl ? nl : end) - p;
        if (l > (int)sizeof(lines[0]) - 1)
            l = sizeof(lines[0]) - 1;
        memcpy(lines[n], p, l);
        lines[n][l] = 0;
        int lw = gfx_text_width(&gfx_font_small, lines[n]);
        if (lw > w)
            w = lw;
        ++n;
        p = nl ? nl + 1 : end;
    }
    if (! n)
        return;
#ifdef FLIPPER_TEST
    ++flipper_bubbles_drawn;
#endif
    const int line_h = gfx_font_small.height + 1, tail = 6;
    int bw = w + 12, bh = n * line_h + 6;
    int bx = ANIM_X + x * 3 / 2, by = ANIM_Y + y * 3 / 2;
    /* Our font is not the one of the Flipper: the bubble kept on the page, in the place of the animation */
    if (bw > GFX_WIDTH - 2 * tail - 2)
        bw = GFX_WIDTH - 2 * tail - 2;
    if (bx + bw + tail > GFX_WIDTH - 1)
        bx = GFX_WIDTH - 1 - tail - bw;
    if (bx < tail + 1)
        bx = tail + 1;
    if (by + bh + tail > ANIM_Y + ANIM_H)
        by = ANIM_Y + ANIM_H - tail - bh;
    if (by < ANIM_Y + tail)
        by = ANIM_Y + tail;
    gfx_fill_rect(fb, bx + 1, by + 1, bw - 2, bh - 2, GFX_WHITE);  /* Inside the rounded corners */
    round_rect(fb, bx, by, bw, bh, 3);
    for (int i = 0; i < n; ++i)
        gfx_text(fb, bx + 6, by + 3 + i * line_h, &gfx_font_small, lines[i], GFX_BLACK, GFX_ALIGN_LEFT);
    /* The tail: a triangle from a side of the bubble (b1, b2) to its point (ax, ay) */
    int h = align & 0x0F, v = align >> 4, r = bx + bw - 1, bot = by + bh - 1, cy = by + (bh - 1) / 2;
    int b1x, b1y, b2x, b2y, ax, ay;
    if (h != 1) {
        int sx = h == 0 ? bx : r, dir = h == 0 ? -1 : 1;
        b1x = b2x = sx;
        ax = sx + dir * tail;
        if (v == 0) {
            b1y = by;
            b2y = by + tail;
            ay = by;
        } else if (v == 1) {
            b1y = cy - tail;
            b2y = cy + tail;
            ay = cy;
        } else {
            b1y = bot - tail;
            b2y = bot;
            ay = bot;
        }
    } else if (v != 1) {
        int cx = bx + (bw - 1) / 2, sy = v == 0 ? by : bot;
        b1y = b2y = sy;
        b1x = cx - tail;
        b2x = cx + tail;
        ax = cx;
        ay = sy + (v == 0 ? -tail : tail);
    } else {
        return;  /* Centered: no tail */
    }
    int steps = b2x - b1x + b2y - b1y;  /* One of both is 0 */
    for (int i = 0; i <= steps; ++i)
        line(fb, b1x == b2x ? b1x : b1x + i, b1y == b2y ? b1y : b1y + i, ax, ay, GFX_WHITE);
    line(fb, b1x, b1y, ax, ay, GFX_BLACK);
    line(fb, ax, ay, b2x, b2y, GFX_BLACK);
}

/* The name of the cicada above the animation */
static void draw_name(uint8_t *fb) {
    char name[24];
    ui_fit(&gfx_font_medium, name, sizeof(name), social_name(), GFX_WIDTH - 8);
    gfx_text(fb, GFX_WIDTH / 2, (ANIM_Y - gfx_font_medium.height) / 2, &gfx_font_medium, name, GFX_BLACK,
             GFX_ALIGN_CENTER);
}

/* ------ The screensaver: the dolphin asleep ------ */

bool flipper_saver_render(uint8_t *fb, uint8_t *scratch, size_t scratch_len) {
    if (! flipper_theme() || scratch_len < sizeof(anim_io_t))
        return false;
    anim_io_t *io = (anim_io_t *)scratch;
    fza_t a;
    gfx_clear(fb, GFX_WHITE);
    draw_name(fb);
    if (fza_open(io, SLEEP_ANIM, &a)) {
        if (fza_read_frame(io, &a, fza_order(io, &a, 0)))
            draw_frame(fb, io, &a);
        fza_close(io);
        draw_bubble(fb, "Zzz...", 6, 62, 2, 0x20);  /* Its tail down to the left, to the head of the dolphin */
    }
    printf("flipper: saver\n");
    return true;
}

/* ------ The desktop ------ */

enum { PASSIVE, ACTIVE };

static struct {
    bool open;  /* An animation is playing */
    bool no_card;  /* No card, or no animation on it */
    bool battery;  /* The animation of the low battery is shown */
    uint8_t n_names, cur;
    fza_t a;
    uint8_t phase, pos, cycle, slot;  /* slot: the bubbles of this active period, 0xFF: none */
    int loaded;  /* The bitmap in io.frame, -1: none */
    absolute_time_t next, passive_since;
} d;

static bool is_fza(const char *name) {
    size_t n = strlen(name);
    if (n < 5 || n >= NAME_LEN || name[0] == '.')
        return false;
    const char *e = name + n - 4;
    return e[0] == '.' && (e[1] | 0x20) == 'f' && (e[2] | 0x20) == 'z' && (e[3] | 0x20) == 'a';
}

/* The .FZA files of FLIPPER/, sorted */
static void scan(void) {
    DIR dir;
    FILINFO info;
    d.n_names = 0;
    if (sd_mount() != FR_OK || f_opendir(&dir, FLIPPER_DIR) != FR_OK)
        return;
    while (d.n_names < MAX_ANIMS && f_readdir(&dir, &info) == FR_OK && info.fname[0]) {
        if ((info.fattrib & AM_DIR) || ! is_fza(info.fname))
            continue;
        int i = d.n_names++;
        while (i > 0 && strcmp(DESK->names[i - 1], info.fname) > 0) {
            memcpy(DESK->names[i], DESK->names[i - 1], NAME_LEN);
            --i;
        }
        snprintf(DESK->names[i], NAME_LEN, "%s", info.fname);
    }
    f_closedir(&dir);
}

static int find_name(const char *name) {
    for (int i = 0; i < d.n_names; ++i)
        if (! strcmp(DESK->names[i], name))
            return i;
    return -1;
}

/* The level of the dolphin: the rank of the badge (10 ranks) on the 3 levels of the Flipper */
static int dolphin_level(void) {
    int rank = achv_level();
    return rank <= 4 ? 1 : rank <= 7 ? 2 : 3;
}

/* At random among the animations of the level, as many chances as their weight */
static int choose_random(void) {
    uint8_t weights[MAX_ANIMS];
    unsigned total = 0;
    int level = dolphin_level();
    for (int i = 0; i < d.n_names; ++i) {
        fza_t a;
        weights[i] = 0;
        if (fza_open(&DESK->io, DESK->names[i], &a)) {
            if (a.min_level && a.min_level <= level && level <= a.max_level)
                weights[i] = a.weight ? a.weight : 1;
            fza_close(&DESK->io);
        }
        total += weights[i];
    }
    if (! total)
        return d.n_names ? (int)(get_rand_32() % d.n_names) : -1;
    unsigned r = get_rand_32() % total;
    for (int i = 0; i < d.n_names; ++i) {
        if (r < weights[i])
            return i;
        r -= weights[i];
    }
    return 0;
}

static void enter_active(void) {
    d.phase = ACTIVE;
    d.pos = 0;
    d.cycle = 0;
    /* The bubbles of one slot at random */
    int slots = 0;
    bubble_t b = {0};
    for (int i = 0; i < d.a.n_bubbles && fza_bubble(&DESK->io, &d.a, i, &b); ++i)
        if (b.slot + 1 > slots)
            slots = b.slot + 1;
    d.slot = slots ? get_rand_32() % slots : 0xFF;
}

static bool load_current(void) {
    int pos = d.phase == ACTIVE ? d.a.passive + d.pos : d.pos;
    int bitmap = fza_order(&DESK->io, &d.a, pos);
    if (bitmap == d.loaded)
        return true;
    if (! fza_read_frame(&DESK->io, &d.a, bitmap))
        return false;
    d.loaded = bitmap;
    return true;
}

static void play(int i, absolute_time_t now) {
    if (d.open)
        fza_close(&DESK->io);
    d.open = false;
    if (i < 0 || i >= d.n_names)
        return;
    d.cur = i;
    if (! fza_open(&DESK->io, DESK->names[i], &d.a)) {
        printf("flipper: cannot open %s\n", DESK->names[i]);
        return;
    }
    d.open = true;
    d.phase = PASSIVE;
    d.pos = 0;
    d.slot = 0xFF;
    d.loaded = -1;
    d.passive_since = now;
    d.next = delayed_by_ms(now, 1000 / (d.a.fps ? d.a.fps : 2));
    if (! d.a.passive)
        enter_active();
    if (! load_current()) {
        fza_close(&DESK->io);
        d.open = false;
    }
    printf("flipper: %s (%u frames, level %d)\n", DESK->names[i], d.a.passive + d.a.active, dolphin_level());
}

static void flipper_start(absolute_time_t now) {
    memset(&d, 0, sizeof(d));
    scan();
    d.no_card = ! d.n_names;
    if (d.no_card) {
        printf("flipper: no animation in " FLIPPER_DIR "/\n");
        return;
    }
    int i = battery_low() ? find_name(BATTERY_ANIM) : -1;
    d.battery = i >= 0;
    play(i >= 0 ? i : choose_random(), now);
}

static void flipper_stop(void) {
    if (d.open)
        fza_close(&DESK->io);
    d.open = false;
}

static bool flipper_buttons(const app_buttons_t *b, absolute_time_t now) {
    if (b->pressed & UI_BTN_B)
        return false;  /* The menu (A does nothing, as Back on the desktop of the Flipper) */
    if (d.n_names && (b->pressed & (UI_BTN_X | UI_BTN_Y))) {
        int delta = b->pressed & UI_BTN_X ? 1 : -1;
        play((d.cur + delta + d.n_names) % d.n_names, now);
    }
    return true;
}

static bool flipper_task(absolute_time_t now) {
    if (! d.open)
        return false;
    if (battery_low() && ! d.battery) {
        d.battery = true;
        int i = find_name(BATTERY_ANIM);
        if (i >= 0) {
            play(i, now);
            return true;
        }
    }
    if (! time_reached(d.next))
        return false;
    int64_t period_us = 1000000 / (d.a.fps ? d.a.fps : 2);
    d.next = delayed_by_us(d.next, period_us);
    if (absolute_time_diff_us(now, d.next) < 0)
        d.next = delayed_by_us(now, period_us);  /* Late (the SD card, a full refresh): no burst */
    if (d.phase == PASSIVE) {
        if (++d.pos >= d.a.passive) {
            d.pos = 0;
            if (d.a.active && absolute_time_diff_us(d.passive_since, now) >= d.a.cooldown * 1000000ll)
                enter_active();
        }
    } else if (++d.pos >= d.a.active) {
        d.pos = 0;
        if (++d.cycle >= (d.a.cycles ? d.a.cycles : 1)) {
            d.phase = PASSIVE;
            d.passive_since = now;
            if (! d.a.passive)
                enter_active();
        }
    }
    if (! load_current()) {
        printf("flipper: read error\n");
        flipper_stop();
    }
    return true;
}

static void flipper_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    gfx_clear(fb, GFX_WHITE);
    draw_name(fb);
    if (d.open) {
        draw_frame(fb, &DESK->io, &d.a);
        if (d.phase == ACTIVE && d.slot != 0xFF) {
            int t = d.a.passive + d.pos;
            bubble_t b = {0};
            for (int i = 0; i < d.a.n_bubbles && fza_bubble(&DESK->io, &d.a, i, &b); ++i)
                if (b.slot == d.slot && b.start <= t && t <= b.end) {
                    draw_bubble(fb, b.text, b.len, b.x, b.y, b.align);
                    break;
                }
        }
    } else {
        /* No animation: what to copy on the card */
        ui_scroll_text(fb, GFX_WIDTH / 2, ANIM_Y + 30, GFX_WIDTH - 8, &gfx_font_small, d.no_card ? N_("Pas d'animation :")
                       : N_("Erreur de lecture"), GFX_BLACK, GFX_ALIGN_CENTER);
        if (d.no_card)
            ui_scroll_text(fb, GFX_WIDTH / 2, ANIM_Y + 50, GFX_WIDTH - 8, &gfx_font_small, N_("FLIPPER/ sur la carte SD"),
                           GFX_BLACK, GFX_ALIGN_CENTER);
    }
    ui_footer(fb, d.n_names > 1 ? N_("Y X : animation  B : menu") : N_("B : menu"));
}

static bool flipper_calm(void) {
    return ! d.open;  /* No slow full refresh while the dolphin moves */
}

const app_t app_flipper = {
    .name = N_("Flipper Zero"),
    .start = flipper_start,
    .buttons = flipper_buttons,
    .task = flipper_task,
    .render = flipper_render,
    .calm = flipper_calm,
    .stop = flipper_stop,
};
