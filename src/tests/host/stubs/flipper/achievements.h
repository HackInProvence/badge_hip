/* achievements.h stand-in, for the test of flipper.c */
#pragma once
#include <stdint.h>
extern uint8_t host_level;
static inline uint8_t achv_level(void) { return host_level; }
