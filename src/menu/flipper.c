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

/* Where the screen of the Flipper is drawn on the page (128 x 64, as on the Flipper) */
#define SCR_X 26
#define SCR_Y 80
#define BODY_Y 68  /* The body of the Flipper, below the bubbles */
#define BODY_H 106

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

static void line(uint8_t *fb, int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        gfx_pixel(fb, x0, y0, GFX_BLACK);
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

/* A closed outline: n points (x, y) */
static void polygon(uint8_t *fb, const uint8_t (*p)[2], int n, int dy) {
    for (int i = 0; i < n; ++i) {
        const uint8_t *a = p[i], *b = p[(i + 1) % n];
        line(fb, a[0], a[1] + dy, b[0], b[1] + dy);
    }
}

/* The outline of the Flipper (as the picture of qFlipper, narrowed around its screen drawn 1:1): the point on the
 * left, the cut corner at the top right; y from the top of the body */
static const uint8_t BODY[][2] = {{21, 0}, {186, 0}, {199, 17}, {199, 96},
                                 {195, 102}, {190, 104}, {31, 104}, {1, 51}, {1, 43}};
/* The line inside it, around the screen and the buttons; "FLIPPER" below it */
static const uint8_t INNER[][2] = {{26, 4}, {184, 4}, {195, 19}, {195, 78},
                                  {191, 82}, {23, 82}, {5, 50}, {5, 44}, {22, 8}};

/* The Flipper drawn around its screen: the body, the frame of the screen, the D-pad and the back button */
static void draw_device(uint8_t *fb) {
    polygon(fb, BODY, sizeof(BODY) / sizeof(BODY[0]), BODY_Y);
    polygon(fb, INNER, sizeof(INNER) / sizeof(INNER[0]), BODY_Y);
    /* The edge of the body is thick: a second line inside, along the top and the bottom */
    gfx_fill_rect(fb, 22, BODY_Y + 1, 165, 1, GFX_BLACK);
    gfx_fill_rect(fb, 31, BODY_Y + 103, 160, 1, GFX_BLACK);
    round_rect(fb, SCR_X - 3, SCR_Y - 3, 128 + 6, 64 + 6, 3);
    gfx_rect(fb, SCR_X - 1, SCR_Y - 1, 128 + 2, 64 + 2, GFX_BLACK);
    gfx_text(fb, SCR_X + 64, BODY_Y + 86, &gfx_font_small, "F L I P P E R", GFX_BLACK, GFX_ALIGN_CENTER);
    /* The D-pad: its ring, the OK button, the arrows */
    const int cx = 176, cy = SCR_Y + 31;
    arc(fb, cx, cy, 17, 15);
    arc(fb, cx, cy, 5, 15);
    for (int i = 0; i < 3; ++i) {
        gfx_fill_rect(fb, cx - i, cy - 13 + i, 2 * i + 1, 1, GFX_BLACK);
        gfx_fill_rect(fb, cx - i, cy + 13 - i, 2 * i + 1, 1, GFX_BLACK);
        gfx_fill_rect(fb, cx - 13 + i, cy - i, 1, 2 * i + 1, GFX_BLACK);
        gfx_fill_rect(fb, cx + 13 - i, cy - i, 1, 2 * i + 1, GFX_BLACK);
    }
    /* The LED */
    gfx_fill_rect(fb, 162, SCR_Y + 58, 2, 2, GFX_BLACK);
    /* The back button, with its arrow */
    const int bx = 188, by = SCR_Y + 57;
    arc(fb, bx, by, 6, 15);
    gfx_fill_rect(fb, bx - 2, by - 2, 5, 1, GFX_BLACK);
    gfx_fill_rect(fb, bx + 2, by - 2, 1, 4, GFX_BLACK);
    gfx_fill_rect(fb, bx - 1, by + 1, 4, 1, GFX_BLACK);
    gfx_pixel(fb, bx - 1, by - 3, GFX_BLACK);
    gfx_pixel(fb, bx - 1, by - 1, GFX_BLACK);
}

/* A frame of the animation on the screen of the Flipper, at the bottom as on the Flipper */
static void draw_frame(uint8_t *fb, const anim_io_t *io, const fza_t *a) {
    int stride = (a->w + 7) / 8, y0 = SCR_Y + 64 - a->h, x0 = SCR_X + (128 - a->w) / 2;
    for (int y = 0; y < a->h; ++y)
        for (int x = 0; x < a->w; ++x)
            if (io->frame[y * stride + x / 8] & (0x80 >> (x % 8)))
                gfx_pixel(fb, x0 + x, y0 + y, GFX_BLACK);
}

/* A bubble of text above the Flipper, its tail to \p anchor_x (the dolphin) */
static void draw_bubble(uint8_t *fb, const char *text, int len, int anchor_x) {
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
    const int line_h = gfx_font_small.height + 1, tail = 7;
    int bw = w + 12, bh = n * line_h + 6;
    if (bw > GFX_WIDTH - 2)
        bw = GFX_WIDTH - 2;
    int by = BODY_Y - tail - bh;
    int bx = anchor_x - bw / 2;
    if (bx < 1)
        bx = 1;
    if (bx + bw > GFX_WIDTH - 1)
        bx = GFX_WIDTH - 1 - bw;
    if (anchor_x < bx + 8)
        anchor_x = bx + 8;
    if (anchor_x > bx + bw - 9)
        anchor_x = bx + bw - 9;
    gfx_fill_rect(fb, bx, by, bw, bh, GFX_WHITE);
    round_rect(fb, bx, by, bw, bh, 4);
    for (int i = 0; i < n; ++i)
        gfx_text(fb, bx + 6, by + 3 + i * line_h, &gfx_font_small, lines[i], GFX_BLACK, GFX_ALIGN_LEFT);
    /* The tail: a triangle down to the body of the Flipper */
    for (int dy = 0; dy < tail; ++dy) {
        int half = 5 * (tail - dy) / tail;
        gfx_fill_rect(fb, anchor_x - half + 1, by + bh - 1 + dy, 2 * half - 1, 1, GFX_WHITE);
        gfx_pixel(fb, anchor_x - half, by + bh - 1 + dy, GFX_BLACK);
        gfx_pixel(fb, anchor_x + half, by + bh - 1 + dy, GFX_BLACK);
    }
}

/* Where the bubble of the Flipper points (its corner given by its alignment), on the page */
static int bubble_anchor(const bubble_t *b) {
    /* The size of the bubble on the Flipper: its font is about 5 x 9 pixels */
    int chars = 0, cur = 0;
    for (int i = 0; i < b->len; ++i) {
        cur = b->text[i] == '\n' ? 0 : cur + 1;
        if (cur > chars)
            chars = cur;
    }
    int w = chars * 5 + 6, x;
    switch (b->align & 3) {
    case 0: x = b->x - 4; break;  /* Left: the dolphin on the left of the bubble */
    case 1: x = b->x + w / 2; break;
    default: x = b->x + w + 4; break;  /* Right */
    }
    return SCR_X + x;
}

/* The name of the cicada above the Flipper, when there is no bubble */
static void draw_name(uint8_t *fb) {
    char name[24];
    ui_fit(&gfx_font_medium, name, sizeof(name), social_name(), GFX_WIDTH - 8);
    gfx_text(fb, GFX_WIDTH / 2, (BODY_Y - gfx_font_medium.height) / 2, &gfx_font_medium, name, GFX_BLACK,
             GFX_ALIGN_CENTER);
}

/* ------ The screensaver: the dolphin asleep ------ */

bool flipper_saver_render(uint8_t *fb, uint8_t *scratch, size_t scratch_len) {
    if (! flipper_theme() || scratch_len < sizeof(anim_io_t))
        return false;
    anim_io_t *io = (anim_io_t *)scratch;
    fza_t a;
    gfx_clear(fb, GFX_WHITE);
    draw_device(fb);
    if (fza_open(io, SLEEP_ANIM, &a)) {
        if (fza_read_frame(io, &a, fza_order(io, &a, 0)))
            draw_frame(fb, io, &a);
        fza_close(io);
        draw_bubble(fb, "Zzz...", 6, SCR_X + 90);
    } else {
        draw_name(fb);
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
    draw_device(fb);
    bool bubble = false;
    if (d.open) {
        draw_frame(fb, &DESK->io, &d.a);
        if (d.phase == ACTIVE && d.slot != 0xFF) {
            int t = d.a.passive + d.pos;
            bubble_t b = {0};
            for (int i = 0; i < d.a.n_bubbles && fza_bubble(&DESK->io, &d.a, i, &b); ++i)
                if (b.slot == d.slot && b.start <= t && t <= b.end) {
                    draw_bubble(fb, b.text, b.len, bubble_anchor(&b));
                    bubble = true;
                    break;
                }
        }
    } else {
        /* No animation: what to copy on the card, on the screen of the Flipper */
        ui_scroll_text(fb, SCR_X + 64, SCR_Y + 8, 124, &gfx_font_small, d.no_card ? N_("Pas d'animation :")
                       : N_("Erreur de lecture"), GFX_BLACK, GFX_ALIGN_CENTER);
        if (d.no_card)
            ui_scroll_text(fb, SCR_X + 64, SCR_Y + 28, 124, &gfx_font_small, N_("FLIPPER/ sur la carte SD"),
                           GFX_BLACK, GFX_ALIGN_CENTER);
    }
    if (! bubble)
        draw_name(fb);
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
