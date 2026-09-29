/* screen.h stand-in, for the tests of rsvp.c (the display is not tested here) */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define SCREEN_WIDTH 200
#define SCREEN_HEIGHT 200
extern const uint8_t screen_ws_10fps[], screen_ws_20fps[];
static inline bool screen_boot(void) { return true; }
static inline bool screen_busy(void) { return false; }
static inline void screen_clear(bool b) { (void)b; }
static inline size_t screen_set_image_position(uint8_t a, uint8_t b, uint8_t c, uint8_t d) { (void)a; (void)b; (void)c; (void)d; return 0; }
#define screen_clear_image_position() screen_set_image_position(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT)
static inline void screen_push_ws(const uint8_t *w) { (void)w; }
static inline void screen_start_multiframe(void) {}
static inline void screen_end_multiframe(void) {}
static inline void screen_draw_multiframe(void) {}
static inline void screen_push_rams(const uint8_t *a, const uint8_t *b, size_t n) { (void)a; (void)b; (void)n; }
