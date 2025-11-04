/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

// Include sys/types.h before inttypes.h to work around issue with
// certain versions of GCC and newlib which causes omission of PRIu64
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "badge_pinout.h"
#include "log.h"
#include "screen.h"


/* The send() macro enables writing send(CMD, ARG1, ARG2) but we can't have it in the header nor include it from here,
 * so you have to copy-paste it to use it in tests... */
#define UINT8_LIT(...) (uint8_t[]){__VA_ARGS__}
#define send(...) _send(UINT8_LIT(__VA_ARGS__), sizeof(UINT8_LIT(__VA_ARGS__))/sizeof(uint8_t))


void time_busy(const char *msg) {
    absolute_time_t t0 = get_absolute_time(), t1;
    while(screen_busy())
        tight_loop_contents();
    t1 = get_absolute_time();
    printf("%s done, took %" PRIu64 "µs\n", msg, absolute_time_diff_us(t0, t1));
}


void test_zones44(const uint8_t *ws0, const uint8_t *ws1, const uint8_t *ws2) {
    uint8_t msb[5000], lsb[5000];
    const uint8_t w8 = SCREEN_WIDTH/8;

    /* Prepare 4 bands on lsb */
    /* Prepare 2 bands on msb to have 4 colors or 4 diffs combined with lsb */
    for (size_t j=0; j<SCREEN_HEIGHT; ++j) {
        for (size_t i=0; i<w8; ++i) {
            if ((i/(w8/4))%2) {
                /* Second and fourth quarters are white */
                lsb[j*w8+i] = 0xFF;
            } else {
                /* First and third quarters are black */
                lsb[j*w8+i] = 0x00;
            }
            if (i/(w8/2)%2) {
                /* Second half is white */
                msb[j*w8+i] = 0xFF;
            } else {
                /* First half is black */
                msb[j*w8+i] = 0x00;
            }
        }
    }

    /* Setup multi-frame push */
    screen_start_multiframe();
    while(screen_busy())
        tight_loop_contents();
    send(SSD1681_DISPLAY_CTRL2, 0x04);

    /* Split the screen in 4 horizontal bands.
     * Note: we can only restrict drawing to the lower part of the screen,
     *  so we start by ws0, draw the before image, draw with ws0, set the drawing zone to 3/4,
     *  draw the before image, draw ws1, ...,
     *  draw the 4g reference on the last 1/4 */

    const uint8_t *wss[] = {ws0, ws1, ws2};
    char nth_band[] = "nth band";
    for (size_t i=0; i<3; ++i) {
        if (! wss[i])
            continue;

        /* Set the active zone to the remaining quarters */
        send(SSD1681_DRIVER_CTRL, (SCREEN_HEIGHT*(4-i))/4-1, 0, 0);

        /* Push with B/W waveform as a reset: 2 vertical bands */
        //screen_show_image_bw(msb); -> Can't use this one because we are in multi-frame mode, and exiting twice is bugged in the SSD1681
        screen_push_ws(screen_ws_1681_bw);
        screen_push_rams(msb, msb, (SCREEN_WIDTH*SCREEN_HEIGHT)/8);
        screen_draw_multiframe();
        while(screen_busy())
            tight_loop_contents();

        /* Wait for things to settle */
        sleep_ms(100);

        /* Push the 4 test bands and WS and redraw */
        screen_push_ws(wss[i]);
        screen_push_rams(lsb, msb, (SCREEN_WIDTH*SCREEN_HEIGHT)/8);
        screen_draw_multiframe();
        nth_band[0] = 0x30+i;
        time_busy(nth_band);
    }

    /* Push a 4g on the last quarter */
    send(SSD1681_DRIVER_CTRL, 50-1, 0, 0);
    screen_push_ws(screen_ws_1681_4grays);
    screen_push_rams(lsb, msb, (SCREEN_WIDTH*SCREEN_HEIGHT)/8);
    screen_draw_multiframe();
    time_busy("reference band");

    screen_end_multiframe();
    while(screen_busy())
        tight_loop_contents();

    /* Reset the driver control for whole screen */
    send(SSD1681_DRIVER_CTRL, 200-1, 0, 0);
    printf("exiting\n");
}


