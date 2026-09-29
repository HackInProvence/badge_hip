/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* SD card in SPI mode, see the "Physical Layer Simplified Specification" of the SD Association
 * and http://elm-chan.org/docs/mmc/mmc_e.html which explains the SPI mode well. */

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/binary_info.h"
#include "pico/time.h"

#include "log.h"
#include "pinouts.h"
#include "sd.h"


#define SD_INIT_BAUD 400000  /* The card must be initialized with a clock < 400kHz */
#define SD_BAUD 12500000  /* Up to 25MHz in SPI mode, but the bus goes through the decoder and is shared with the screen */

#define CMD0 0  /* GO_IDLE_STATE */
#define CMD8 8  /* SEND_IF_COND */
#define CMD12 12  /* STOP_TRANSMISSION */
#define CMD16 16  /* SET_BLOCKLEN */
#define CMD17 17  /* READ_SINGLE_BLOCK */
#define CMD18 18  /* READ_MULTIPLE_BLOCK */
#define CMD55 55  /* APP_CMD */
#define CMD58 58  /* READ_OCR */
#define ACMD41 (0x80 | 41)  /* SD_SEND_OP_COND (application command, prefixed by CMD55) */

static bool ready = false;
static bool block_addressing = false;  /* SDHC/SDXC are addressed by sectors, SDSC by bytes */
static uint saved_baud = 0;


static uint8_t xchg(uint8_t b) {
    uint8_t r;
    spi_write_read_blocking(spi0, &b, &r, 1);
    return r;
}

/* Take SPI0: CSn as GPIO (decoder disabled), decoder address to the SD card, SD baud rate */
static void take_bus(void) {
    gpio_put(BADGE_SPI0_CSn, 1);
    gpio_set_dir(BADGE_SPI0_CSn, GPIO_OUT);
    gpio_set_function(BADGE_SPI0_CSn, GPIO_FUNC_SIO);
    gpio_put(BADGE_SPI0_CS_A0, BADGE_SPI0_ADDR_SD & 1);
    gpio_put(BADGE_SPI0_CS_A1, (BADGE_SPI0_ADDR_SD >> 1) & 1);
    saved_baud = spi_get_baudrate(spi0);
    spi_set_baudrate(spi0, ready ? SD_BAUD : SD_INIT_BAUD);
}

/* Give SPI0 back to the screen */
static void release_bus(void) {
    gpio_put(BADGE_SPI0_CSn, 1);
    xchg(0xFF);  /* The card releases MISO on the next clock after CS goes high */
    gpio_put(BADGE_SPI0_CS_A0, BADGE_SPI0_ADDR_SCREEN & 1);
    gpio_put(BADGE_SPI0_CS_A1, (BADGE_SPI0_ADDR_SCREEN >> 1) & 1);
    spi_set_baudrate(spi0, saved_baud);
    gpio_set_function(BADGE_SPI0_CSn, GPIO_FUNC_SPI);
}

static bool wait_ready(uint32_t timeout_ms) {
    absolute_time_t timeout = make_timeout_time_ms(timeout_ms);
    while (xchg(0xFF) != 0xFF) {
        if (time_reached(timeout))
            return false;
    }
    return true;
}

/* Sends a command, returns the R1 response (bit 7 set on error/timeout) */
static uint8_t send_cmd(uint8_t cmd, uint32_t arg) {
    if (cmd & 0x80) {
        cmd &= 0x7F;
        uint8_t r = send_cmd(CMD55, 0);
        if (r > 1)
            return r;
    }
    if (cmd != CMD0 && ! wait_ready(500))
        return 0xFF;

    /* CRC is only checked for CMD0 and CMD8 in SPI mode */
    uint8_t frame[6] = {0x40 | cmd, arg >> 24, arg >> 16, arg >> 8, arg, cmd == CMD0 ? 0x95 : (cmd == CMD8 ? 0x87 : 0x01)};
    spi_write_blocking(spi0, frame, sizeof(frame));
    if (cmd == CMD12)
        xchg(0xFF);  /* Skip the stuff byte */

    uint8_t r = 0xFF;
    for (int i = 0; i < 10 && (r & 0x80); ++i)
        r = xchg(0xFF);
    return r;
}

