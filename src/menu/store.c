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
#define STORE_EXT_VERSION 3  /* 2: cards of 512 bytes (longer URLs); 3: 20 announcements (the contacts are erased) */
/* The flash, from its end: the store (1 sector), 8 KB (the contacts of the older firmwares, unused since version 3),
 * the factory settings (1 sector, never moved: they survive the new firmwares), the contacts and the announcements
 * (12 KB, below the factory settings since version 3) */
#define STORE_FACTORY_VERSION 1
#define STORE_FACTORY_OFFSET (STORE_OFFSET - 8192 - FLASH_SECTOR_SIZE)
#define STORE_EXT_OFFSET (STORE_FACTORY_OFFSET - STORE_EXT_SIZE)

_Static_assert(sizeof(store_t) <= FLASH_SECTOR_SIZE, "the store must fit in a flash sector");

static store_t store;
static bool dirty = false;
static absolute_time_t dirty_ts = 0;
static uint8_t last_page[FLASH_PAGE_SIZE] __attribute__((aligned(4)));  /* The end of a structure, padded with 0xFF */
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


/* Writes \p len bytes of \p data (RAM) at \p offset, erased before: the whole pages straight from \p data, the last
 * one padded with 0xFF (no copy of a whole sector: the RAM is short). Runs with the interrupts disabled */
static void program(uint32_t offset, const void *data, size_t len) {
    size_t whole = len / FLASH_PAGE_SIZE * FLASH_PAGE_SIZE;
    if (whole)
        flash_range_program(offset, data, whole);
    if (len > whole) {
        memset(last_page, 0xFF, sizeof(last_page));
        memcpy(last_page, (const uint8_t *)data + whole, len - whole);
        flash_range_program(offset + whole, last_page, FLASH_PAGE_SIZE);
    }
}


static void save_factory(void *param) {
    (void)param;
    flash_range_erase(STORE_FACTORY_OFFSET, FLASH_SECTOR_SIZE);
    program(STORE_FACTORY_OFFSET, &factory, sizeof(factory));
}


bool store_factory_save(void) {
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


void store_save_now(void) {
    dirty = true;
    dirty_ts = 0;
    store_task(get_absolute_time());
}


/* Runs with the interrupts disabled, from RAM (the flash is not readable while it is written) */
static void save(void *param) {
    (void)param;
    flash_range_erase(STORE_OFFSET, FLASH_SECTOR_SIZE);
    program(STORE_OFFSET, &store, sizeof(store));
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
    int r = flash_safe_execute(save, NULL, 100);
    printf("store: saved (%s)\n", r == PICO_OK ? "ok" : "error");
}