/* Have some working configurations to play with */
uint8_t working_ws0[159] = {};
uint8_t working_ws1[159] = {};
uint8_t working_ws2[159] = {};

/* Have some tools to change the configurations in a readable way */
void ws_dump(const uint8_t *ws, const char *name) {
    /* Just hexdump the ws in a usable format */
    size_t offset = 0;

    printf("%s = \\\n", name);
    /* VS_LUTs */
    for (uint8_t lut=0; lut<5; ++lut) {
        printf("\"");
        for(uint8_t group=0; group<12; ++group) {
            printf("\\x%02X", ws[offset]);
            assert(offset == lut*12+group);
            ++offset;
        }
        printf("\" \\\n");
    }

    /* Repetitions */
    for(uint8_t group=0; group<12; ++group) {
        printf("\"");
        for(uint8_t t=0; t<7; ++t) {
            printf("\\x%02X", ws[offset]);
            assert(offset == 5*12+group*7+t);
            ++offset;
        }
        printf("\" /* group %02d */ \\\n", group);
    }

    /* Frequencies */
    printf("\"");
    for (uint8_t dbl=0; dbl<6; ++dbl) {
        printf("\\x%02X", ws[offset]);
        assert(offset == 5*12+12*7+dbl);
        ++offset;
    }
    /* XON */
    printf("\" \"");
    for (uint8_t quad=0; quad<3; ++quad) {
        printf("\\x%02X", ws[offset]);
        assert(offset == 5*12+12*7+6+quad);
        ++offset;
    }
    printf("\" \\\n");

    /* Voltages */
    for (uint8_t v=0; v<6; ++v) {
        printf("\"\\x%02X\" \\\n", ws[offset]);
        assert(offset == 5*12+12*7+6+3+v);
        ++offset;
    }

    /* Omits the trailing ; */
}

typedef enum {
    SKIP_VSS = 0b00,
    BLACK_VSH1 = 0b01,
    WHITE_VSL = 0b10,
    VSH2 = 0b11,  /* VSH2 <= VSH1, so "less black" */
} vs_cols_t;

typedef enum {
    DCVCOM = 0b00,
    VSH1_DCVCOM = 0b01,  /* DVCOM is raised to VSH1 -> more voltage between VSL and VCOM */
    VSL_DVCOM = 0b10,  /* DVCOM is lowered to VSL -> more voltage between VSH1/2 and VCOM */
} vs_vcom_t;


/* When decoding phases, convert B,W or S to their vs_cols_t enum value */
vs_cols_t ctov(char c) {
    switch(c) {
    case 'S':
        return SKIP_VSS;
    case 'B':
        return BLACK_VSH1;
    case 'W':
        return WHITE_VSL;
    case '2':
        return VSH2;
    }
    assert("invalid char from encoded phases, maybe reached the \\0");
}

/* You should always provide more tps than phases... (no stop on this one, we will overrun the buffer) */
void ws_set_phases(uint8_t *ws, const char *phases_col0, const char *phases_col1, const char *phases_col2, const char *phases_col3, const uint8_t *tps) {
    /* We don't set vs_col4, I've not yet tested its effects and may damage the screen */
    /* I'm sorry this function is a mess. The goal is to have a function call that is easier to read.
     * The implementation is not done to outlive this test, hence not very readable/maintainable. */
    assert(ws);
    for (size_t phase=0; phase<48; ++phase) {
        /* Remove the alignment spaces */
        while(*phases_col0 == ' ') ++phases_col0; if(! *phases_col0) break;
        while(*phases_col1 == ' ') ++phases_col1; if(! *phases_col1) break;
        while(*phases_col2 == ' ') ++phases_col2; if(! *phases_col2) break;
        while(*phases_col3 == ' ') ++phases_col3; if(! *phases_col3) break;
        /* Deduce group and make a mask for byte editing, as we only modify 2 bits per byte for the LUTs */
        const uint8_t group = phase/4;
        const uint8_t iphase = phase%4;
        const size_t shift = (3-iphase)*2;
        const uint8_t mask = 0xff ^ (0x03 << shift);
        ws[12*0+group] = (ws[12*0+group] & mask) | (ctov(*phases_col0) << shift);
        ws[12*1+group] = (ws[12*1+group] & mask) | (ctov(*phases_col1) << shift);
        ws[12*2+group] = (ws[12*2+group] & mask) | (ctov(*phases_col2) << shift);
        ws[12*3+group] = (ws[12*3+group] & mask) | (ctov(*phases_col3) << shift);
        /* The structures for repetitions programming is found after the 5 LUTs and composed of 7 bytes:
         * TP[nA], TP[nB], SR[nAB], <- TP =0 means skip whereas other reps =0 means 1 rep
         * TP[nC], TP[nD], SR[nCD], <- TP[nC] and TP[nD] require an increment in offset to skip SR[nAB]
         * RP[n] <- group repetition */
        ws[12*5 + 7*group + iphase + (iphase>1 ? 1:0)] = tps[phase];
        /* Advance phases decoding */
        ++phases_col0; ++phases_col1; ++phases_col2; ++phases_col3;
    }
}

