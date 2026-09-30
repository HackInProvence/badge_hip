/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Image by radio: a badge sends an image of its gallery (or the SecSea), dithered to black and white (5000 bytes),
 * the badges in receive mode show it as it arrives.
 * 105 blocks of 48 bytes, plus a parity block (XOR) per group of 8 blocks: a lost block of a group is rebuilt
 * from the other 7 and the parity (forward error correction, no need to ask again).
 * NET_IMAGE [transfer 2][block][48 bytes]: blocks 0-104 = the image, 105-118 = the parities of the groups. */

#include <stdio.h>
#include <string.h>

#include "pico/rand.h"

#include "app.h"
#include "ff.h"
#include "gfx.h"
#include "net.h"
#include "remote.h"
#include "screen_demo.h"
#include "sd.h"

#define BLOCK 48
#define DATA_BLOCKS ((GFX_FB_SIZE + BLOCK - 1) / BLOCK)  /* 105 */
#define GROUP 8
#define GROUPS ((DATA_BLOCKS + GROUP - 1) / GROUP)  /* 14 */
#define ALL_BLOCKS (DATA_BLOCKS + GROUPS)
#define SEND_GAP_MS 70  /* A block is ~53 ms on air: the queue keeps room for the other features */
#define MAX_IMAGES 24

static uint8_t image[DATA_BLOCKS * BLOCK];  /* 1 bit per pixel, 1 = white, padded */
static uint8_t parity[GROUPS][BLOCK];
static uint8_t have[(ALL_BLOCKS + 7) / 8];
static uint16_t transfer = 0;
static uint32_t sender = 0;
static int received = 0, rebuilt = 0;
static bool changed = false;
static bool receiving = false;
/* Sending */
static int send_block = -1;  /* -1: not sending */
static int rounds_left = 0;
static absolute_time_t send_ts = 0;
static char files[MAX_IMAGES][SD_NAME_MAX];
static int n_files = 0, sel = 0;

static bool has(int b) {
    return have[b / 8] & (1 << (b % 8));
}

static void set_has(int b) {
    have[b / 8] |= 1 << (b % 8);
}

/* A missing block of a group is rebuilt from the others and the parity */
static void rebuild(int g) {
    int missing = -1, n = 0;
    int last = (g + 1) * GROUP < DATA_BLOCKS ? (g + 1) * GROUP : DATA_BLOCKS;
    for (int b = g * GROUP; b < last; ++b)
        if (! has(b)) {
            missing = b;
            ++n;
        }
    if (n != 1 || ! has(DATA_BLOCKS + g))
        return;
    uint8_t *m = image + missing * BLOCK;
    memcpy(m, parity[g], BLOCK);
    for (int b = g * GROUP; b < last; ++b)
        if (b != missing)
            for (int i = 0; i < BLOCK; ++i)
                m[i] ^= image[b * BLOCK + i];
    set_has(missing);
    ++rebuilt;
    printf("image: block %d rebuilt from the parity\n", missing);
}

static void handle_image(const net_packet_t *p) {
    if (! receiving || p->len < 3 + BLOCK)
        return;
    uint16_t t = p->data[0] | p->data[1] << 8;
    int b = p->data[2];
    if (b >= ALL_BLOCKS)
        return;
    if (t != transfer || p->src != sender) {
        /* A new image */
        transfer = t;
        sender = p->src;
        memset(image, 0, sizeof(image));
        memset(have, 0, sizeof(have));
        received = rebuilt = 0;
        printf("image: receiving from %08lX\n", (unsigned long)p->src);
    }
    if (has(b))
        return;
    memcpy(b < DATA_BLOCKS ? image + b * BLOCK : parity[b - DATA_BLOCKS], p->data + 3, BLOCK);
    set_has(b);
    ++received;
    rebuild(b < DATA_BLOCKS ? b / GROUP : b - DATA_BLOCKS);
    if (b < DATA_BLOCKS && b % GROUP == GROUP - 1)
        changed = true;  /* Redraw now and then */
    int n = 0;
    for (int i = 0; i < DATA_BLOCKS; ++i)
        n += has(i);
    if (n == DATA_BLOCKS) {
        changed = true;
        printf("image: complete (%d packets, %d rebuilt)\n", received, rebuilt);
    }
}

