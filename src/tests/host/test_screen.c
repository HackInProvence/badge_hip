/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Host tests of the screen module: RAM window, copy of the screen (screen_shot()), clear */

#include "pico_host.h"
#include "../../screen/screen.c"  /* Included to access its static state */
#include "test.h"

uint64_t host_time_us = 0;
bool host_gpio[32];
uint8_t host_spi_log[65536];
size_t host_spi_len = 0;
void log_printf(log_level lev, const char *fmt, ...) { (void)lev; (void)fmt; }

int main(void) {
    static uint8_t data[5000];
    const uint8_t *lsb, *msb;
    state = STATE_READY;  /* As if screen_boot() was done */

    /* Full screen: the RAM is filled from its end, which is the top left of the image */
    CHECK_EQ(screen_clear_image_position(), 5000);
    for (int k = 0; k < 5000; ++k)
        data[k] = k * 7;
    screen_push_rams(data, NULL, 5000);
    screen_shot(&lsb, &msb);
    CHECK(! memcmp(lsb, data, 5000));

    /* Window of 100x100 pixels at (32, 18) in RAM coordinates: 13 bytes wide (x 32..135 -> bytes 4..16) */
    CHECK_EQ(screen_set_image_position(32, 18, 32 + 104, 18 + 100), 1300);
    CHECK_EQ(win_x0, 4);
    CHECK_EQ(win_x1, 16);
    CHECK_EQ(win_y0, 18);
    CHECK_EQ(win_y1, 117);
    for (int k = 0; k < 1300; ++k)
        data[k] = 0xA0 + k % 13;  /* The column in the window */
    screen_push_rams(NULL, data, 1300);
    screen_shot(&lsb, &msb);
    /* RAM (x, y) is the byte (24 - x, 199 - y) of the image: the window is at columns 8..20, rows 82..181 */
    CHECK_EQ(msb[82 * 25 + 8], 0xA0);
    CHECK_EQ(msb[82 * 25 + 20], 0xA0 + 12);
    CHECK_EQ(msb[181 * 25 + 8], 0xA0);
    CHECK_EQ(msb[81 * 25 + 8], 0);  /* Outside of the window: untouched */
    CHECK_EQ(msb[82 * 25 + 21], 0);

    /* Partial bytes are kept in the window, no underflow */
    CHECK_EQ(screen_set_image_position(3, 0, 5, 1), 1);
    CHECK_EQ(screen_set_image_position(0, 0, 0, 0), 1);
    CHECK_EQ(screen_set_image_position(0, 0, 9, 1), 2);  /* 9 pixels: 2 bytes */
    CHECK_EQ(screen_set_image_position(200, 200, 200, 200), 1);  /* Clamped */

    /* How the screen was drawn */
    uint32_t n = screen_shot_counter();
    screen_show_image_4g(data, data);
    CHECK_EQ(screen_shot(&lsb, &msb), SCREEN_SHOT_4G);
    CHECK_EQ(screen_shot_counter(), n + 1);
    screen_show_image_bw(data);
    CHECK_EQ(screen_shot(&lsb, &msb), SCREEN_SHOT_BW);
    screen_clear(1);
    CHECK_EQ(screen_shot(&lsb, &msb), SCREEN_SHOT_WHITE);
    screen_clear(0);
    CHECK_EQ(screen_shot(&lsb, &msb), SCREEN_SHOT_BLACK);

    /* After a clear, the next draw stops bypassing the RAM (DISPLAY_CTRL1 = 0) */
    host_spi_len = 0;
    screen_show_rams();
    CHECK(host_spi_len >= 2 && host_spi_log[0] == SSD1681_DISPLAY_CTRL1 && host_spi_log[1] == 0x00);
    host_spi_len = 0;
    screen_show_rams();
    CHECK(host_spi_log[0] != SSD1681_DISPLAY_CTRL1);  /* Only once */

    /* Clean with the waveform of the screen (OTP): uniform color, then full update 0xF7 */
    host_spi_len = 0;
    screen_clean(1);
    CHECK_EQ(host_spi_log[0], SSD1681_DISPLAY_CTRL1);
    CHECK_EQ(host_spi_log[1], 0x55);
    CHECK_EQ(host_spi_log[2], SSD1681_DISPLAY_CTRL2);
    CHECK_EQ(host_spi_log[3], 0xF7);
    CHECK_EQ(host_spi_log[4], SSD1681_ACTIVATE);
    CHECK_EQ(screen_shot(&lsb, &msb), SCREEN_SHOT_WHITE);
    host_spi_len = 0;
    screen_show_image_4g(data, data);  /* Pushes its waveform, restores the RAM */
    CHECK(host_spi_log[0] != SSD1681_DISPLAY_CTRL1);  /* The window first */
    bool restored = false;
    for (size_t i = 0; i + 1 < host_spi_len && ! restored; ++i)
        restored = host_spi_log[i] == SSD1681_DISPLAY_CTRL1 && host_spi_log[i + 1] == 0x00;
    CHECK(restored);

    TEST_END();
}