void ws_set_reps_(uint8_t *ws, uint8_t group, uint8_t sr_ab, uint8_t sr_cd, uint8_t rp, uint8_t freq) {
    assert(ws);
    assert(group < 12 && "invalid group selection");
    assert(freq < 9 && "invalid frequency selection");  /* what about 0? */

    /* The structures for repetitions programming is found after the 5 LUTs and composed of 7 bytes:
     * TP[nA], TP[nB], SR[nAB], <- TP =0 means skip whereas other reps =0 means 1 rep
     * TP[nC], TP[nD], SR[nCD],
     * RP[n] <- group repetition */
    const size_t offset = 5*12 + 7*group;
    ws[offset+2] = sr_ab;
    ws[offset+5] = sr_cd;
    ws[offset+6] = rp;

    /* The frequency is a nibble after the 12 structure for repetitions programming,
     * so we must first read the untouched nibble and merge ours */
    const size_t off_req = 5*12 + 7*12 + (group/2);
    uint8_t freq_byte = ws[off_req];
    if (group%2)
        freq_byte = (freq_byte & 0xf0) | freq;
    else
        freq_byte = (freq << 4) | (freq_byte & 0x0f);
    ws[off_req] = freq_byte;
}


void search_quick_white(void) {
    /* Results (most may seem intuitive, but not all):
     * - !! grays are better when whitening black than darkening white !! -> TODO: change ws_4g
     * - ~~activation is better than no activation~~,
     * - just crank the white on,
     * - for activation, slow B/W cycles are better than faster repeated cycles (same frame time),
     * - fast or slow W/B activation cycles are the same,
     * - @30fps: with 2@200Hz + 3@175Hz, we have a dark gray instead of white -> this would only work if white is re-activated,
     * - @20fps: with 4@200Hz + 3@175Hz + 1@150Hz, we have a mid gray instead of white -> still better without activation,
     * - @10fps: with 13@200Hz + 5@175 (93.5ms < 93.9ms budget), we have an almost white -> still better without activation. */
    /* config 0 */
    ws_set_phases(
        working_ws0,
                   /*---group 0---   ---group 1---   ---group 2---*/
                    "S   S   S   S   S   S   S   S   S   S   S   S",
                    "W   B   W   S   W   S   S   S   S   S   S   S",
                    "S   S   S   S   S   S   S   S   S   S   S   S",
                    "S   S   S   S   S   S   S   S   S   S   S   S",
        (uint8_t[]){ 5,  1,  6,  0,  5,  0,  0,  0,  0,  0,  0,  1}
    );
    ws_set_reps_(working_ws0, 0, 0, 0, 0, 8);  /* 8=200Hz */
    ws_set_reps_(working_ws0, 1, 0, 0, 0, 7);
    ws_set_reps_(working_ws0, 2, 0, 0, 0, 8);

    /* config 1 */
    ws_set_phases(
        working_ws1,
                   /*---group 0---   ---group 1---   ---group 2---*/
                    "S   S   S   S   S   S   S   S   S   S   S   S",
                    "W   B   W   S   W   S   S   S   S   S   S   S",
                    "S   S   S   S   S   S   S   S   S   S   S   S",
                    "S   S   S   S   S   S   S   S   S   S   S   S",
        (uint8_t[]){ 0,  1, 11,  0,  5,  0,  0,  0,  0,  0,  0,  1}
    );
    ws_set_reps_(working_ws1, 0, 0, 0, 0, 8);  /* 8=200Hz */
    ws_set_reps_(working_ws1, 1, 0, 0, 0, 7);
    ws_set_reps_(working_ws1, 2, 0, 0, 0, 8);

    /* config 2 */
    /* black activation is better but only once (more repeats => worse results) */
    ws_set_phases(
        working_ws2,
                   /*---group 0---   ---group 1---   ---group 2---*/
                    "S   S   S   S   S   S   S   S   S   S   S   S",
                    "W   B   W   S   W   S   S   S   S   S   S   S",
                    "S   S   S   S   S   S   S   S   S   S   S   S",
                    "S   S   S   S   S   S   S   S   S   S   S   S",
        (uint8_t[]){ 0,  0, 12,  0,  5,  0,  0,  0,  0,  0,  0,  1}
    );
    ws_set_reps_(working_ws2, 0, 0, 0, 0, 8);  /* 8=200Hz */
    ws_set_reps_(working_ws2, 1, 0, 0, 0, 7);
    ws_set_reps_(working_ws2, 2, 0, 0, 0, 8);

    //ws_dump(working_ws0, "working_ws0");
    //ws_dump(working_ws1, "working_ws1");
    //ws_dump(working_ws2, "working_ws2");
    test_zones44(working_ws0, working_ws1, working_ws2);
}

