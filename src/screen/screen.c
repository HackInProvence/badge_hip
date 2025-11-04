/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */


// Include sys/types.h before inttypes.h to work around issue with
// certain versions of GCC and newlib which causes omission of PRIu64
#include <sys/types.h>
#include <inttypes.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/binary_info.h"
#include "pico/time.h"

#include "badge_pinout.h"
#include "log.h"
#include "screen.h"


/* Internal (for now) state description to handle boot sequence */
typedef enum {
    STATE_UNINIT = 0,
    STATE_SLEEP = 1, /* Or boot */
    STATE_HWRESET = 2,  /* Hardware reset ongoing */
    STATE_SWRESET = 3,  /* Software reset ongoing */
    STATE_SETUP = 4,  /* Does a BUSY operation */
    STATE_READY = 5,  /* You can send commands (if not busy) */
    STATE_MULTIFRAME = 6,  /* You can send some commands (or exit the mode) */
} state_t;
STATIC state_t state = STATE_UNINIT;
STATIC absolute_time_t state_ts = 0;  /* Last time the state changed */


/* Send data on the SPI but don't wait for BUSY to be LOW */
STATIC void _send(const uint8_t *cmd, size_t len) {
    if(! cmd || len == 0)
        return;

    gpio_put(BADGE_SCREEN_DC, 0);  /* Low for commands, high for data */
    spi_write_blocking(spi0, cmd, 1);
    gpio_put(BADGE_SCREEN_DC, 1);
    if(len > 1) {
        spi_write_blocking(spi0, (cmd+1), len-1);
    }
}

/* The send() macro enables writing send(CMD, ARG1, ARG2) but we can't have it in the header nor include it from here,
 * so you have to copy-paste it to use it in tests... */
#define UINT8_LIT(...) (uint8_t[]){__VA_ARGS__}
#define send(...) _send(UINT8_LIT(__VA_ARGS__), sizeof(UINT8_LIT(__VA_ARGS__))/sizeof(uint8_t))


void screen_init(void) {
    // Declare our GPIO usages
    bi_decl_if_func_used(bi_4pins_with_func(BADGE_SPI0_TX_MOSI_SCREEN, BADGE_SPI0_RX_MISO, BADGE_SPI0_SCK_SCREEN, BADGE_SPI0_CSn, GPIO_FUNC_SPI));
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_SCREEN_DC, "e-Paper D/C"));
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_SCREEN_BUSY, "e-Paper BUSY"));
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_SCREEN_RST, "e-Paper RST"));

    // Init SPI
    spi_init(spi0, 20*1000*1000);  /* Should go up to 20 MHz in write, but 2.5 in read */
    gpio_set_function(BADGE_SPI0_TX_MOSI_SCREEN, GPIO_FUNC_SPI);
    gpio_set_function(BADGE_SPI0_RX_MISO, GPIO_FUNC_SPI);
    gpio_set_function(BADGE_SPI0_SCK_SCREEN, GPIO_FUNC_SPI);
    gpio_set_function(BADGE_SPI0_CSn, GPIO_FUNC_SPI);

    // Init other pins
    gpio_init(BADGE_SCREEN_BUSY);
    gpio_init(BADGE_SCREEN_DC);
    gpio_put(BADGE_SCREEN_DC, 1);
    gpio_set_dir(BADGE_SCREEN_DC, GPIO_OUT);
    gpio_init(BADGE_SCREEN_RST);
    gpio_put(BADGE_SCREEN_RST, 1);  /* High = running */
    gpio_set_dir(BADGE_SCREEN_RST, GPIO_OUT);

    state = STATE_SLEEP;
    state_ts = get_absolute_time();
    log_info("screen init ok");

    /* Explicitly set the SPI chip select to output Y0 */
    /* TODO: move this to somewhere else because it will necessary create surprises here */
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_SPI0_CS_A0, "SPI0: Chip Select A0"));
    gpio_init(BADGE_SPI0_CS_A0);
    gpio_put(BADGE_SPI0_CS_A0, 0);
    gpio_set_dir(BADGE_SPI0_CS_A0, true);
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_SPI0_CS_A1, "SPI0: Chip Select A1"));
    gpio_init(BADGE_SPI0_CS_A1);
    gpio_put(BADGE_SPI0_CS_A1, 0);
    gpio_set_dir(BADGE_SPI0_CS_A1, true);
}


