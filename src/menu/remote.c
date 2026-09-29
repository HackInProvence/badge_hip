/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>
#include <string.h>

#include "pico/rand.h"

#include "audio.h"
#include "net.h"
#include "noise_gen.h"
#include "ook_rx.h"
#include "radio_tools.h"
#include "remote.h"
#include "store.h"

#define REPEATS 3  /* An admin command is sent 3 times (some badges miss a packet) */
#define REPEAT_MS 150
#define SEEN_SLOTS 8
#define SEEN_MS 10000  /* The same command (sender + nonce) is executed once */
#define PRINCETON_SEEN_MS 1500  /* A remote repeats its code while its button is held */
#define CIGALE_MS 6000
#define OOK_WINDOW_MS 220  /* 2 frames of a Princeton remote (~50 ms each) whatever the start */
#define OOK_PERIOD_MS 800  /* Hold the button of the remote ~1 s */
#define OOK_WINDOW_MAX_MS 1500  /* A window is extended while pulses come (a remote is sending) */
#define OOK_ACTIVE_US 60000  /* Pulses within this time: activity */

typedef struct {
    uint32_t src;
    uint16_t nonce;
    absolute_time_t at;
} seen_t;

static remote_handler_t handlers[16];
static seen_t seen[SEEN_SLOTS];
static int seen_next = 0;
static uint32_t last_princeton = 0;
static absolute_time_t last_princeton_at = 0;
static bool cigale_on = false;
static absolute_time_t cigale_end = 0;
static uint8_t to_send = 0;
static int sends_left = 0;
static uint16_t send_nonce = 0;
static absolute_time_t next_send = 0;
static char event[40];
static bool event_pending = false;
static bool window = false;  /* Listening to the OOK remotes */
static absolute_time_t window_ts = 0;
static absolute_time_t window_start = 0;
static int windows_paused = 0;


static bool is_muted(void) {
    return store_get()->muted == 1;
}


bool remote_muted(void) {
    return is_muted();
}


void remote_set_muted(bool m) {
    store_get()->muted = m ? 1 : 0;
    store_changed();
    audio_set_mute(m);
    if (m && cigale_on) {
        noise_gen_set_enabled(false);
        cigale_on = false;
    }
}


bool remote_enabled(void) {
    return store_get()->remote_off != 1;
}


void remote_set_enabled(bool e) {
    store_get()->remote_off = e ? 0 : 1;
    store_changed();
}


void remote_subscribe(uint8_t group, remote_handler_t handler) {
    handlers[(group >> 4) & 0x0F] = handler;
}


bool remote_event(char *buf, int len) {
    if (! event_pending)
        return false;
    event_pending = false;
    snprintf(buf, len, "%s", event);
    return true;
}


void remote_execute(uint8_t command, const char *from) {
    printf("remote: command 0x%02x from %s\n", command, from);
    snprintf(event, sizeof(event), "Commande 0x%02X reçue", command);
    switch (command) {
    case REMOTE_CIGALE:
        if (! is_muted()) {
            noise_gen_set_enabled(true);
            cigale_on = true;
            cigale_end = delayed_by_ms(get_absolute_time(), CIGALE_MS);
        }
        snprintf(event, sizeof(event), "La cigale chante !");
        break;
    case REMOTE_MUTE:
        remote_set_muted(true);
        snprintf(event, sizeof(event), "Mode muet (conférence)");
        break;
    case REMOTE_UNMUTE:
        remote_set_muted(false);
        snprintf(event, sizeof(event), "Fin du mode muet");
        break;
    default:
        if (handlers[command >> 4])
            handlers[command >> 4](command & 0x0F);
        break;
    }
    event_pending = true;
}


/* The same sender and nonce was received less than SEEN_MS ago */
static bool already_seen(uint32_t src, uint16_t nonce, absolute_time_t now) {
    for (int i = 0; i < SEEN_SLOTS; ++i)
        if (seen[i].src == src && seen[i].nonce == nonce && absolute_time_diff_us(seen[i].at, now) < SEEN_MS * 1000ll)
            return true;
    seen[seen_next] = (seen_t){src, nonce, now};
    seen_next = (seen_next + 1) % SEEN_SLOTS;
    return false;
}


static void handle_command(const net_packet_t *p) {
    if (p->len < 3 || ! remote_enabled())
        return;
    uint16_t nonce = p->data[1] | p->data[2] << 8;
    if (already_seen(p->src, nonce, p->at))
        return;
    char from[16];
    snprintf(from, sizeof(from), "%08lX", (unsigned long)p->src);
    remote_execute(p->data[0], from);
}


void remote_princeton(uint32_t code) {
    if ((code & 0xFFFF00) != REMOTE_PRINCETON_ADDRESS || ! remote_enabled())
        return;
    absolute_time_t now = get_absolute_time();
    if (code == last_princeton && absolute_time_diff_us(last_princeton_at, now) < PRINCETON_SEEN_MS * 1000ll) {
        last_princeton_at = now;  /* Still the same press */
        return;
    }
    last_princeton = code;
    last_princeton_at = now;
    remote_execute(code & 0xFF, "Princeton");
}


void remote_send(uint8_t command) {
    to_send = command;
    sends_left = REPEATS;
    send_nonce = get_rand_32();
    next_send = get_absolute_time();
    remote_execute(command, "this badge");  /* The admin badge obeys too */
}


void remote_init(void) {
    net_subscribe(NET_COMMAND, handle_command);
    audio_set_mute(is_muted());
}


void remote_pause_windows(bool pause) {
    windows_paused += pause ? 1 : -1;
    if (windows_paused < 0)
        windows_paused = 0;
}


void remote_task(absolute_time_t now) {
    /* A moment every second, listen to the Princeton remotes (Flipper Zero): ook_rx.c decodes them */
    if (! window) {
        if (remote_enabled() && ! windows_paused && radio_tools_idle() && net_idle()
                && absolute_time_diff_us(window_ts, now) >= 0) {
            ook_rx_start();
            window = true;
            window_start = now;
            window_ts = delayed_by_ms(now, OOK_WINDOW_MS);
        }
    } else if (absolute_time_diff_us(window_ts, now) >= 0 && ook_rx_quiet_us() < OOK_ACTIVE_US
               && absolute_time_diff_us(window_start, now) < OOK_WINDOW_MAX_MS * 1000ll
               && radio_tools_idle() && ! windows_paused) {
        window_ts = delayed_by_ms(now, 100);  /* Something is sending: listen until it stops (the frames repeat) */
    } else if (absolute_time_diff_us(window_ts, now) >= 0 || ! radio_tools_idle() || windows_paused) {
        ook_rx_stop();
        window = false;
        window_ts = delayed_by_ms(now, OOK_PERIOD_MS);
    }

    if (cigale_on && absolute_time_diff_us(cigale_end, now) >= 0) {
        noise_gen_set_enabled(false);
        cigale_on = false;
    }
    if (sends_left && absolute_time_diff_us(next_send, now) >= 0) {
        uint8_t data[3] = {to_send, send_nonce, send_nonce >> 8};
        if (net_send(NET_COMMAND, data, sizeof(data), NET_LOUD)) {
            --sends_left;
            next_send = delayed_by_ms(now, REPEAT_MS);
        }
    }
}