void search_quick_black(void) {
    /* Results (most may seem intuitive, but not all):
     * - ~~activation is better than no activation~~,
     * - just crank the black on,
     * - tried activation to have a more homogeneous black:
     *   - white before or after just makes it more white, not more homogeneous,
     *   - repeated B/W with more black than white just makes it more white, not more homogeneous,
     *   -> the only way to have an homogeneous gray is to go full black for long, then light it up.
     * - @30fps: with 2@200Hz + 3@175Hz, we have a light gray instead of black,
     * - @20fps: with 4@200Hz + 3@175Hz + 1@150Hz, we have a mid-light gray,
     * - @10fps: with 13@200Hz + 5@175 (93.5ms < 93.9ms budget), we have a dark gray, more homogeneous. */
    /* config 0 */
    ws_set_phases(
        working_ws0,
                   /*---group 0---   ---group 1---   ---group 2---*/
                    "S   S   S   S   S   S   S   S   S   S   S   S",
                    "S   S   S   S   S   S   S   S   S   S   S   S",
                    "B   W   B   S   B   S   S   S   S   S   S   S",
                    "S   S   S   S   S   S   S   S   S   S   S   S",
        (uint8_t[]){ 1,  0,  0,  0,  3,  0,  0,  0,  0,  0,  0,  1}
    );
    ws_set_reps_(working_ws0, 0, 0, 0, 0, 8);  /* 8=200Hz */
    ws_set_reps_(working_ws0, 1, 0, 0, 0, 7);
    ws_set_reps_(working_ws0, 2, 0, 0, 0, 8);

    /* config 1 */
    ws_set_phases(
        working_ws1,
                   /*---group 0---   ---group 1---   ---group 2---*/
                    "S   S   S   S   S   S   S   S   S   S   S   S",
                    "S   S   S   S   S   S   S   S   S   S   S   S",
                    "B   W   B   S   B   S   S   S   W   B   S   S",
                    "S   S   S   S   S   S   S   S   S   S   S   S",
        (uint8_t[]){ 1,  0,  0,  0,  3,  0,  0,  0,  0,  3,  0,  1}
    );
    ws_set_reps_(working_ws1, 0, 0, 0, 0, 6);  /* 8=200Hz */
    ws_set_reps_(working_ws1, 1, 0, 0, 0, 7);
    ws_set_reps_(working_ws1, 2, 0, 0, 0, 8);

    /* config 2 */
    /* black activation is better but only once (more repeats => worse results) */
    ws_set_phases(
        working_ws2,
                   /*---group 0---   ---group 1---   ---group 2---*/
                    "S   S   S   S   S   S   S   S   S   S   S   S",
                    "S   S   S   S   S   S   S   S   S   S   S   S",
                    "B   W   B   S   B   S   S   S   W   B   S   S",
                    "S   S   S   S   S   S   S   S   S   S   S   S",
        (uint8_t[]){ 0,  0, 12,  0,  5,  0,  0,  0,  0,  0,  0,  1}
    );
    ws_set_reps_(working_ws2, 0, 0, 0, 0, 8);  /* 8=200Hz */
    ws_set_reps_(working_ws2, 1, 0, 0, 0, 7);
    ws_set_reps_(working_ws2, 2, 0, 0, 0, 8);

    //ws_dump(working_ws0, "working_ws0");
    //ws_dump(working_ws1, "working_ws1");
    //ws_dump(working_ws2, "working_ws2");
    test_zones44(working_ws0, working_ws1, working_ws2);
}

