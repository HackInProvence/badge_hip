/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "credits.h"
#include "gfx.h"

#define TITLE_H 28
#define FOOTER_Y 180

/* The logos of the screen demo (tests/imgs, built by badge_images and defined in screen_demo.c) */
extern const uint8_t hip_bw[], secsea_bw[];

typedef struct {
    const uint8_t *logo;  /* 200x200 image drawn at half size (organizations), NULL = the initials in a disc */
    uint8_t logo_y0, logo_y1;  /* Rows of the logo to show */
    uint8_t logo_split;  /* Left of this column, the thin lines are black (kept at half size), right of it white */
    bool logo_band;  /* Black band behind the logo (its background is black) */
    const char *initials;
    const char *name;
    const char *role;  /* Lines separated by '\n' */
    const char *link;  /* Where to find them, "" = none */
} credit_t;

/* The contacts are the public pages that each one chose to show (GitHub account of the commits, association sites) */
static const credit_t CREDITS[] = {
    {hip_bw, 40, 148, 140, true, NULL, "Hack In Provence", "L'association qui soutient\nle projet et organise SecSea.", "hackinprovence.fr"},
    {secsea_bw, 18, 172, 200, false, NULL, "SecSea 2026", "L'année de sortie du badge.", "x.com/SecSeaConf"},
    {NULL, 0, 0, 0, false, "M", "Miaou", "A posé toutes les bases\ndu projet : écran, radio,\nson, LEDs, outils.", "github.com/Miaou"},
    {NULL, 0, 0, 0, false, "PG", "Paul Ganelon", "A poussé le projet, avec\nl'auteur du premier design\ndu badge.", "linkedin.com/in/paulganelon"},
    {NULL, 0, 0, 0, false, "CC", "Christophe Chaloin", "Design électronique\n(schéma KiCad V1.1).", ""},
    {NULL, 0, 0, 0, false, "C", "Cédric", "A refait le design\ndu circuit imprimé.", ""},
    {NULL, 0, 0, 0, false, "T", "Tristus1er", "Le code des applications :\nmenus, médias, jeux,\nradio, extensions.", "github.com/Tristus1er"},
    {NULL, 0, 0, 0, false, "+", "Et vous tous !", "Toutes les personnes qui\nont aidé et que l'on aurait\npu oublier. Merci !", "github.com/HackInProvence"},
};
#define N_CREDITS ((int)(sizeof(CREDITS) / sizeof(CREDITS[0])))

int credits_count(void) {
    return N_CREDITS;
}

const char *credits_name(int page) {
    return page >= 0 && page < N_CREDITS ? CREDITS[page].name : "";
}

/* Rows [y0, y1) of a 200x200 image (image2epaper format: 1 = white) at half size, from y.
 * The lines are thin: each pixel of 4 source pixels takes the color of the lines when one of them has it,
 * black left of the column \p split, white right of it (the head of the HIP cicada is white lines on black).
 * Returns the height drawn. */
static int draw_half(uint8_t *fb, const uint8_t *img, int y0, int y1, int split, bool band, int y) {
    int h = (y1 - y0) / 2, x0 = (GFX_WIDTH - 100) / 2;
    if (band)
        gfx_fill_rect(fb, 0, y - 2, GFX_WIDTH, h + 4, GFX_BLACK);
    for (int oy = 0; oy < h; ++oy)
        for (int ox = 0; ox < 100; ++ox) {
            int whites = 0;
            for (int k = 0; k < 4; ++k) {
                int sx = 2*ox + (k & 1), sy = y0 + 2*oy + (k >> 1);
                whites += (img[sy * 25 + sx / 8] & (0x80 >> (sx % 8))) != 0;
            }
            bool white = 2*ox < split ? whites == 4 : whites > 0;
            gfx_pixel(fb, x0 + ox, y + oy, white ? GFX_WHITE : GFX_BLACK);
        }
    return h;
}

/* Initials in a black disc, for the people */
static void draw_monogram(uint8_t *fb, int cy, const char *initials) {
    int r = 19;
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx)
            if (dx*dx + dy*dy <= r*r + r)
                gfx_pixel(fb, GFX_WIDTH/2 + dx, cy + dy, GFX_BLACK);
    gfx_text(fb, GFX_WIDTH/2, cy - gfx_font_medium.height/2, &gfx_font_medium, initials, GFX_WHITE, GFX_ALIGN_CENTER);
}

void credits_render(uint8_t *fb, int page) {
    if (page < 0 || page >= N_CREDITS)
        page = 0;
    const credit_t *c = &CREDITS[page];
    char title[24];
    gfx_clear(fb, GFX_WHITE);
    gfx_fill_rect(fb, 0, 0, GFX_WIDTH, TITLE_H, GFX_BLACK);
    snprintf(title, sizeof(title), "Crédits %d/%d", page + 1, N_CREDITS);
    gfx_text(fb, GFX_WIDTH/2, (TITLE_H - gfx_font_medium.height)/2, &gfx_font_medium, title, GFX_WHITE, GFX_ALIGN_CENTER);

    /* The organizations have the logo of the cicada, the people their initials */
    int y;
    if (c->logo) {
        y = TITLE_H + 4;
        y += draw_half(fb, c->logo, c->logo_y0, c->logo_y1, c->logo_split, c->logo_band, y) + 5;
    } else {
        draw_monogram(fb, TITLE_H + 23, c->initials);
        y = TITLE_H + 45;
    }
    const gfx_font_t *font = &gfx_font_medium;
    gfx_text(fb, GFX_WIDTH/2, y, font, c->name, GFX_BLACK, GFX_ALIGN_CENTER);
    y += font->height + 2;

    char line[48];
    for (const char *t = c->role; *t; ) {
        const char *end = strchr(t, '\n');
        size_t n = end ? (size_t)(end - t) : strlen(t);
        if (n >= sizeof(line))
            n = sizeof(line) - 1;
        memcpy(line, t, n);
        line[n] = 0;
        gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, line, GFX_BLACK, GFX_ALIGN_CENTER);
        y += gfx_font_small.height + 1;
        t += end ? (size_t)(end - t) + 1 : n;
    }
    if (c->link[0]) {
        int w = gfx_text_width(&gfx_font_small, c->link);
        gfx_text(fb, GFX_WIDTH/2, FOOTER_Y - gfx_font_small.height - 5, &gfx_font_small, c->link, GFX_BLACK, GFX_ALIGN_CENTER);
        gfx_fill_rect(fb, (GFX_WIDTH - w)/2, FOOTER_Y - 5, w, 1, GFX_BLACK);  /* Underlined, like a link */
    }

    gfx_fill_rect(fb, 0, FOOTER_Y - 2, GFX_WIDTH, 1, GFX_BLACK);
    gfx_text(fb, GFX_WIDTH/2, FOOTER_Y, &gfx_font_small, "Flancs : défiler  G : retour", GFX_BLACK, GFX_ALIGN_CENTER);
}
