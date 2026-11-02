/* store.h stand-in, for the test of flipper.c */
#pragma once
#include <stdint.h>
typedef struct { uint8_t theme; } store_t;
extern store_t host_store;
static inline store_t *store_get(void) { return &host_store; }
static inline void store_changed(void) {}
