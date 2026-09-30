/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "pico/rand.h"

#include "ff.h"
#include "gfx.h"
#include "oled.h"
#include "oled_demo.h"
#include "screen_demo.h"
#include "sd.h"


#define FRAME_US 33000  /* ~30 fps */
#define EPV_FRAME_SIZE 5000
#define EPV_HEADER_SIZE 512
#define N_STARS 60

static const char *NAMES[OLED_DEMO_COUNT] = {"Étoiles", "Cube 3D", "Cigale", "Texte défilant", "Vidéo (carte SD)"};

static int demo = -1;
static absolute_time_t next_frame = 0;
static uint32_t t = 0;  /* Frame counter */
static uint8_t fb[OLED_FB_SIZE];

/* Starfield */
static int16_t star_x[N_STARS], star_y[N_STARS], star_z[N_STARS];

/* SD video */
static FIL video;
static bool video_open = false;
static uint32_t video_frames = 0;
static uint32_t video_fps = 10;
static uint8_t epv_frame[EPV_FRAME_SIZE];


const char *oled_demo_name(int d) {
    return d >= 0 && d < OLED_DEMO_COUNT ? NAMES[d] : "";
}


static void new_star(int i) {
    star_x[i] = (int16_t)(get_rand_32() % 2000) - 1000;
    star_y[i] = (int16_t)(get_rand_32() % 1000) - 500;
    star_z[i] = 1000;
}


static bool open_video(void) {
    char names[1][SD_NAME_MAX];
    char path[2 * SD_NAME_MAX];
    const char *dir = "VIDEOS";
    if (sd_list_files(dir, ".EPV", names, 1) == 0) {
        dir = "";
        if (sd_list_files(dir, ".EPV", names, 1) == 0)
            return false;
    }
    snprintf(path, sizeof(path), "%s%s%s", dir, dir[0] ? "/" : "", names[0]);
    if (f_open(&video, path, FA_READ) != FR_OK)
        return false;
    uint8_t header[20];
    UINT n;
    if (f_read(&video, header, sizeof(header), &n) != FR_OK || n != sizeof(header) || memcmp(header, "EPVIDEO", 7)) {
        f_close(&video);
        return false;
    }
    video_fps = header[12] | header[13] << 8;
    video_frames = header[16] | header[17] << 8 | header[18] << 16 | (uint32_t)header[19] << 24;
    if (! video_fps)
        video_fps = 10;
    f_lseek(&video, EPV_HEADER_SIZE);
    return true;
}


void oled_demo_start(int d) {
    oled_demo_stop();
    demo = d;
    t = 0;
    next_frame = get_absolute_time();
    if (demo == 0)
        for (int i = 0; i < N_STARS; ++i) {
            new_star(i);
            star_z[i] = 1 + get_rand_32() % 1000;
        }
    if (demo == 4)
        video_open = open_video();
}


void oled_demo_stop(void) {
    if (video_open)
        f_close(&video);
    video_open = false;
    demo = -1;
}


static void draw_stars(void) {
    for (int i = 0; i < N_STARS; ++i) {
        star_z[i] -= 20;
        if (star_z[i] <= 0)
            new_star(i);
        int x = OLED_WIDTH/2 + star_x[i] * 64 / star_z[i];
        int y = OLED_HEIGHT/2 + star_y[i] * 64 / star_z[i];
        if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) {
            new_star(i);
            continue;
        }
        gfx_pixel(fb, x, y, GFX_WHITE);
        if (star_z[i] < 300)  /* Close stars are bigger */
            gfx_fill_rect(fb, x, y, 2, 2, GFX_WHITE);
    }
}


static void draw_line(int x0, int y0, int x1, int y1) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    while (true) {
        gfx_pixel(fb, x0, y0, GFX_WHITE);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}


