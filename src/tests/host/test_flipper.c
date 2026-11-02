/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Test of the Flipper theme (flipper.c) with the animations of docs/sd/FLIPPER: run from docs/sd (run_tests.py).
 * Every animation is played (no read error, the dolphin on the screen of the Flipper, the bubbles), then the
 * screensaver. With the environment variable FLIPPER_DUMP=<dir>: pages saved as PBM images, to look at them. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test.h"

#include "flipper.h"
#include "gfx.h"
#include "store.h"
#include "ui.h"

uint64_t host_time_us = 1000000;
store_t host_store;
const char *host_name = "";
bool host_battery_low = false;
uint8_t host_level = 1;

uint32_t get_rand_32(void) {
    static uint32_t s = 12345;
    s = s * 1103515245u + 12345u;
    return s >> 8;
}

static uint8_t fb[GFX_FB_SIZE];
static uint8_t scratch[FLIPPER_SAVER_SCRATCH];

/* Black pixels in a rectangle (1 = white in the frame buffer) */
static int black(int x0, int y0, int w, int h) {
    int n = 0;
    for (int y = y0; y < y0 + h; ++y)
        for (int x = x0; x < x0 + w; ++x)
            if (! (fb[y * (GFX_WIDTH / 8) + x / 8] & (0x80 >> (x % 8))))
                ++n;
    return n;
}

static void dump(const char *name) {
    const char *dir = getenv("FLIPPER_DUMP");
    if (! dir)
        return;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.pbm", dir, name);
    FILE *f = fopen(path, "wb");
    if (! f)
        return;
    fprintf(f, "P4\n%d %d\n", GFX_WIDTH, GFX_HEIGHT);
    for (int i = 0; i < GFX_FB_SIZE; ++i)
        fputc(fb[i] ^ 0xFF, f);  /* PBM: 1 = black */
    fclose(f);
}

static void row_label(int i, char *buf, size_t len) {
    static const char *const ROWS[] = {"Veille : 5 min", "Langue : Français", "Thème : Flipper Zero",
                                       "Télécommande : oui", "Mode muet : non", "Infos", "Crédits", "Radio", "Batterie"};
    snprintf(buf, len, "%s", ROWS[i]);
}

static void render(void) {
    app_flipper.render(fb, host_time_us);
}

static app_buttons_t press(uint8_t bit) {
    app_buttons_t b;
    memset(&b, 0, sizeof(b));
    b.pressed = bit;
    return b;
}

int main(void) {
    /* The theme */
    CHECK(! flipper_theme());
    CHECK(! flipper_saver_render(fb, scratch, sizeof(scratch)));
    flipper_set_theme(true);
    CHECK(flipper_theme());
    CHECK(ui_flipper);

    /* The desktop: every animation, a bubble in some of them */
    app_flipper.start(host_time_us);
    render();
    dump("desktop");
    CHECK(black(10, 88, 128, 64) > 50);  /* The dolphin on the screen of the Flipper */
    int animations = 0, bubbles = 0, empty_frames = 0, dumped = 0;
    for (int a = 0; a < 40; ++a) {
        bool bubble = false;
        for (int step = 0; step < 400; ++step) {
            host_time_us += 500000;  /* 2 frames per second */
            app_flipper.task(host_time_us);
            render();
            if (black(10, 88, 128, 64) < 20)
                ++empty_frames;
            if (black(0, 0, GFX_WIDTH, 60)) {  /* No name: something above the Flipper is a bubble */
                bubble = true;
                if (dumped < 6 && step % 50 == 0) {
                    char name[32];
                    snprintf(name, sizeof(name), "bubble_%d", dumped++);
                    dump(name);
                }
            }
        }
        bubbles += bubble;
        ++animations;
        app_buttons_t b = press(UI_BTN_X);
        CHECK(app_flipper.buttons(&b, host_time_us));  /* Next animation */
    }
    printf("%d animations played, %d with a bubble, %d frames almost empty\n", animations, bubbles, empty_frames);
    CHECK(bubbles >= 10);
    CHECK(empty_frames < 200);  /* Some frames are almost empty (the dolphin leaves the screen), not many */
    app_buttons_t b = press(UI_BTN_A);
    CHECK(app_flipper.buttons(&b, host_time_us));  /* A stays on the desktop */
    b = press(UI_BTN_B);
    CHECK(! app_flipper.buttons(&b, host_time_us));  /* B: the menu */
    app_flipper.stop();

    /* The name of the cicada above the Flipper */
    host_name = "Cigale42";
    app_flipper.start(host_time_us);
    render();
    CHECK(black(0, 0, GFX_WIDTH, 60) > 0);
    app_flipper.stop();

    /* The low battery: its animation */
    host_battery_low = true;
    app_flipper.start(host_time_us);
    render();
    dump("battery");
    CHECK(black(10, 88, 128, 64) > 50);
    app_flipper.stop();
    host_battery_low = false;

    /* The screensaver: the dolphin asleep */
    CHECK(flipper_saver_render(fb, scratch, sizeof(scratch)));
    dump("saver");
    CHECK(black(10, 88, 128, 64) > 50);
    CHECK(black(0, 0, GFX_WIDTH, 60) > 0);  /* "Zzz..." */

    /* A list in the style of the Flipper: black on white title, rounded selected row */
    gfx_clear(fb, GFX_WHITE);
    ui_title(fb, "Réglages");
    ui_list(fb, 9, 2, row_label);
    ui_footer(fb, "Y X : choix  A : retour  B : OK");
    dump("menu");
    CHECK(black(0, 0, GFX_WIDTH, 20) < 600);  /* Not a black title bar */
    CHECK(black(2, UI_TITLE_H + 3 + 2 * UI_ROW_H, 1, 1) == 0);  /* The rounded corner of the selected row */

    /* Back to the SecSea theme */
    flipper_set_theme(false);
    CHECK(! ui_flipper);

    printf("%d passed, %d failed\n", test_passes, test_failures);
    return test_failures ? 1 : 0;
}