/* Sets up some common parameters on the screen, must be STATE_READY and not busy */
STATIC void setup(void) {
    /* Driver output control */
    send(SSD1681_DRIVER_CTRL, 0xC7, 0x00, 0x00);  /* 199+1 lines, no gate interlacing */

    /* Image orientation control: our up direction is toward the flexible connector,
     * which is the lowest Y coordinate, so we have to configure the RAM reading with decreasing X and Y */
    send(SSD1681_DATA_ENTRY, 0);/* x and y auto decrement; NOTE POR is 0x01, not 0x03!!! */

    /* The Power On Reset window to the ram is weird: 176*296, we have a 200x200 screen */
    send(SSD1681_RAM_XRANGE, 0x18, 0x00);  /* Set RAM-X start/end (*8) -> 0x18=24, (24+1)*8 = 200 */
    send(SSD1681_RAM_YRANGE, 0xC7, 0x00, 0x00, 0x00);  /* Set RAM-Y start/end -> 0xC7=199, 199+1 = 200 */
    /* Set RAM counters to be to the top line and column */
    send(SSD1681_RAM_XSTART, 0x18);
    send(SSD1681_RAM_YSTART, 0xC7, 0x00);

    /* Border WaveForm */
    send(SSD1681_BORDER_CTRL, 0x07);  /* bit 2 = follow LUT, bit 1-0 = LUTx */

    /* Use internal temp sensor instead of external */
    send(SSD1681_TEMP_CTRL, 0x80);

    /* Load internal Waveform Settings for display mode 1 using temp */
    send(SSD1681_DISPLAY_CTRL2, 0xB1);
    send(SSD1681_ACTIVATE);

    /* Can we display something without customized LUT ? Yes. */

    /* TODO: if debug, measure how long this step was */
}


bool screen_boot(void) {
    if (state == STATE_UNINIT)
        return false;
    if (state >= STATE_READY)
        return true;

    absolute_time_t now = get_absolute_time();

    switch(state) {
    case STATE_SLEEP: /* or boot */
        /* Boot procedure has various lengths, but after VCI, we should leave 10ms */
        if (absolute_time_diff_us(state_ts, now) >= 10000) {
            /* Starts HW RESET by pulling its pin down */
            gpio_put(BADGE_SCREEN_RST, 0);
            log_info("boot: SLEEP lasted %" PRIu64 "µs", absolute_time_diff_us(state_ts, now));
            state = STATE_HWRESET;
            state_ts = get_absolute_time();
        }
        break;
    case STATE_HWRESET:
        /* It is not specify how long we should pull RST down, but the Arduino project does it for 5ms */
        if (absolute_time_diff_us(state_ts, now) >= 5000) {
            gpio_put(BADGE_SCREEN_RST, 1);
            /* Also wait for BUSY, but it is not specified whether the busy pin is high in this time.
             * Tests showed that BUSY is high for 1.2ms on cold boot */
            if (! gpio_get(BADGE_SCREEN_BUSY)) {
                /* Now send a command to the screen and wait for busy to be low */
                send(SSD1681_SWRESET);
                log_info("boot: HWRESET lasted %" PRIu64 "µs", absolute_time_diff_us(state_ts, now));
                state = STATE_SWRESET;
                state_ts = get_absolute_time();
            }
        }
        break;
    case STATE_SWRESET:
        /* SWRESET is sent, now wait for busy to be low */
        if (gpio_get(BADGE_SCREEN_BUSY) == 0) {
            log_info("boot: SWRESET lasted %" PRIu64 "µs", absolute_time_diff_us(state_ts, now));
            state = STATE_SETUP;
            state_ts = get_absolute_time();
            setup();
        }
        break;
    case STATE_SETUP:
        /* Wait for setup: load LUT with temperature reading */
        if (gpio_get(BADGE_SCREEN_BUSY) == 0) {
            log_info("boot: SETUP lasted %" PRIu64 "µs, now ready", absolute_time_diff_us(state_ts, now));
            state = STATE_READY;
            state_ts = get_absolute_time();
        }
        break;
    default:
        /* We are lost. TODO Should we panic? */
        break;
    }

    return state >= STATE_READY;
}


bool screen_busy(void) {
    return (state < STATE_READY) || gpio_get(BADGE_SCREEN_BUSY);
}


void screen_border(uint8_t color) {
    if (screen_busy()) {
        log_warning("screen_border() called but screen is busy");
        return;
    }

    /* Put the command in a 2 bytes int
     * bit 2 = follow LUT, bit 1-0 = LUTx */
    send(SSD1681_BORDER_CTRL, 4 | (color & 3));
}


