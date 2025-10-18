/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

// Include sys/types.h before inttypes.h to work around issue with
// certain versions of GCC and newlib which causes omission of PRIu64
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "badge_pinout.h"
#include "screen.h"


/* Development functions to test the SPI library */
void init(void) {
    // Declare our GPIO usages
    bi_decl_if_func_used(bi_3pins_with_func(BADGE_SPI0_TX_MOSI_SCREEN, BADGE_SPI0_RX_MISO, BADGE_SPI0_SCK_SCREEN, GPIO_FUNC_SPI));
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_SPI0_CSn, "e-Paper"));
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_SCREEN_DC, "e-Paper"));
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_SCREEN_BUSY, "e-Paper"));
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_SCREEN_RST, "e-Paper"));

    // Init SPI
    spi_init(spi0, 2000*1000);  /* Should go up to 20 MHz in write, but 2.5 in read */
    gpio_set_function(BADGE_SPI0_TX_MOSI_SCREEN, GPIO_FUNC_SPI);
    gpio_set_function(BADGE_SPI0_RX_MISO, GPIO_FUNC_SPI);
    gpio_set_function(BADGE_SPI0_SCK_SCREEN, GPIO_FUNC_SPI);
    gpio_init(BADGE_SPI0_CSn);  /* Set to GPIO_FUNC_SIO and input */ /* FIXME: maybe it could be controlled by SPI0... ? the problem is for the reads */
    gpio_put(BADGE_SPI0_CSn, 1);
    gpio_set_dir(BADGE_SPI0_CSn, GPIO_OUT);

    // Init other pins
    gpio_init(BADGE_SCREEN_BUSY);
    gpio_init(BADGE_SCREEN_DC);
    gpio_put(BADGE_SCREEN_DC, 1);
    gpio_set_dir(BADGE_SCREEN_DC, GPIO_OUT);
    gpio_init(BADGE_SCREEN_RST);
    gpio_put(BADGE_SCREEN_RST, 1);  /* High = running */
    gpio_set_dir(BADGE_SCREEN_RST, GPIO_OUT);
}


void send_no_wait(const char *cmd, size_t len) {
    gpio_put(BADGE_SPI0_CSn, 0);
    gpio_put(BADGE_SCREEN_DC, 0);  /* Low for commands, high for data */
    spi_write_blocking(spi0, (const uint8_t *)cmd, 1);
    if(len > 1) {
        gpio_put(BADGE_SCREEN_DC, 1);
        spi_write_blocking(spi0, (const uint8_t *)(cmd+1), len-1);
    }
    gpio_put(BADGE_SPI0_CSn, 1);
}

void send(const char *cmd, size_t len) {
    send_no_wait(cmd, len);
    while (gpio_get(BADGE_SCREEN_BUSY)) ;
}


void reset(void) {
    absolute_time_t t0, t1;
    /* Boot procedure has various lengths, but after VCI, we should leave 10ms */
    /* It does not specify how long we should pull RST down, nor whether the busy pin is high in this time */
    /* HWRESET */
    gpio_put(BADGE_SCREEN_RST, 0);
    sleep_ms(5);  // Source: Arduino
    gpio_put(BADGE_SCREEN_RST, 1);
    t0 = get_absolute_time();

    // Is Busy high when booting? -> it is !
    //absolute_time_t target = make_timeout_time_ms(20);
    //uint64_t delay = 50;
    //uint64_t vals[16];
    //size_t i;
    //for(i=0; i<16 && get_absolute_time() < target; ++i) {
    //    sleep_us(delay);
    //    delay *= 2;
    //    vals[i] = gpio_get(BADGE_SCREEN_BUSY);
    //}
    //for(size_t j=0; j<i; ++j)
    //    printf("BUSY[%d] = %d\n", j, vals[j]); // This will kill the delay!
    while (gpio_get(BADGE_SCREEN_BUSY)) ;
    t1 = get_absolute_time();
    printf("HWRESET lasted %" PRIu64 "µs\n", absolute_time_diff_us(t0, t1));  /* around 1.2ms */

    /* SWRESET */
    gpio_put(BADGE_SPI0_CSn, 0);
    gpio_put(BADGE_SCREEN_DC, 0);  /* Low for commands, high for data */
    spi_write_blocking(spi0, (const uint8_t *)"\x12", 1);
    gpio_put(BADGE_SPI0_CSn, 1);
    t0 = get_absolute_time();
    while (gpio_get(BADGE_SCREEN_BUSY)) ;
    t1 = get_absolute_time();
    printf("SWRESET lasted %" PRIu64 "µs\n", absolute_time_diff_us(t0, t1));  /* around 1.7ms */
}


