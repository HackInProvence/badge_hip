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


/* 30 fps means a budget of 33.3ms - 6.1ms to send an image, leaving 27.2ms hence 5.4 ticks @200Hz or 4.1 @150Hz (better ON time) */
/* TODO: we have to set a VSS phase, so take this into account in the ticks computations */
const uint8_t ws_30fps[159] = \
    "\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* 00 = no touch */ \
    "\x02\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* 01 = lighter */ \
    "\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* 10 = darker */ \
    "\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* 11 = relight white */ \
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* VCOM = DVCOM */ \
    "\x00\x00\x00\x00\x03\x00\x00" /* TP[0A], TP[0B], SR[0AB], TP[0C], TP[0D], SR[0CD], RP[0] */ \
    "\x00\x00\x00\x00\x01\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x01\x00\x00\x00\x00\x00\x00"  /* 11A=1, stabilizes to VSS before power off */ \
    "\x66\x22\x22\x22\x22\x28" "\x00\x00\x00" \
    "\x07"  /* EOPT, 0x22 = normal -> 0x07 helps if there is another image, as 0x22 flashes white on enable analog... */ \
    "\x17"  /*  VGH, 0x17 == 0x00 == 20V */   \
    "\x41"  /* VSH1, 0x41 == 15V */           \
    /* This LUT never uses VSH2 */            \
    "\xA8"  /* VSH2, 0x00 == ???, POR is 5V */\
    "\x32"  /*  VSL, 0x32 == -15V */          \
    "\x20"; /* VCOM, 0x20 == -0.8V */

/* 20 fps means a budget of 50.0ms - 6.1ms to send an image, leaving 43.9ms hence 8.8 ticks @200Hz (any other freq has the same ON time) */
/* TODO -> currently this targets 14ms */
/* TODO -> keep the duplicate in src/tests/screen.c up to date */
const uint8_t ws_20fps[159] = \
    "\x00\x00\x00\x00\x00\x00\x02\x10\x00\x00\x00\x00" /* 00 = redark */ \
    "\x12\x02\x00\x00\x00\x00\x12\x12\x00\x00\x00\x00" /* 01 = lighter */ \
    "\x21\x01\x00\x00\x00\x00\x21\x21\x00\x00\x00\x00" /* 10 = darker */ \
    "\x00\x00\x00\x00\x00\x00\x01\x20\x00\x00\x00\x00" /* 11 = relight white */ \
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* VCOM = DVCOM */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* TP[0A], TP[0B], SR[0AB], TP[0C], TP[0D], SR[0CD], RP[0] */ \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x02\x00\x00\x01\x00\x00" /* group6 */ \
    "\x00\x02\x00\x00\x08\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x01\x00\x00\x00\x00\x00\x00"  /* 11A=1, stabilizes to VSS before power off */ \
    "\x88\x88\x88\x88\x88\x88" "\x00\x00\x00" \
    "\x07"  /* EOPT, 0x22 = normal -> 0x07 helps if there is another image, as 0x22 flashes white on enable analog... */ \
    "\x17"  /*  VGH, 0x17 == 0x00 == 20V */   \
    "\x41"  /* VSH1, 0x41 == 15V */           \
    /* This LUT never uses VSH2 */            \
    "\xA8"  /* VSH2, 0x00 == ???, POR is 5V */\
    "\x32"  /*  VSL, 0x32 == -15V */          \
    "\x20"; /* VCOM, 0x20 == -0.8V */


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
        printf("\"\\x%02X\"", ws[offset]);
        assert(offset == 5*12+12*7+6+3+v);
        ++offset;
        printf("\" \\\n");
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

void ws_set_luts(uint8_t *ws, uint8_t group, uint8_t phase, vs_cols_t vs_col0, vs_cols_t vs_col1, vs_cols_t vs_col2, vs_cols_t vs_col3, vs_vcom_t vcom) {
    assert(ws);
    assert(group < 12 && phase < 4 && "invalid phase or group selection");

    /* We don't set vs_col4, I've not yet tested its effects and may damage the screen */
    /* We write only 2 bits per byte, we need to not modify the other bits */
    const size_t shift = (3-phase)*2;
    const uint8_t mask = 0xff ^ (0x03 << shift);

    ws[group+12*0] = (ws[group+12*0] & mask) | (vs_col0 << shift);
    ws[group+12*1] = (ws[group+12*1] & mask) | (vs_col1 << shift);
    ws[group+12*2] = (ws[group+12*2] & mask) | (vs_col2 << shift);
    ws[group+12*3] = (ws[group+12*3] & mask) | (vs_col3 << shift);
    ws[group+12*4] = (ws[group+12*4] & mask) | (vcom    << shift);
}

