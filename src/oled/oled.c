/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <string.h>

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/binary_info.h"

#include "log.h"
#include "oled.h"
#include "pinouts.h"


#define OLED_I2C_INST I2C_INSTANCE(BADGE_OLED_I2C)
#define OLED_BAUD 400000
#define TIMEOUT_US 5000

static uint8_t address = 0;
static uint8_t frame[OLED_FB_SIZE];  /* Last requested frame */
static int next_page = -1;  /* Page to send, -1 when idle */


static bool command(const uint8_t *cmds, size_t n) {
    uint8_t buf[32];
    buf[0] = 0x00;  /* Control byte: commands */
    memcpy(buf + 1, cmds, n);
    return i2c_write_timeout_us(OLED_I2C_INST, address, buf, n + 1, false, TIMEOUT_US) == (int)(n + 1);
}


bool oled_init(void) {
    bi_decl_if_func_used(bi_2pins_with_func(BADGE_OLED_SDA, BADGE_OLED_SCL, GPIO_FUNC_I2C));
    i2c_init(OLED_I2C_INST, OLED_BAUD);
    gpio_set_function(BADGE_OLED_SDA, GPIO_FUNC_I2C);
    gpio_set_function(BADGE_OLED_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(BADGE_OLED_SDA);  /* The modules usually have their pull-ups, these weak ones only help */
    gpio_pull_up(BADGE_OLED_SCL);

    address = 0;
    const uint8_t addresses[2] = {0x3C, 0x3D};
    for (int i = 0; i < 2 && ! address; ++i) {
        uint8_t dummy;
        if (i2c_read_timeout_us(OLED_I2C_INST, addresses[i], &dummy, 1, false, TIMEOUT_US) >= 0)
            address = addresses[i];
    }
    if (! address) {
        log_info("oled: no screen on I2C%d", BADGE_OLED_I2C);
        return false;
    }

    static const uint8_t init[] = {
        0xAE,        /* Display off */
        0xD5, 0x80,  /* Clock divider */
        0xA8, 0x3F,  /* Multiplex: 64 rows */
        0xD3, 0x00,  /* No display offset */
        0x40,        /* Start line 0 */
        0x8D, 0x14,  /* Charge pump on (internal VCC) */
        0x20, 0x00,  /* Horizontal addressing */
        0xA1,        /* Segment remap: column 127 is SEG0 */
        0xC8,        /* COM scan direction: remapped (these 2 give the usual orientation of the modules) */
        0xDA, 0x12,  /* COM pins: alternative, for 128x64 */
        0x81, 0xCF,  /* Contrast */
        0xD9, 0xF1,  /* Pre-charge */
        0xDB, 0x40,  /* VCOMH deselect level */
        0xA4,        /* Display from RAM */
        0xA6,        /* Normal (not inverted) */
        0xAF,        /* Display on */
    };
    if (! command(init, sizeof(init))) {
        address = 0;
        log_warning("oled: screen found but initialization failed");
        return false;
    }
    log_info("oled: SSD1306 at 0x%02x", address);
    memset(frame, 0, sizeof(frame));
    next_page = 0;
    return true;
}


bool oled_present(void) {
    return address != 0;
}


void oled_show(const uint8_t *fb) {
    memcpy(frame, fb, OLED_FB_SIZE);
    next_page = 0;  /* (Re)start from the first page: a frame may mix two frames, acceptable for animations */
}


void oled_power(bool on) {
    if (! address)
        return;
    uint8_t cmd = on ? 0xAF : 0xAE;
    command(&cmd, 1);
}


bool oled_task(void) {
    if (! address || next_page < 0)
        return false;

    /* Window: all the columns, this page */
    uint8_t window[6] = {0x21, 0, OLED_WIDTH - 1, 0x22, next_page, next_page};
    if (! command(window, sizeof(window))) {
        next_page = -1;
        return false;
    }

    /* Convert the page: from row major (gfx format) to columns of 8 vertical pixels */
    uint8_t data[1 + OLED_WIDTH];
    data[0] = 0x40;  /* Control byte: data */
    for (int x = 0; x < OLED_WIDTH; ++x) {
        uint8_t col = 0;
        for (int b = 0; b < 8; ++b) {
            int y = next_page * 8 + b;
            if (frame[y * (OLED_WIDTH/8) + x/8] & (0x80 >> (x % 8)))
                col |= 1 << b;
        }
        data[1 + x] = col;
    }
    i2c_write_timeout_us(OLED_I2C_INST, address, data, sizeof(data), false, TIMEOUT_US * 4);

    if (++next_page >= OLED_HEIGHT / 8)
        next_page = -1;
    return next_page >= 0;
}