void search_maintain(void) {
    /* These are IN FACT the real deal (just for the ws_dump)
     * - @30fps: with 2@200Hz + 3@175Hz,
     * - @20fps: with 4@200Hz + 3@175Hz + 1@150Hz,
     * - @10fps: with 13@200Hz + 5@175 (93.5ms < 93.9ms budget). */
    /* config 0 */
    ws_set_phases(
        working_ws0,
                   /*---group 0---   ---group 1---   ---group 2---*/
                    "W   S   S   S   B   S   S   S   S   S   S   S",
                    "W   S   S   S   W   S   S   S   S   S   S   S",
                    "B   S   S   S   B   S   S   S   S   S   S   S",
                    "B   S   S   S   W   S   S   S   S   S   S   S",
        (uint8_t[]){ 1,  0,  0,  0,  3,  0,  0,  0,  0,  0,  0,  1}
    );
    ws_set_reps_(working_ws0, 0, 0, 0, 0, 8);  /* 8=200Hz */
    ws_set_reps_(working_ws0, 1, 0, 0, 0, 7);
    ws_set_reps_(working_ws0, 2, 0, 0, 0, 8);

    /* config 1 */
    ws_set_phases(
        working_ws1,
                   /*---group 0---   ---group 1---   ---group 2---*/
                    "B   S   S   S   B   S   S   S   W   B   S   S",
                    "W   S   S   S   W   S   S   S   W   W   S   S",
                    "B   S   S   S   B   S   S   S   B   B   S   S",
                    "W   S   S   S   W   S   S   S   B   W   S   S",
        (uint8_t[]){ 1,  0,  0,  0,  3,  0,  0,  0,  1,  2,  0,  1}
    );
    ws_set_reps_(working_ws1, 0, 0, 0, 0, 6);  /* 8=200Hz */
    ws_set_reps_(working_ws1, 1, 0, 0, 0, 7);
    ws_set_reps_(working_ws1, 2, 0, 0, 0, 8);

    /* config 2 */
    /* black activation is better but only once (more repeats => worse results) */
    ws_set_phases(
        working_ws2,
                   /*---group 0---   ---group 1---   ---group 2---*/
                    "S   W   B   S   B   S   S   S   S   S   S   S",
                    "S   W   W   S   W   S   S   S   S   S   S   S",
                    "S   B   B   S   B   S   S   S   S   S   S   S",
                    "S   B   W   S   W   S   S   S   S   S   S   S",
        (uint8_t[]){ 0,  1, 11,  0,  5,  0,  0,  0,  0,  0,  0,  1}
    );
    ws_set_reps_(working_ws2, 0, 0, 0, 0, 8);  /* 8=200Hz */
    ws_set_reps_(working_ws2, 1, 0, 0, 0, 7);
    ws_set_reps_(working_ws2, 2, 0, 0, 0, 8);

    ws_dump(working_ws0, "screen_ws_30fps");
    ws_dump(working_ws1, "screen_ws_20fps");
    ws_dump(working_ws2, "screen_ws_10fps");
    test_zones44(working_ws0, working_ws1, working_ws2);
}

