/* For ui.c in the test of flipper.c */
#pragma once
#include <stdint.h>
extern uint64_t host_time_us;
static inline uint64_t time_us_64(void) { return host_time_us; }
