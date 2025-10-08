/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>

#include "pico/stdlib.h"

#include "music.h"


#define SIL {SILENCE, 1.f/128}
#define NEVEGONA {NOTE_D5, 1.f/4}, {NOTE_E5, 1.f/4}, {NOTE_G5, 1.f/4}, {NOTE_E5, 1.f/4}
const Note rick[] = {
    // TODO: *60/114
    // Never gonna
    NEVEGONA,
    // give you up Never gonna
    {NOTE_B5, 3.f/4}, SIL, {NOTE_B5, 3.f/4}, {NOTE_A5, 3.f/2}, NEVEGONA,
    // let you down Never gonna
    {NOTE_A5, 3.f/4}, SIL, {NOTE_A5, 3.f/4}, {NOTE_G5, 3.f/4}, {NOTE_FS5, 1.f/4}, {NOTE_E5, 1.f/2}, NEVEGONA,
    // run around and de-
    {NOTE_G5, 1.f/1}, {NOTE_A5, 1.f/2}, {NOTE_FS5, 3.f/4}, {NOTE_E5, 1.f/4}, {NOTE_D5, 1.f/2}, {NOTE_D5, 1.f/2}, {NOTE_D5, 1.f/2},
    // -sert you Never gonna
    {NOTE_A5, 1.f/1}, {NOTE_G5, 15.f/8}, {SILENCE, 1.f/8}, NEVEGONA,
    // make you cry Never gonna
    {NOTE_B5, 3.f/4}, SIL, {NOTE_B5, 3.f/4}, {NOTE_A5, 3.f/2}, NEVEGONA,
    // say goodbye Never gonna
    {NOTE_D6, 1.f/1}, {NOTE_FS5, 1.f/2}, {NOTE_G5, 3.f/4}, {NOTE_FS5, 1.f/4}, {NOTE_E5, 1.f/2}, NEVEGONA,
    // tell a lie and
    {NOTE_G5, 1.f/1}, {NOTE_A5, 1.f/2}, {NOTE_FS5, 3.f/4}, {NOTE_E5, 1.f/4}, {NOTE_D5, 1.f/1}, SIL, {NOTE_D5, 1.f/2},
    // hurt you
    {NOTE_A5, 1.f/1}, {NOTE_G5, 2.f/1},
    {SILENCE, 3.f/1}, // Before repeat
    {}
};

void printstatus(void);
int main() {
    stdio_usb_init();

    music_init();
    printf("init\n");
    music_set_melody(rick, 35.);
    printf("set\n");
    music_set_enabled(true);
    printf("play rick=%p\n", rick);
    while (true) {
        printstatus();
        sleep_ms(300);
    }
}