static bool receive_block(uint8_t *buf) {
    absolute_time_t timeout = make_timeout_time_ms(200);
    uint8_t token;
    while ((token = xchg(0xFF)) == 0xFF) {
        if (time_reached(timeout))
            return false;
    }
    if (token != 0xFE)
        return false;
    spi_read_blocking(spi0, 0xFF, buf, 512);
    xchg(0xFF);  /* CRC, ignored */
    xchg(0xFF);
    return true;
}


bool sd_detect(void) {
    return gpio_get(BADGE_SD_DETECT);
}


bool sd_is_ready(void) {
    return ready;
}


bool sd_init(void) {
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_SD_DETECT, "SD card detect"));
    gpio_init(BADGE_SD_DETECT);
    gpio_pull_up(BADGE_SD_DETECT);
    gpio_pull_up(BADGE_SPI0_RX_MISO);  /* The card needs a pull-up on its DO line */

    ready = false;
    take_bus();

    /* At least 74 clocks with CS high to enter the native mode */
    for (int i = 0; i < 10; ++i)
        xchg(0xFF);

    /* Go to SPI mode */
    gpio_put(BADGE_SPI0_CSn, 0);
    uint8_t r = 0xFF;
    for (int i = 0; i < 10 && r != 0x01; ++i)
        r = send_cmd(CMD0, 0);
    if (r != 0x01) {
        log_warning("sd: no card (CMD0 answered 0x%02x)", r);
        release_bus();
        return false;
    }

    bool ok = false;
    absolute_time_t timeout = make_timeout_time_ms(1000);
    if (send_cmd(CMD8, 0x1AA) == 0x01) {
        /* SD v2: check the voltage range echo, then initialize with HCS (high capacity supported) */
        uint8_t r7[4];
        spi_read_blocking(spi0, 0xFF, r7, 4);
        if (r7[2] == 0x01 && r7[3] == 0xAA) {
            while ((r = send_cmd(ACMD41, 1u << 30)) != 0 && ! time_reached(timeout))
                ;
            if (r == 0 && send_cmd(CMD58, 0) == 0) {
                uint8_t ocr[4];
                spi_read_blocking(spi0, 0xFF, ocr, 4);
                block_addressing = ocr[0] & 0x40;  /* CCS bit */
                ok = true;
            }
        }
    } else {
        /* SD v1 (old SDSC cards) */
        while ((r = send_cmd(ACMD41, 0)) != 0 && ! time_reached(timeout))
            ;
        block_addressing = false;
        ok = r == 0 && send_cmd(CMD16, 512) == 0;
    }

    release_bus();
    ready = ok;
    if (ok)
        log_info("sd: card ready (%s)", block_addressing ? "SDHC/SDXC" : "SDSC");
    else
        log_warning("sd: card initialization failed");
    return ok;
}


bool sd_read_blocks(uint32_t lba, uint8_t *buf, size_t count) {
    if (! ready || count == 0)
        return false;
    if (! block_addressing)
        lba *= 512;

    take_bus();
    gpio_put(BADGE_SPI0_CSn, 0);
    bool ok;
    if (count == 1) {
        ok = send_cmd(CMD17, lba) == 0 && receive_block(buf);
    } else {
        if (send_cmd(CMD18, lba) == 0) {
            do {
                if (! receive_block(buf))
                    break;
                buf += 512;
            } while (--count);
            send_cmd(CMD12, 0);
        }
        ok = count == 0;
    }
    release_bus();
    if (! ok)
        log_warning("sd: read error at sector %lu", (unsigned long)lba);
    return ok;
}