void setup(void) {
    /* Driver output control */
    send("\x01\xC7\x00\x00", 4);  /* Driver output control: 199+1 lines, no gate interlacing */
    //send("\x11\x03", 2);  /* Data entry mode: x,y auto increment -> TODO: POR is 0x01, not 0x03!!! */
    send("\x11\x00", 2);

    /* The Power On Reset view of the ram is weird: 176*296 */
    send("\x44\x18\x00", 3);  /* Set RAM-X start/end (x8 -> (0x18=24, (24+1)*8 = 200) */
    send("\x45\xC7\x00\x00\x00", 5);  /* Set RAM-X start/end (x8 -> (0xC7=199, 199+1 = 200) */
    /* Set RAM counters (POR 0, so useless?) */
    send("\x4E\x18", 2);
    send("\x4F\xC7\x00", 2);

    /* Border WaveForm */
    send("\x3C\x07", 2);  /* bit 2 = follow LUT, bit 1-0 = LUTx */

    /* Set internal temp sensor */
    send("\x18\x80", 2);

    /* Load LUT for display mode 1 using temp */
    send("\x22\xB1", 2);
    send("\x20", 1);

    /* Can we display something without LUT ? */
}


void deep_sleep(void) {
    send_no_wait("\x10\x01", 2);  /* 0x01 or 0x03...*/
    //gpio_put(BADGE_SCREEN_RST, 0);  /* Not clear whether this is useful or not */
}


// Waveform settings, taken from the Arduino "test suite"
const uint8_t ws_1681_bw_full[159] = \
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

// Waveform settings, showing grays
const uint8_t ws_1681_times[159] = \
    "\x80\x48\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* black */ \
    "\x40\x48\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* darker */ \
    "\x80\x48\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* lighter */ \
    "\x40\x48\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* white */ \
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
    "\x0A\x00\x00\x00\x00\x00\x00" \
    "\x08\x01\x00\x08\x01\x00\x02" \
    "\x01\x40\x00\x00\x00\x00\x0E" \
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

// Waveform settings, showing grays
const uint8_t ws_1681_4grays[159] = \
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

void set_ws(const uint8_t *ws) {
    /* First 153 are the LUT + similar parameters */
    gpio_put(BADGE_SPI0_CSn, 0);
    gpio_put(BADGE_SCREEN_DC, 0);  /* Low for commands, high for data */
    spi_write_blocking(spi0, "\x32", 1);
    gpio_put(BADGE_SCREEN_DC, 1);
    spi_write_blocking(spi0, ws, 153);
    gpio_put(BADGE_SPI0_CSn, 1);
    while (gpio_get(BADGE_SCREEN_BUSY)) ;

    //send("\x22\xC0", 2); /* Enable analog clock */
    //send("\x20", 1);

    /* Then EOPT, VGH, VSH1, VSH2, VSL, VCOM */
    uint32_t buf;
    buf = 0x3F | (ws[153]<<8);  /* EOPT */
    send((const uint8_t *)&buf, 2);
    buf = 0x03 | (ws[154]<<8);  /* VGH */
    send((const uint8_t *)&buf, 2);
    buf = 0x04 | (ws[155]<<8) | (ws[156]<<16) | (ws[157]<<24);  /* VSH1, VSH2, VSL */
    send((const uint8_t *)&buf, 4);
    //send_no_wait((const uint8_t *)&buf, 4);
    //send("\x04\x41\x00\x32", 4);
    buf = 0x2C | (ws[158]<<8);  /* VCOM */
    send((const uint8_t *)&buf, 2);

    /* Then soft start !! TODO */
}