void image_radio_init(void) {
    net_subscribe(NET_IMAGE, handle_image);
}

static int data_blocks_ok(void) {
    int n = 0;
    for (int i = 0; i < DATA_BLOCKS; ++i)
        n += has(i);
    return n;
}

/* ------ Load an image to send ------ */

static void dither(uint8_t *lsb, const uint8_t *msb) {
    static const uint8_t THRESHOLD[2][2] = {{1, 3}, {3, 2}};
    for (int y = 0; y < GFX_HEIGHT; ++y)
        for (int bx = 0; bx < GFX_WIDTH / 8; ++bx) {
            int i = y * (GFX_WIDTH / 8) + bx;
            uint8_t out = 0;
            for (int k = 0; k < 8; ++k) {
                uint8_t mask = 0x80 >> k;
                int gray = (msb[i] & mask ? 2 : 0) + (lsb[i] & mask ? 1 : 0);
                if (gray >= THRESHOLD[y & 1][k & 1])
                    out |= mask;
            }
            lsb[i] = out;
        }
}

static bool load(int index) {
    static uint8_t msb[GFX_FB_SIZE];
    memset(image, 0xFF, sizeof(image));
    if (index < 0 || index >= n_files) {
        const uint8_t *l, *m;
        screen_demo_secsea_4g(&l, &m);
        memcpy(image, l, GFX_FB_SIZE);
        dither(image, m);
        return true;
    }
    char path[SD_NAME_MAX + 10];
    snprintf(path, sizeof(path), "IMAGES/%s", files[index]);
    FIL f;
    UINT n;
    uint8_t header[16];
    if (sd_mount() != FR_OK || f_open(&f, path, FA_READ) != FR_OK)
        return false;
    bool ok = f_read(&f, header, 16, &n) == FR_OK && n == 16 && ! memcmp(header, "EPIMAGE1", 8)
              && f_read(&f, image, GFX_FB_SIZE, &n) == FR_OK && n == GFX_FB_SIZE;
    int bpp = header[12];
    if (ok && bpp == 2)
        ok = f_read(&f, msb, GFX_FB_SIZE, &n) == FR_OK && n == GFX_FB_SIZE;
    f_close(&f);
    if (ok && bpp == 2)
        dither(image, msb);
    return ok;
}

static void compute_parity(void) {
    memset(parity, 0, sizeof(parity));
    for (int b = 0; b < DATA_BLOCKS; ++b)
        for (int i = 0; i < BLOCK; ++i)
            parity[b / GROUP][i] ^= image[b * BLOCK + i];
}

static void send_task(absolute_time_t now) {
    if (send_block < 0 || absolute_time_diff_us(send_ts, now) < 0)
        return;
    uint8_t d[3 + BLOCK] = {transfer, transfer >> 8, send_block};
    memcpy(d + 3, send_block < DATA_BLOCKS ? image + send_block * BLOCK : parity[send_block - DATA_BLOCKS], BLOCK);
    if (! net_send(NET_IMAGE, d, sizeof(d), NET_LOUD))
        return;
    send_ts = delayed_by_ms(now, SEND_GAP_MS);
    if (++send_block >= ALL_BLOCKS) {
        if (--rounds_left > 0) {
            send_block = 0;
        } else {
            send_block = -1;
            remote_pause_windows(false);
            printf("image: sent\n");
        }
        changed = true;
    } else if (send_block % 20 == 0) {
        changed = true;
    }
}

/* ------ Pages: send (choose an image), receive ------ */

static void file_label(int i, char *buf, size_t len) {
    if (i == 0)
        snprintf(buf, len, "SecSea (intégrée)");
    else
        snprintf(buf, len, "%s", files[i - 1]);
}

static void send_start(absolute_time_t now) {
    (void)now;
    n_files = sd_mount() == FR_OK ? (int)sd_list_files("IMAGES", ".EPI", files, MAX_IMAGES) : 0;
    sel = 0;
}