void screen_clear(bool bit) {
    if (screen_busy()) {
        log_warning("screen_clear() called but screen is busy");
        return;
    }

    /* RAM bypass configuration. 4 bits per RAM bank. LSB for black and white, MSB for red bank.
     * RRRR WWWW
     *  ^    ^ bypass RAM when 1 (read 0)
     * ^    ^  inverse value when 1 (if bypassed, read 1) */
    // 00 to 33 -> normal
    // 44 -> black
    // 55 -> white
    // 66 to BB -> inverse
    // CC,EE -> black
    // DD,FF -> white
    send(SSD1681_DISPLAY_CTRL1, 0x44 | (bit ? 0x11 : 0x00));

    /* Do start drawing */
    screen_show_rams();
}


void screen_deep_sleep(void) {
    if (screen_busy()) {
        log_warning("screen_deep_sleep() called but screen is busy");
        return;
    }

    /* After that, the screen keeps the BADGE_SCREEN_BUSY pin high until hard reset */
    send(SSD1681_DEEP_SLEEP, 0x01);  /* 0x01 or 0x03... */
    state = STATE_SLEEP;
    state_ts = get_absolute_time();
    log_info("screen put asleep");
}


size_t screen_set_image_position(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    if (screen_busy()) {
        log_warning("screen_set_image_position() called but screen is busy");
        return -1;
    }

    /* Swap min/max if needed */
    uint8_t o;
    if(x0 > x1) {
        o = x1; x1 = x0; x0 = o;
    }
    if(y0 > y1) {
        o = y1; y1 = y0; y0 = o;
    }

    /* Bind values to [0..200] */
    /* x1,y1 includethe last line/column, but the screen commands excludes them */
    x0 = x0 >= 200 ? 25 : x0/8;
    x1 = x1 >= 200 ? 24 : (x1%8 == 0 ? x1/8-1 : x1/8-1); /* FIXME: this is the same value in both cases */
    y0 = y0 >= 200 ? 200 : y0;
    y1 = y1 >= 200 ? 199 : y1-1;

    send(SSD1681_RAM_XRANGE, x1, x0);  /* Set RAM-X start/end (*8) -> 0x18=24, (24+1)*8 = 200 */
    send(SSD1681_RAM_YRANGE, y1, 0, y0, 0);  /* Set RAM-Y start/end -> 0xC7=199, 199+1 = 200 */
    /* Because of our data entry used to display the image upside (see setup()), set RAM counters to be to the top line and column */
    send(SSD1681_RAM_XSTART, x1);
    send(SSD1681_RAM_YSTART, y1, 0);

    return (y1-y0+1)*(x1-x0+1);
}


void screen_show_image_bw(const uint8_t *img) {
    if (screen_busy()) {
        log_warning("screen_show_image_bw() called but screen is busy");
        return;
    }

    /* Automatic function that does the manual commands */
    screen_clear_image_position();
    screen_push_ws(screen_ws_1681_bw);
    screen_push_rams(img, NULL, (SCREEN_WIDTH*SCREEN_HEIGHT)/8);
    screen_show_rams();
}


void screen_show_image_4g(const uint8_t *lsb, const uint8_t *msb) {
    if (screen_busy()) {
        log_warning("screen_show_image_4g() called but screen is busy");
        return;
    }

    /* Automatic function that does the manual commands */
    screen_clear_image_position();
    screen_push_ws(screen_ws_1681_4grays);
    screen_push_rams(lsb, msb, (SCREEN_WIDTH*SCREEN_HEIGHT)/8);
    screen_show_rams();
}


void screen_push_rams(const uint8_t *lsb, const uint8_t *msb, size_t len) {
    if (screen_busy()) {
        log_warning("screen_push_rams() called but screen is busy");
        return;
    }

    /* FIXME: this should be in another function */
    /* Configure RAM bypass to use only the pushed planes */
    //send(SSD1681_DISPLAY_CTRL1, (lsb ? 0x00:0x05) /* Bypass B/W bank */ | (msb ? 0x00:0x50) /* Bypass RED bank */);

    /* Push the image */
    if(lsb) {
        gpio_put(BADGE_SCREEN_DC, 0);  /* Low for commands, high for data */
        spi_write_blocking(spi0, "\x24", 1);  /* B/W RAM */
        gpio_put(BADGE_SCREEN_DC, 1);
        spi_write_blocking(spi0, lsb, len);
    }

    if(msb) {
        gpio_put(BADGE_SCREEN_DC, 0);
        spi_write_blocking(spi0, "\x26", 1);  /* RED RAM */
        gpio_put(BADGE_SCREEN_DC, 1);
        spi_write_blocking(spi0, msb, len);
    }
}