void ws_set_reps(uint8_t *ws, uint8_t group, uint8_t freq, uint8_t tp_a, uint8_t tp_b, uint8_t tp_c, uint8_t tp_d, uint8_t sr_ab, uint8_t sr_cd, uint8_t rp) {
    assert(ws);
    assert(group < 12 && "invalid group selection");
    assert(freq < 9 && "invalid frequency selection");  /* what about 0? */

    /* The structures for repetitions programming is found after the 5 LUTs and composed of 7 bytes:
     * TP[nA], TP[nB], SR[nAB], <- TP =0 means skip whereas other reps =0 means 1 rep
     * TP[nC], TP[nD], SR[nCD],
     * RP[n] <- group repetition */
    const size_t offset = 5*12 + 7*group;
    ws[offset+0] = tp_a;
    ws[offset+1] = tp_b;
    ws[offset+2] = sr_ab;
    ws[offset+3] = tp_c;
    ws[offset+4] = tp_d;
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


typedef struct {
    vs_cols_t vs;
    uint8_t tp;
    //uint8_t freq;
} phase_t;
#define PHASE_ZERO (phase_t){SKIP_VSS, 0}
bool phase_equals(phase_t *a, phase_t *b) {
    return a->vs == b->vs && a->tp == b->tp/* && a->freq == b-> freq */;
}

/* Writes the 0-terminated phases into config for LUT index col */
void ws_set(uint8_t *ws, uint8_t col, phase_t *phases) {
    assert(col<5 && "there are only 5 LUTs");
    for (size_t i=0; !phase_equals(&phases[i], &PHASE_ZERO); ++i) {
        assert(i<12 && "too much phases");
        /* Change the voltage select (LUT) for this color */
        uint8_t group = i/4;
        uint8_t phase = i%4;
        const size_t shift = (3-phase)*2;
        const uint8_t mask = 0xff ^ (0x03 << shift);
        ws[12*col+group] = (ws[12*col+group] & mask) | (phases[i].vs << shift);
        /* Change TP, and TP[nA] and TP[nB] are at indices 0 and 1 (i.e. phase), while TP[nC] and TP[nD] are at indices 2 and 3 (i.e. phase+1) */
        ws[12*5 + 7*group + phase + (phase>2 ? 1:0)] = phases[i].tp;
        ///* Change frequency -> by groups */
        //const size_t off_req = 12*5 + 7*12 + (group/2);
        //if (group%2)
        //    ws[off_req] = (ws[off_req] & 0xf0) | phases[i].freq;
        //else
        //    ws[off_req] = (phases[i].freq << 4) | (ws[off_req] & 0x0f);
    }
}


void search_quick_white(void) {
    /* config 0 */
    ws_set(working_ws0, 0, (phase_t[]){  /* black -> black */
        {SKIP_VSS, 1},
        PHASE_ZERO,
    });
    ws_set(working_ws0, 1, (phase_t[]){  /* black -> white */
        {WHITE_VSL, 8},
        {SKIP_VSS, 8},
        PHASE_ZERO,
    });
    ws_set(working_ws0, 2, (phase_t[]){  /* white -> black */
        {SKIP_VSS, 1},
        PHASE_ZERO,
    });
    ws_set(working_ws0, 3, (phase_t[]){  /* white -> white */
        {SKIP_VSS, 1},
        PHASE_ZERO,
    });
    memset(working_ws0+12*5+7*12, 0x88, 6);  /* set frequency to 200Hz */

    /* config 1 */
    ws_set_luts(working_ws1, 11, 0, SKIP_VSS, SKIP_VSS, SKIP_VSS, SKIP_VSS, DCVCOM);
    ws_set_reps(working_ws1, 11, 8, 1, 0, 0, 0, 0, 0, 0);

    //ws_set_luts(working_ws1, 1, 0, SKIP_VSS, BLACK_VSH1, SKIP_VSS, SKIP_VSS, DCVCOM);
    //ws_set_reps(working_ws1, 1, 8, 4, 0, 0, 0, 0, 0, 0);
    //ws_set_luts(working_ws1, 2, 0, SKIP_VSS, WHITE_VSL, SKIP_VSS, SKIP_VSS, DCVCOM);
    //ws_set_reps(working_ws1, 2, 8, 4, 0, 0, 0, 0, 0, 0);
    ws_set_luts(working_ws1, 1, 0, SKIP_VSS, BLACK_VSH1, SKIP_VSS, SKIP_VSS, DCVCOM);
    ws_set_luts(working_ws1, 1, 1, SKIP_VSS, WHITE_VSL, SKIP_VSS, SKIP_VSS, DCVCOM);
    ws_set_reps(working_ws1, 1, 8, 4, 4, 0, 0, 0, 0, 1);

    /* config 2 */
    ws_set_luts(working_ws2, 11, 0, SKIP_VSS, SKIP_VSS, SKIP_VSS, SKIP_VSS, DCVCOM);
    ws_set_reps(working_ws2, 11, 8, 1, 0, 0, 0, 0, 0, 0);

    ws_set_luts(working_ws2, 1, 0, SKIP_VSS, BLACK_VSH1, SKIP_VSS, SKIP_VSS, DCVCOM);
    ws_set_luts(working_ws2, 1, 1, SKIP_VSS, WHITE_VSL, SKIP_VSS, SKIP_VSS, DCVCOM);
    ws_set_reps(working_ws2, 1, 8, 4, 4, 0, 0, 0, 0, 2);
    //ws_set_luts(working_ws2, 2, 0, SKIP_VSS, WHITE_VSL, SKIP_VSS, SKIP_VSS);
    //ws_set_reps(working_ws2, 2, 8, 4, 0, 0, 0, 0, 0, 0);

    ws_dump(working_ws0, "working_ws0");
    //ws_dump(working_ws0, "working_ws0");
    //ws_dump(working_ws0, "working_ws0");
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

    search_quick_white();

    /* It's important to deep sleep because it sometimes have side effects on color stabilisation */
    screen_deep_sleep();
    sleep_ms(1000);
    printf("Screen sleeping, after 1s BUSY = %d\n\n", gpio_get(BADGE_SCREEN_BUSY));
};