void img_const(bool bit/*, uint8_t flag*/) {
    //static uint8_t buf[200];
    //for (size_t i=0; i<200; ++i)
    //    buf[i] = bit ? 0xf2 : 0x00;

    //char ch;
    //gpio_put(BADGE_SPI0_CSn, 0);
    //for(size_t bank=0; bank<2; ++bank) {
    //    ch = 0x24 + 2*bank;
    //    gpio_put(BADGE_SCREEN_DC, 0);  /* Low for commands, high for data */
    //    spi_write_blocking(spi0, &ch, 1);
    //    gpio_put(BADGE_SCREEN_DC, 1);
    //    for (size_t i=0; i<25; ++i)
    //        spi_write_blocking(spi0, buf, 200);
    //}
    //gpio_put(BADGE_SPI0_CSn, 1);
    //while (gpio_get(BADGE_SCREEN_BUSY)) ;

    /* Red then BW content bypass: 4 bits mask, 4 -> bypass to 0, 8 -> inverse content */
    // 00 to 33 -> normal
    // 44 -> black
    // 55 -> white
    // 66 to BB -> inverse
    // CC,EE -> black
    // DD,FF -> white
    //uint16_t cmd = 0x21 | (flag << 8);
    uint16_t cmd = 0x21 | ((0x44 | (bit ? 0x11 : 0x00)) << 8);
    send((const uint8_t *)&cmd, 2);

    send("\x22\xC7", 2);
    send("\x20", 1);

    /* Don't forget to reset to normal afterwards */
    send("\x21\x00", 2);
}


void push_image(const uint8_t *src) {
    char ch;
    gpio_put(BADGE_SPI0_CSn, 0);
    for(size_t bank=0; bank<2; ++bank) {
        ch = 0x24 + 2*bank;
        gpio_put(BADGE_SCREEN_DC, 0);  /* Low for commands, high for data */
        spi_write_blocking(spi0, &ch, 1);
        gpio_put(BADGE_SCREEN_DC, 1);
        spi_write_blocking(spi0, src, 200*25);
    }
    gpio_put(BADGE_SPI0_CSn, 1);
    while (gpio_get(BADGE_SCREEN_BUSY)) ;

    send("\x22\xC7", 2);  /* 0xF7 on the b version (Red), 0xCF ? for the partial image */
    send("\x20", 1);
}

#include "hip_bw.h"
#include "secsea_bw.h"


void push_images(const uint8_t *bw, const uint8_t *red) {
    char ch = 0x24; /* B/W == LSB */
    gpio_put(BADGE_SPI0_CSn, 0);
    gpio_put(BADGE_SCREEN_DC, 0);  /* Low for commands, high for data */
    spi_write_blocking(spi0, &ch, 1);
    gpio_put(BADGE_SCREEN_DC, 1);
    spi_write_blocking(spi0, bw, 200*25);
    ch = 0x26; /* RED == MSB */
    gpio_put(BADGE_SCREEN_DC, 0);  /* Low for commands, high for data */
    spi_write_blocking(spi0, &ch, 1);
    gpio_put(BADGE_SCREEN_DC, 1);
    spi_write_blocking(spi0, red, 200*25);
    gpio_put(BADGE_SPI0_CSn, 1);
    while (gpio_get(BADGE_SCREEN_BUSY)) ;

    /* 0xC7 seems the normal mode for our target */
    /* 0xF7 (load temperature) on the b version (Red) */
    /* 0xCF for the partial image (display mode 2) */
    send("\x22\xCF", 2);
    send("\x20", 1);
}

#include "squares_bw.h"
#include "text_bw.h"
#include "grad_4g.h"
#include "secsea_4g.h"


int main() {
    stdio_usb_init();

    init();

    printf("Start screen\n");

    reset();
    setup();

    //read_all();
    set_ws(ws_1681_bw_full);

    //printf("clear to black\n");
    //img_const(0);  // black
    //printf("done\n");
    /* Test all 0x21 (bypass ram) */
    //for(size_t i=0; i<16; ++i) {
    //    uint8_t flag = i | (i<<4);
    //    printf("0x%02X\n", flag);
    //    img_const(0, flag);
    //    sleep_ms(5000);
    //}

    ////sleep_ms(1000);
    //printf("push image\n");
    //push_image(secsea_bw);
    //printf("done\n");
    //sleep_ms(2000);
    //printf("push image\n");
    //push_image(hip_bw);
    //printf("done\n");

    //set_ws(ws_1681_times);
    //push_image(text_bw);
    //sleep_ms(5000);
    //printf("done\n");

    set_ws(ws_1681_4grays);
    //push_images(text_bw, squares_bw); /* LSB, MSB */
    //push_images(grad_4g_lsb, grad_4g_msb);
    push_images(secsea_4g_lsb, secsea_4g_msb);

    //sleep_ms(2000);
    //printf("clear to white\n");
    //img_const(1);  // Clear to white before sleep

    deep_sleep();
    sleep_ms(1000);
    printf("Screen sleeping, after 1s BUSY = %d\n\n", gpio_get(BADGE_SCREEN_BUSY));
}
