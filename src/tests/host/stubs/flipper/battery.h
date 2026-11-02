/* battery.h stand-in, for the test of flipper.c */
#pragma once
#include <stdbool.h>
extern bool host_battery_low;
static inline bool battery_low(void) { return host_battery_low; }
