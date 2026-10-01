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
#define STORE_EXT_MAGIC 0x544E4F43  /* "CONT" */
#define STORE_EXT_VERSION 2  /* 2: cards of 512 bytes (longer URLs) */
#define STORE_EXT_OFFSET (STORE_OFFSET - STORE_EXT_SIZE)
#define STORE_FACTORY_VERSION 1
#define STORE_FACTORY_OFFSET (STORE_EXT_OFFSET - FLASH_SECTOR_SIZE)

_Static_assert(sizeof(store_t) <= FLASH_SECTOR_SIZE, "the store must fit in a flash sector");

static store_t store;
static bool dirty = false;
static absolute_time_t dirty_ts = 0;
static uint8_t sector[FLASH_SECTOR_SIZE] __attribute__((aligned(4)));
static store_ext_t ext __attribute__((aligned(4)));
static store_factory_t factory;
static bool ext_dirty = false;
static absolute_time_t ext_dirty_ts = 0;

_Static_assert(sizeof(store_ext_t) == STORE_EXT_SIZE && STORE_EXT_SIZE % FLASH_SECTOR_SIZE == 0, "whole sectors");


void store_init(void) {
    memcpy(&store, (const void *)(XIP_BASE + STORE_OFFSET), sizeof(store));
    if (store.magic != STORE_MAGIC || store.version != STORE_VERSION || store.n_met > STORE_MAX_MET) {
        memset(&store, 0, sizeof(store));
        store.magic = STORE_MAGIC;
        store.version = STORE_VERSION;
        memset(store.puzzle_records, 0xFF, sizeof(store.puzzle_records));  /* No record yet */
        printf("store: initialized\n");
    }
    if (store.v2_magic != STORE_V2_MAGIC) {  /* The fields added later: their defaults */
        store.v2_magic = STORE_V2_MAGIC;
        store.skills = 0;
        store.achievements = 0;
        memset(store.achv_counters, 0, sizeof(store.achv_counters));
        store.book_hash = 0;
        store.book_section = 0;
        memset(store.cargo, 0, sizeof(store.cargo));
        store.cargo_seeded = 0;
    }
    for (uint16_t i = 0; i < store.n_met; ++i)
        store.met[i].last_minute = 0xFFFF;
    memcpy(&ext, (const void *)(XIP_BASE + STORE_EXT_OFFSET), sizeof(ext));
    if (ext.magic != STORE_EXT_MAGIC || ext.version != STORE_EXT_VERSION || ext.n_contacts > STORE_CONTACTS) {
        memset(&ext, 0, sizeof(ext));
        ext.magic = STORE_EXT_MAGIC;
        ext.version = STORE_EXT_VERSION;
        ext.send_mask = 0x0003;  /* First name and name */
    }
    memcpy(&factory, (const void *)(XIP_BASE + STORE_FACTORY_OFFSET), sizeof(factory));
    if (factory.magic != STORE_FACTORY_MAGIC || factory.version != STORE_FACTORY_VERSION) {
        memset(&factory, 0, sizeof(factory));  /* Never set (an erased flash reads 0xFF) */
        factory.magic = STORE_FACTORY_MAGIC;
        factory.version = STORE_FACTORY_VERSION;
    }
    for (int i = 0; i < STORE_IR_SLOTS; ++i)
        if (store.ir[i].n > IR_MAX_PULSES)
            store.ir[i].n = 0;
}


store_t *store_get(void) {
    return &store;
}


store_ext_t *store_ext_get(void) {
    return &ext;
}


store_factory_t *store_factory_get(void) {
    return &factory;
}


static void save_factory(void *param) {
    (void)param;
    flash_range_erase(STORE_FACTORY_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(STORE_FACTORY_OFFSET, sector, FLASH_SECTOR_SIZE);
}


bool store_factory_save(void) {
    if (dirty) {  /* The sector buffer is shared: write the pending store first */
        dirty_ts = 0;
        store_task(get_absolute_time());
    }
    memset(sector, 0xFF, sizeof(sector));
    memcpy(sector, &factory, sizeof(factory));
    int r = flash_safe_execute(save_factory, NULL, 100);
    printf("store: factory settings saved (%s)\n", r == PICO_OK ? "ok" : "error");
    return r == PICO_OK;
}


void store_ext_changed(void) {
    ext_dirty = true;
    ext_dirty_ts = get_absolute_time();
}


static void save_ext(void *param) {
    (void)param;
    flash_range_erase(STORE_EXT_OFFSET, STORE_EXT_SIZE);
    flash_range_program(STORE_EXT_OFFSET, ext.raw, STORE_EXT_SIZE);
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
    if (ext_dirty && absolute_time_diff_us(ext_dirty_ts, now) >= SAVE_DELAY_US) {
        ext_dirty = false;
        int r = flash_safe_execute(save_ext, NULL, 100);
        printf("store: contacts saved (%s)\n", r == PICO_OK ? "ok" : "error");
    }
    if (! dirty || absolute_time_diff_us(dirty_ts, now) < SAVE_DELAY_US)
        return;
    dirty = false;
    memset(sector, 0xFF, sizeof(sector));
    memcpy(sector, &store, sizeof(store));
    int r = flash_safe_execute(save, NULL, 100);
    printf("store: saved (%s)\n", r == PICO_OK ? "ok" : "error");
}