void screen_show_rams(void) {
    if (screen_busy()) {
        log_warning("screen_show_rams() called but screen is busy");
        return;
    }

    /* Protects against a known bug where being in multiframe mode and calling end_multiframe
     * after having already disabled the analog+clocks will freeze the SSD1681 in busy mode. */
    if (state == STATE_MULTIFRAME) {
        log_warning("screen_show_rams() called but screen was in multiframe mode; now in normal mode (next screen_end_multiframe will fail)");
        state = STATE_READY;
    }

    /* Configure then Activate */
    /* 0xC7 seems the normal mode for our target */
    /* 0xF7 (load temperature) on the b version (Red) */
    /* 0xCF for the partial image (display mode 2) */
    send(SSD1681_DISPLAY_CTRL2, 0xC7);
    send(SSD1681_ACTIVATE);
}


void screen_push_ws(const uint8_t *luts) {
    if (screen_busy()) {
        log_warning("screen_push_ws() called but screen is busy");
        return;
    }

    /* First 153 are the LUT + similar parameters */
    gpio_put(BADGE_SCREEN_DC, 0);  /* Low for commands, high for data */
    spi_write_blocking(spi0, "\x32", 1);
    gpio_put(BADGE_SCREEN_DC, 1);
    spi_write_blocking(spi0, luts, 153);

    /* Then EOPT, VGH, VSH1, VSH2, VSL, VCOM */
    /* Put the command in a 4 bytes int, as the longest command has 3 params */
    send(SSD1681_EOPT_CTRL, luts[153]);
    send(SSD1681_GATE_CTRL, luts[154]);
    send(SSD1681_SOURCE_CTRL, luts[155], luts[156], luts[157]);
    send(SSD1681_VCOM_CTRL, luts[158]);

    /* Then configure soft booster start !! TODO */
}


const uint8_t screen_ws_1681_bw[159] = \
    "\x80\x48\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* r=0, bw=0 */ \
    "\x40\x48\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* r=0, bw=1 */ \
    "\x80\x48\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* r=1, bw=0 */ \
    "\x40\x48\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* r=1, bw=1 */ \
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
    "\x0A\x00\x00\x00\x00\x00\x00" \
    "\x08\x01\x00\x08\x01\x00\x02" \
    "\x0A\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x01\x00\x00\x00\x00\x00\x00"  /* 11A=1, stabilizes to VSS before power off */ \
    "\x22\x22\x22\x22\x22\x28" "\x00\x00\x00" \
    "\x07"  /* EOPT, 0x22 = normal -> 0x07 helps if there is another image, as 0x22 flashes white on enable analog... */ \
    "\x17"  /*  VGH, 0x17 == 0x00 == 20V */   \
    "\x41"  /* VSH1, 0x41 == 15V */           \
    /* This LUT never uses VSH2 */            \
    "\x00"  /* VSH2, 0x00 == ???, POR is 5V */\
    "\x32"  /*  VSL, 0x32 == -15V */          \
    "\x20"; /* VCOM, 0x20 == -0.8V */

const uint8_t screen_ws_1681_4grays[159] = \
    "\xA8\x84\x54\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* black */ \
    "\x40\x84\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* darker */ \
    "\x50\x84\xA0\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* lighter */ \
    "\x54\x48\xA8\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* white */ \
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* VCOM = DVCOM */ \
    "\x08\x04\x00\x24\x00\x00\x00" /* TP[0A], TP[0B], SR[0AB], TP[0C], TP[0D], SR[0CD], RP[0] -> DC compensation */ \
    "\x08\x01\x00\x08\x01\x00\x01" /* erase group: white then black because grays are sharper from black than from white */ \
    "\x08\x04\x00\x24\x01\x00\x00" /* image group + 1 tick VSS to stabilize the image */\
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x82\x80\x00\x00\x00\x00" "\x00\x00\x00" \
    "\x07"  /* EOPT, 0x07 = keep before power off */ \
    "\x17"  /*  VGH, 0x17 == 0x00 == 20V */   \
    "\x41"  /* VSH1, 0x41 == 15V */           \
    /* This LUT never uses VSH2 */            \
    "\xA8"  /* VSH2, 0x00 == ???, POR is 5V */\
    "\x32"  /*  VSL, 0x32 == -15V */          \
    "\x20"; /* VCOM, 0x20 == -0.8V */