static bool send_buttons(const app_buttons_t *b, absolute_time_t now) {
    if (b->pressed & UI_BTN_A) {
        if (send_block >= 0) {
            send_block = -1;  /* Stop */
            remote_pause_windows(false);
            return true;
        }
        return false;
    }
    if (send_block >= 0)
        return true;
    if (b->pressed & UI_BTN_Y)
        sel = (sel + n_files) % (n_files + 1);
    if (b->pressed & UI_BTN_X)
        sel = (sel + 1) % (n_files + 1);
    if ((b->pressed & UI_BTN_B) && load(sel - 1)) {
        compute_parity();
        transfer = (get_rand_32() & 0xFFFE) + 1;
        send_block = 0;
        rounds_left = 2;  /* Twice: who missed a block the first time gets it the second time */
        send_ts = now;
        remote_pause_windows(true);
        printf("image: sending %s\n", sel ? files[sel - 1] : "SecSea");
    }
    return true;
}

static bool send_task_page(absolute_time_t now) {
    send_task(now);
    bool c = changed;
    changed = false;
    return c;
}

static void send_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    if (send_block >= 0) {
        memcpy(fb, image, GFX_FB_SIZE);  /* The image being sent */
        gfx_fill_rect(fb, 0, GFX_HEIGHT - 20, GFX_WIDTH, 20, GFX_WHITE);
        gfx_text(fb, GFX_WIDTH/2, GFX_HEIGHT - 18, &gfx_font_small, rounds_left > 1 ? "Envoi (1/2)... G : arrêter" :
                 "Envoi (2/2)... G : arrêter", GFX_BLACK, GFX_ALIGN_CENTER);
        return;
    }
    ui_title(fb, "Envoyer une image");
    ui_list(fb, n_files + 1, sel, file_label);
    ui_footer(fb, "G : retour  D : envoyer");
}

const app_t app_image_send = {
    .name = "Envoyer une image",
    .start = send_start,
    .buttons = send_buttons,
    .task = send_task_page,
    .render = send_render,
    .no_saver = true,
};

static void recv_start(absolute_time_t now) {
    (void)now;
    receiving = true;
    remote_pause_windows(true);  /* All the packets */
}

static void recv_stop(void) {
    receiving = false;
    remote_pause_windows(false);
}

static bool recv_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_B) {
        memset(have, 0, sizeof(have));  /* Wait for another image */
        transfer = 0;
        received = rebuilt = 0;
    }
    return ! (b->pressed & UI_BTN_A);
}

static bool recv_task(absolute_time_t now) {
    (void)now;
    bool c = changed;
    changed = false;
    return c;
}

static void recv_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    int ok = data_blocks_ok();
    if (! transfer) {
        ui_title(fb, "Recevoir une image");
        ui_lines(fb, 50, &gfx_font_small, "En attente d'une image...\nSur l'autre badge :\nRadio & IR > Envoyer\nune image.");
        ui_footer(fb, "G : retour");
        return;
    }
    /* The image, the missing blocks in gray */
    memcpy(fb, image, GFX_FB_SIZE);
    for (int b = 0; b < DATA_BLOCKS; ++b)
        if (! has(b))
            for (int i = 0; i < BLOCK && b * BLOCK + i < GFX_FB_SIZE; ++i)
                fb[b * BLOCK + i] = ((b * BLOCK + i) / (GFX_WIDTH / 8)) % 2 ? 0xAA : 0x55;
    char text[48];
    if (ok == DATA_BLOCKS)
        snprintf(text, sizeof(text), "Reçue ! (%d bloc%s corrigé%s)", rebuilt, rebuilt > 1 ? "s" : "", rebuilt > 1 ? "s" : "");
    else
        snprintf(text, sizeof(text), "%d / %d blocs", ok, DATA_BLOCKS);
    gfx_fill_rect(fb, 0, GFX_HEIGHT - 20, GFX_WIDTH, 20, GFX_WHITE);
    gfx_text(fb, GFX_WIDTH/2, GFX_HEIGHT - 18, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
}

const app_t app_image_recv = {
    .name = "Recevoir une image",
    .start = recv_start,
    .buttons = recv_buttons,
    .task = recv_task,
    .render = recv_render,
    .stop = recv_stop,
    .no_saver = true,
};
