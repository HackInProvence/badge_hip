/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>
#include <string.h>

#include "hardware/flash.h"
#include "pico/flash.h"

#include "store.h"


#define STORE_MAGIC 0x41474943  /* "CIGA" */
#define STORE_VERSION 1
#define STORE_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define SAVE_DELAY_US 5000000

_Static_assert(sizeof(store_t) <= FLASH_SECTOR_SIZE, "the store must fit in a flash sector");

static store_t store;
static bool dirty = false;
static absolute_time_t dirty_ts = 0;
static uint8_t sector[FLASH_SECTOR_SIZE] __attribute__((aligned(4)));


void store_init(void) {
    memcpy(&store, (const void *)(XIP_BASE + STORE_OFFSET), sizeof(store));
    if (store.magic != STORE_MAGIC || store.version != STORE_VERSION || store.n_met > STORE_MAX_MET) {
        memset(&store, 0, sizeof(store));
        store.magic = STORE_MAGIC;
        store.version = STORE_VERSION;
        memset(store.puzzle_records, 0xFF, sizeof(store.puzzle_records));  /* No record yet */
        printf("store: initialized\n");
    }
    for (uint16_t i = 0; i < store.n_met; ++i)
        store.met[i].last_minute = 0xFFFF;
    for (int i = 0; i < STORE_IR_SLOTS; ++i)
        if (store.ir[i].n > IR_MAX_PULSES)
            store.ir[i].n = 0;
}


store_t *store_get(void) {
    return &store;
}


void store_changed(void) {
    dirty = true;
    dirty_ts = get_absolute_time();
}


/* Runs with the interrupts disabled, from RAM (the flash is not readable while it is written) */
static void save(void *param) {
    (void)param;
    flash_range_erase(STORE_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(STORE_OFFSET, sector, FLASH_SECTOR_SIZE);
}


void store_task(absolute_time_t now) {
    if (! dirty || absolute_time_diff_us(dirty_ts, now) < SAVE_DELAY_US)
        return;
    dirty = false;
    memset(sector, 0xFF, sizeof(sector));
    memcpy(sector, &store, sizeof(store));
    int r = flash_safe_execute(save, NULL, 100);
    printf("store: saved (%s)\n", r == PICO_OK ? "ok" : "error");
}
