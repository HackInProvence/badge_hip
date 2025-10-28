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
} state_t;
STATIC state_t state = STATE_UNINIT;
STATIC absolute_time_t state_ts = 0;  /* Last time the state changed */


/* Send data on the SPI but don't wait for BUSY to be LOW */
STATIC void send(const uint8_t *cmd, size_t len) {
    if(! cmd || len == 0)
        return;

    gpio_put(BADGE_SCREEN_DC, 0);  /* Low for commands, high for data */
    spi_write_blocking(spi0, cmd, 1);
    gpio_put(BADGE_SCREEN_DC, 1);
    if(len > 1) {
        spi_write_blocking(spi0, (cmd+1), len-1);
    }
}


void screen_init(void) {
    // Declare our GPIO usages
    bi_decl_if_func_used(bi_4pins_with_func(BADGE_SPI0_TX_MOSI_SCREEN, BADGE_SPI0_RX_MISO, BADGE_SPI0_SCK_SCREEN, BADGE_SPI0_CSn, GPIO_FUNC_SPI));
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_SCREEN_DC, "e-Paper D/C"));
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_SCREEN_BUSY, "e-Paper BUSY"));
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_SCREEN_RST, "e-Paper RST"));

    // Init SPI
    spi_init(spi0, 2000*1000);  /* Should go up to 20 MHz in write, but 2.5 in read */
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
}


/* Sets up some common parameters on the screen, must be STATE_READY and not busy */
STATIC void setup(void) {
    /* Driver output control */
    send("\x01\xC7\x00\x00", 4);  /* Driver output control: 199+1 lines, no gate interlacing */

    /* Image orientation control: our up direction is toward the flexible connector,
     * which is the lowest Y coordinate, so we have to configure the RAM reading with decreasing X and Y */
    /* Data entry mode, x and y auto decrement; NOTE POR is 0x01, not 0x03!!! */
    send("\x11\x00", 2);
    /* The Power On Reset window to the ram is weird: 176*296, we have a 200x200 screen */
    send("\x44\x18\x00", 3);  /* Set RAM-X start/end (x8 -> (0x18=24, (24+1)*8 = 200) */
    send("\x45\xC7\x00\x00\x00", 5);  /* Set RAM-Y start/end (x8 -> (0xC7=199, 199+1 = 200) */
    /* Set RAM counters to be to the top line and column */
    send("\x4E\x18", 2);
    send("\x4F\xC7\x00", 2);

    /* Border WaveForm */
    send("\x3C\x07", 2);  /* bit 2 = follow LUT, bit 1-0 = LUTx */

    /* Use internal temp sensor instead of external */
    send("\x18\x80", 2);

    /* Load internal Waveform Settings for display mode 1 using temp */
    send("\x22\xB1", 2);
    send("\x20", 1);

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
                send("\x12", 1);
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
    uint16_t cmd = 0x3C | ((4 | (color & 3)) << 8);
    send((uint8_t *)&cmd, 2);  /* Don't pass pointers to local variables when the callee may borrow them... */
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
    uint16_t cmd = 0x21 | ((0x44 | (bit ? 0x11 : 0x00)) << 8);
    send((const uint8_t *)&cmd, 2);

    send("\x22\xC7", 2);
    send("\x20", 1);

}


void screen_deep_sleep(void) {
    if (screen_busy()) {
        log_warning("screen_deep_sleep() called but screen is busy");
        return;
    }

    /* After that, the screen keeps the BADGE_SCREEN_BUSY pin high until hard reset */
    send("\x10\x01", 2);  /* 0x01 or 0x03... */
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
    /* x1,y1 include the last line/column, but the screen commands excludes them */
    x0 = x0 >= 200 ? 25 : x0/8;
    x1 = x1 >= 200 ? 24 : (x1%8 == 0 ? x1/8-1 : x1/8-1);
    y0 = y0 >= 200 ? 200 : y0;
    y1 = y1 >= 200 ? 199 : y1-1;

    uint64_t cmd;
    /* Set RAM-X start/end (give end first because we give data in reverse order, see setup) */
    cmd = 0x44 | (x1 << 8) | (x0 << 16);
    send((uint8_t *)&cmd, 3);  /* Don't pass pointers to local variables when the callee may borrow them... */

    /* Set RAM-Y start/end, this time on 9 bits (we only use 8 of them) */
    cmd = 0x45 | (y1 << 8) | (y0 << 24);
    send((uint8_t *)&cmd, 5);

    /* Set RAM counters to be to the top line and column */
    cmd = 0x4E | (x1 << 8);
    send((uint8_t *)&cmd, 2);
    cmd = 0x4F | (y1 << 8);  /* Again y1 is on 2 bytes but we use only the first */
    send((uint8_t *)&cmd, 3);

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

    /* Configure RAM bypass to use only the pushed planes */
    uint16_t cmd = 0x21;
    if(! lsb)
        cmd |= (0x05 << 8);  /* Bypass B/W bank */
    if(! msb)
        cmd |= (0x50 << 8);  /* Bypass RED bank */
    send((uint8_t *)&cmd, 2);  /* Don't pass pointers to local variables when the callee may borrow them... */

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

    /* Configure then Activate */
    /* 0xC7 seems the normal mode for our target */
    /* 0xF7 (load temperature) on the b version (Red) */
    /* 0xCF for the partial image (display mode 2) */
    send("\x22\xC7", 2);
    send("\x20", 1);
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
    uint32_t buf;
    buf = 0x3F | (luts[153]<<8);  /* EOPT */
    send((const uint8_t *)&buf, 2);  /* Don't pass pointers to local variables when the callee may borrow them... */
    buf = 0x03 | (luts[154]<<8);  /* VGH */
    send((const uint8_t *)&buf, 2);
    buf = 0x04 | (luts[155]<<8) | (luts[156]<<16) | (luts[157]<<24);  /* VSH1, VSH2, VSL */
    send((const uint8_t *)&buf, 4);
    buf = 0x2C | (luts[158]<<8);  /* VCOM */
    send((const uint8_t *)&buf, 2);

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
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x22\x22\x22\x22\x22\x22" "\x00\x00\x00" \
    "\x22"  /* EOPT, 0x22 = normal */         \
    "\x17"  /*  VGH, 0x17 == 0x00 == 20V */   \
    "\x41"  /* VSH1, 0x41 == 15V */           \
    /* This LUT never uses VSH2 */            \
    "\x00"  /* VSH2, 0x00 == ???, POR is 5V */\
    "\x32"  /*  VSL, 0x32 == -15V */          \
    "\x20"; /* VCOM, 0x20 == -0.8V */

const uint8_t screen_ws_1681_4grays[159] = \
    "\xA8\x48\x54\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* black */ \
    "\xA0\x48\x50\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* darker */ \
    "\x80\x48\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* lighter */ \
    "\x54\x48\xA8\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* white */ \
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
    "\x01\x02\x00\x09\x00\x00\x00" \
    "\x08\x01\x00\x08\x01\x00\x02" \
    "\x01\x02\x00\x09\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x22\x22\x22\x22\x22\x22" "\x00\x00\x00" \
    "\x22"  /* EOPT, 0x22 = normal */         \
    "\x17"  /*  VGH, 0x17 == 0x00 == 20V */   \
    "\x41"  /* VSH1, 0x41 == 15V */           \
    /* This LUT never uses VSH2 */            \
    "\x00"  /* VSH2, 0x00 == ???, POR is 5V */\
    "\x32"  /*  VSL, 0x32 == -15V */          \
    "\x20"; /* VCOM, 0x20 == -0.8V */