void screen_start_multiframe(void) {
    if (screen_busy()) {
        log_warning("screen_start_multiframe() called but screen is busy");
        return;
    }

    /* Not busy implies STATE_READY or above */
    if (state != STATE_READY) {
        log_warning("screen_start_multiframe() called but screen is not in the right state," \
                    " for example already started a multiframe (now is %d instead of %d)", state, STATE_READY);
    }

    send(SSD1681_DISPLAY_CTRL2, 0xC0);
    send(SSD1681_ACTIVATE);
    state = STATE_MULTIFRAME;
}

void screen_draw_multiframe(void) {
    if (screen_busy()) {
        log_warning("screen_draw_multiframe() called but screen is busy");
        return;
    }

    if (state != STATE_MULTIFRAME) {
        log_warning("screen_draw_multiframe() called but screen is not currently in multiframe mode");
    }

    send(SSD1681_DISPLAY_CTRL2, 0x04);
    send(SSD1681_ACTIVATE);
}

void screen_end_multiframe(void) {
    if (screen_busy()) {
        log_warning("screen_end_multiframe() called but screen is busy");
        return;
    }

    if (state != STATE_MULTIFRAME) {
        log_warning("screen_end_multiframe() called but screen is not currently in multiframe mode");
    }

    send(SSD1681_DISPLAY_CTRL2, 0x03);
    send(SSD1681_ACTIVATE);
    state = STATE_READY;
}


/* 30 fps means a budget of 33.3ms - 6.1ms to send an image, leaving 27.2ms hence 2 ticks @200Hz + 3@175Hz (27.1ms)
 * -> white is dark gray on the first images, it's too short */
const uint8_t screen_ws_30fps[159] = \
    "\x80\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
    "\x80\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
    "\x40\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
    "\x40\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
    "\x01\x00\x00\x00\x00\x00\x00" /* group 00 */ \
    "\x03\x00\x00\x00\x00\x00\x00" /* group 01 */ \
    "\x00\x00\x00\x00\x01\x00\x00" /* group 02 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 03 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 04 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 05 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 06 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 07 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 08 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 09 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 10 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 11 */ \
    "\x87\x80\x00\x00\x00\x00" "\x00\x00\x00" \
    "\x07" \
    "\x17" \
    "\x41" \
    "\xA8" \
    "\x32" \
    "\x20";

/* 20 fps means a budget of 50.0ms - 6.1ms to send an image, leaving 43.9ms hence 4 ticks @200Hz + 3@175Hz + 1@150Hz (43.8ms)
 * -> white is light gray on the first images, it's too short */
const uint8_t screen_ws_20fps[159] = \
    "\x40\x40\x90\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
    "\x80\x80\xA0\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
    "\x40\x40\x50\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
    "\x80\x80\x60\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
    "\x01\x00\x00\x00\x00\x00\x00" /* group 00 */ \
    "\x03\x00\x00\x00\x00\x00\x00" /* group 01 */ \
    "\x01\x02\x00\x00\x01\x00\x00" /* group 02 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 03 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 04 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 05 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 06 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 07 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 08 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 09 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 10 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 11 */ \
    "\x67\x80\x00\x00\x00\x00" "\x00\x00\x00" \
    "\x07" \
    "\x17" \
    "\x41" \
    "\xA8" \
    "\x32" \
    "\x20";

/* 10 fps means a budget of 100.0ms - 6.1ms to send an image, leaving 33.9ms hence 13 ticks @200Hz + 5@175Hz (93.5ms)
 * -> white is somewhat white and black is somewhat black since the first image */
const uint8_t screen_ws_10fps[159] = \
    "\x24\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
    "\x28\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
    "\x14\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
    "\x18\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x01\x00\x0B\x00\x00\x00" /* group 00 */ \
    "\x05\x00\x00\x00\x00\x00\x00" /* group 01 */ \
    "\x00\x00\x00\x00\x01\x00\x00" /* group 02 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 03 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 04 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 05 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 06 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 07 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 08 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 09 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 10 */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* group 11 */ \
    "\x87\x80\x00\x00\x00\x00" "\x00\x00\x00" \
    "\x07" \
    "\x17" \
    "\x41" \
    "\xA8" \
    "\x32" \
    "\x20";
