/* store.h stand-in, for the tests of rsvp.c */
#pragma once
#include <stdint.h>
typedef struct { uint16_t rsvp_wpm; uint32_t rsvp_hash, rsvp_offset; } store_t;
extern store_t host_store;
static inline store_t *store_get(void) { return &host_store; }
static inline void store_changed(void) {}
