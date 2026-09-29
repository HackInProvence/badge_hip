/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Host tests of the infrared module: NEC decoding */

#include "pico_host.h"
#include "../../ir/ir.c"
#include "test.h"

uint64_t host_time_us = 0;
bool host_gpio[32];
uint8_t host_spi_log[65536];
size_t host_spi_len = 0;

/* A NEC frame, with a tolerance factor on the durations (100 = exact) */
static void nec(ir_signal_t *s, uint32_t bits, int percent) {
    s->n = 0;
    s->us[s->n++] = 9000 * percent / 100;
    s->us[s->n++] = 4500 * percent / 100;
    for (int i = 0; i < 32; ++i) {
        s->us[s->n++] = 562 * percent / 100;
        s->us[s->n++] = ((bits >> i) & 1 ? 1687 : 562) * percent / 100;
    }
    s->us[s->n++] = 562 * percent / 100;
}

int main(void) {
    static ir_signal_t s;
    uint16_t address;
    uint8_t command;

    /* Standard NEC: address, ~address, command, ~command (LSB first) */
    nec(&s, 0x04 | 0xFBu << 8 | 0x08u << 16 | 0xF7u << 24, 100);
    CHECK_EQ(s.n, 67);
    CHECK(ir_decode_nec(&s, &address, &command));
    CHECK_EQ(address, 0x04);
    CHECK_EQ(command, 0x08);

    /* The remotes are not precise: +-20% */
    nec(&s, 0x10 | 0xEFu << 8 | 0x42u << 16 | 0xBDu << 24, 120);
    CHECK(ir_decode_nec(&s, &address, &command));
    CHECK_EQ(command, 0x42);
    nec(&s, 0x10 | 0xEFu << 8 | 0x42u << 16 | 0xBDu << 24, 80);
    CHECK(ir_decode_nec(&s, &address, &command));

    /* Extended NEC: 16 bits address */
    nec(&s, 0x34 | 0x12u << 8 | 0x08u << 16 | 0xF7u << 24, 100);
    CHECK(ir_decode_nec(&s, &address, &command));
    CHECK_EQ(address, 0x1234);

    /* Invalid: wrong command check, wrong header, too short */
    nec(&s, 0x04 | 0xFBu << 8 | 0x08u << 16 | 0xF6u << 24, 100);
    CHECK(! ir_decode_nec(&s, &address, &command));
    nec(&s, 0x04 | 0xFBu << 8 | 0x08u << 16 | 0xF7u << 24, 100);
    s.us[0] = 2400;  /* Sony SIRC header */
    CHECK(! ir_decode_nec(&s, &address, &command));
    s.n = 20;
    CHECK(! ir_decode_nec(&s, &address, &command));

    TEST_END();
}