void search_4g(void) {
    /* The goal is to have flatter grays, because the current ones are "floating" */
    /* config 0 -> old 4g (= reference at that time) */
    ws_set_phases(
        working_ws0,
                   /*---group 0---   ---group 1---   ---group 2---*/
                    "W   W   W   S   B   S   W   S   B   B   B   S",
                    "W   W   S   S   B   S   W   S   B   B   S   S",
                    "W   S   S   S   B   S   W   S   B   S   S   S",
                    "B   B   B   S   B   S   W   S   W   W   W   S",
        (uint8_t[]){ 1,  2,  9,  0,  8,  1,  8,  1,  1,  2,  9,  1}
    );
    ws_set_reps_(working_ws0, 0, 0, 0, 0, 2);  /* 8=200Hz */
    ws_set_reps_(working_ws0, 1, 0, 0, 2, 2);
    ws_set_reps_(working_ws0, 2, 0, 0, 0, 2);

    /* config 1 */
    ws_set_phases(
        working_ws1,
                   /*---group 0---   ---group 1---   ---group 2---*/
                    "W   W   W   S   W   S   B   S   B   B   B   S",
                    "B   S   S   S   W   S   B   S   W   S   S   S",
                    "B   B   S   S   W   S   B   S   W   W   S   S",
                    "B   B   B   S   B   S   W   S   W   W   W   S",
        (uint8_t[]){ 2,  1,  9,  0,  8,  1,  8,  1,  2,  1,  9,  1}
    );
    ws_set_reps_(working_ws1, 0, 0, 0, 0, 2);  /* 8=200Hz */
    ws_set_reps_(working_ws1, 1, 0, 0, 0, 2);
    ws_set_reps_(working_ws1, 2, 0, 0, 0, 2);

    /* config 2 */
    /* Longer reset for better ghost removal */
    ws_set_phases(
        working_ws2,
                   /*---group 0---   ---group 1---   ---group 2---*/
                    "W   W   W   S   W   S   B   S   B   B   B   S",
                    "B   S   S   S   W   S   B   S   W   S   S   S",
                    "B   B   S   S   W   S   B   S   W   W   S   S",
                    "B   B   B   S   B   S   W   S   W   W   W   S",
        (uint8_t[]){ 8,  4, 36,  0,  8,  1,  8,  1,  8,  4, 36,  1}
    );
    ws_set_reps_(working_ws2, 0, 0, 0, 0, 8);  /* 8=200Hz */
    ws_set_reps_(working_ws2, 1, 0, 0, 1, 2);
    ws_set_reps_(working_ws2, 2, 0, 0, 0, 8);

    //ws_dump(working_ws0, "working_ws0");
    //ws_dump(working_ws1, "working_ws1");
    ws_dump(working_ws2, "working_ws2");
    test_zones44(working_ws0, working_ws1, working_ws2);
}


int main() {
    stdio_usb_init();
    log_set_level(LOG_LEVEL_INFO);
    screen_init();
    printf("boot sequence\n");
    while(! screen_boot())
        tight_loop_contents();

    /* Bench test some WS to improve animations */
    //test_zones44(NULL, NULL, NULL);
    //test_zones44(ws_30fps, ws_20fps, NULL);

    //ws_dump(screen_ws_1681_4grays);

    /* Copy the voltage values from a working configuration */
    memcpy(working_ws0+153, screen_ws_1681_4grays+153, 6);  /* Use 4G which has VSH2 set */
    memcpy(working_ws1+153, screen_ws_1681_4grays+153, 6);
    memcpy(working_ws2+153, screen_ws_1681_4grays+153, 6);

    //search_quick_white();
    //search_quick_black();
    search_maintain();
    //search_4g();

    /* It's important to deep sleep because it sometimes have side effects on color stabilisation */
    screen_deep_sleep();
    sleep_ms(1000);
    printf("Screen sleeping, after 1s BUSY = %d\n\n", gpio_get(BADGE_SCREEN_BUSY));
};