static void draw_cube(void) {
    static const int8_t v[8][3] = {{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
    static const uint8_t e[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
    float a = t * 0.05f, b = t * 0.037f;
    float ca = cosf(a), sa = sinf(a), cb = cosf(b), sb = sinf(b);
    int px[8], py[8];
    for (int i = 0; i < 8; ++i) {
        float x = v[i][0], y = v[i][1], z = v[i][2];
        float x1 = x * ca - z * sa, z1 = x * sa + z * ca;  /* Rotation around Y */
        float y1 = y * cb - z1 * sb, z2 = y * sb + z1 * cb;  /* Rotation around X */
        float d = 4.f / (z2 + 4.f);  /* Perspective */
        px[i] = OLED_WIDTH/2 + (int)(x1 * d * 22);
        py[i] = OLED_HEIGHT/2 + (int)(y1 * d * 22);
    }
    for (int i = 0; i < 12; ++i)
        draw_line(px[e[i][0]], py[e[i][0]], px[e[i][1]], py[e[i][1]]);
}


/* Draws a 200x200 e-Paper image (1 = white) scaled in a square of \p size pixels, centered, lit where the image is black */
static void draw_epaper_image(const uint8_t *img, int size, int src_y0, int src_h) {
    int x0 = (OLED_WIDTH - size * 200 / src_h) / 2;
    int w = size * 200 / src_h;
    for (int y = 0; y < size; ++y) {
        int sy = src_y0 + y * src_h / size;
        for (int x = 0; x < w; ++x) {
            int sx = x * 200 / w;
            if (! (img[sy * 25 + sx / 8] & (0x80 >> (sx % 8))))
                gfx_pixel(fb, x0 + x, y, GFX_WHITE);
        }
    }
}


static void draw_cicada(void) {
    size_t n;
    const uint8_t *const *frames = screen_demo_cicada_frames(&n);
    draw_epaper_image(frames[(t / 2) % n], OLED_HEIGHT, 0, 200);  /* 15 fps */
}


static void draw_text(void) {
    const char *text = "Badge SecSea - Hack In Provence - Degun soulet es mai fort que toutis ensèn";
    int w = gfx_text_width(&gfx_font_large, text);
    int x = OLED_WIDTH - (int)(t * 2 % (w + OLED_WIDTH));
    gfx_text(fb, x, (OLED_HEIGHT - gfx_font_large.height) / 2, &gfx_font_large, text, GFX_WHITE, GFX_ALIGN_LEFT);
    gfx_fill_rect(fb, 0, 0, OLED_WIDTH, 2, GFX_WHITE);
    gfx_fill_rect(fb, 0, OLED_HEIGHT - 2, OLED_WIDTH, 2, GFX_WHITE);
}


static void draw_video(void) {
    if (! video_open) {
        gfx_text(fb, OLED_WIDTH/2, 12, &gfx_font_small, "Pas de vidéo", GFX_WHITE, GFX_ALIGN_CENTER);
        gfx_text(fb, OLED_WIDTH/2, 34, &gfx_font_small, "sur la carte SD", GFX_WHITE, GFX_ALIGN_CENTER);
        return;
    }
    /* The video runs at its own frame rate: read a new frame every 30/fps demo frames */
    if (t % (30 / video_fps ? 30 / video_fps : 1) == 0) {
        /* Loop at the end of the frames (the sound is stored after them) */
        if (f_tell(&video) >= EPV_HEADER_SIZE + (FSIZE_t)video_frames * EPV_FRAME_SIZE)
            f_lseek(&video, EPV_HEADER_SIZE);
        UINT n;
        if (f_read(&video, epv_frame, EPV_FRAME_SIZE, &n) != FR_OK || n != EPV_FRAME_SIZE)
            f_lseek(&video, EPV_HEADER_SIZE);
    }
    /* The central band of the square video (200x100), scaled to 128x64; here white is lit */
    for (int y = 0; y < OLED_HEIGHT; ++y) {
        int sy = 50 + y * 100 / OLED_HEIGHT;
        for (int x = 0; x < OLED_WIDTH; ++x) {
            int sx = x * 200 / OLED_WIDTH;
            if (epv_frame[sy * 25 + sx / 8] & (0x80 >> (sx % 8)))
                gfx_pixel(fb, x, y, GFX_WHITE);
        }
    }
}


void oled_demo_task(absolute_time_t now) {
    if (demo < 0 || ! oled_present()) {
        oled_task();
        return;
    }
    /* Send the current frame page by page, then draw the next one */
    if (oled_task() || absolute_time_diff_us(now, next_frame) > 0)
        return;
    next_frame = delayed_by_us(now, FRAME_US);

    gfx_set_size(OLED_WIDTH, OLED_HEIGHT);
    gfx_clear(fb, GFX_BLACK);
    switch (demo) {
    case 0: draw_stars(); break;
    case 1: draw_cube(); break;
    case 2: draw_cicada(); break;
    case 3: draw_text(); break;
    case 4: draw_video(); break;
    default: break;
    }
    gfx_set_size(GFX_WIDTH, GFX_HEIGHT);
    oled_show(fb);
    ++t;
}
