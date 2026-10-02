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
#include "ook_tx.h"
#include "radio.h"
#include "radio_tools.h"
#include "remote.h"
#include "store.h"

#define REPEATS 5  /* An admin command is sent 5 times over 2 s by the network (after its Princeton frames): */
#define REPEAT_MS 450  /* a badge listening to a remote at that moment does not hear the network */
#define SEEN_SLOTS 8
#define SEEN_MS 10000  /* The same command (sender + nonce) is executed once */
#define PRINCETON_SEEN_MS 1500  /* A remote repeats its code while its button is held */
#define CIGALE_MS 6000
#define OOK_GIVE_UP_MS 2000  /* The Princeton frames not sent after this: the network packets anyway */
#define OOK_FRAMES 12  /* Princeton frames of an admin command (~0.6 s) */
#define SAME_COMMAND_MS 4000  /* The command came by the network and in OOK: executed once */
#define OOK_WINDOW_MS 150  /* A window: enough to see that a remote sends (~95 edges per 100 ms), then extended */
#define OOK_PERIOD_MS 800  /* After a window: the next one not before (a transmitter that never stops) */
#define OOK_PERIOD_MAX_MS 3200  /* Doubled after each window without a remote decoded (an interferer); not more: a
                                * remote must still be heard when its button is held ~3 s */
#define RSSI_POLL_MS 20
#define OOK_TRIGGER_DBM (-90)  /* Without tuning (the noise is ~-105 dBm) */
#define OOK_TRIGGER_ABOVE_NOISE 15  /* With the tuning (radio_tune.c): the noise measured + 15 dB */
#define OOK_STRONG_DBM (-75)  /* A remote close to the badge: listened to even during the pause of the windows */
#define OOK_TRIGGER_POLLS 2  /* Measures in a row above the trigger, without a packet of the network */
#define OOK_FORCED_MS 10000  /* A window anyway, for a remote weaker than the trigger */
#define OOK_WINDOW_MAX_MS 1500  /* A window is extended while pulses come (a remote is sending) */
#define OOK_ACTIVE_PULSES 30  /* A remote is sending: at least this many edges in 100 ms (a Princeton frame gives ~95,
                                * the noise and the GFSK packets of the badges much fewer) */

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
static uint32_t window_pulses = 0;
static int windows_paused = 0;
static bool ook_pending = false;  /* The Princeton frames of the command to send */
static absolute_time_t ook_pending_ts = 0;
static absolute_time_t rssi_ts = 0, forced_ts = 0;
static int loud_polls = 0;
static uint32_t window_frames = 0;  /* Frames decoded before the window */
static uint32_t window_period_ms = OOK_PERIOD_MS;
static uint8_t last_command = 0;  /* Received by the network and in OOK: executed once */
static absolute_time_t last_command_at = 0;


static bool is_muted(void) {
    return store_get()->muted == 1;
}


bool remote_muted(void) {
    return is_muted();
}


void remote_set_muted(bool m) {
    printf("remote: %s\n", m ? "muted" : "unmuted");
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
    printf("remote: %s\n", e ? "enabled" : "disabled");
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


static bool sleep_requested = false;

bool remote_sleep_requested(void) {
    bool r = sleep_requested;
    sleep_requested = false;
    return r;
}

void remote_execute(uint8_t command, const char *from) {
    printf("remote: command 0x%02x from %s\n", command, from);
    last_command = command;
    last_command_at = get_absolute_time();
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
    case REMOTE_SLEEP:
        if (strcmp(from, "this badge")) {
            sleep_requested = true;  /* main.c: not the talk badge */
            snprintf(event, sizeof(event), "Mise en sommeil");
        } else {
            snprintf(event, sizeof(event), "Ordre de sommeil envoyé");  /* The admin badge stays awake */
        }
        break;
    default:
        if (handlers[command >> 4])
            handlers[command >> 4](command & 0x0F);
        break;
    }
    event_pending = true;
}


/* The admin badge sends a command both in OOK (Princeton) and by the network: the second one is ignored */
static bool same_command(uint8_t command, absolute_time_t now) {
    return command == last_command && last_command_at
           && absolute_time_diff_us(last_command_at, now) < SAME_COMMAND_MS * 1000ll;
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
    if (same_command(p->data[0], p->at))
        return;
    char from[16];
    snprintf(from, sizeof(from), "%08lX", (unsigned long)p->src);
    remote_execute(p->data[0], from);
}


/* The remote of the Flipper Zero (Sub-GHz app, a saved Princeton file): OK sends the code of the file, the arrows
 * the same code with another button in the low nibble: up 2, down 4, left 8, right F. The commands missing from a
 * group are put on these buttons, so that one file drives a whole group:
 * - file C16A01: OK cicada, up mute, down end of the mute;
 * - file C16A11: OK green, up orange (5 min), right red (done), down angry, left off. */
static uint8_t flipper_buttons(uint8_t command) {
    switch (command) {
    case 0x04: return REMOTE_UNMUTE;
    case 0x18: return REMOTE_TALK + 0;
    case 0x1F: return REMOTE_TALK + 3;
    default: return command;
    }
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
    uint8_t command = flipper_buttons(code & 0xFF);
    if (command == REMOTE_SLEEP)
        return;  /* Only from the network of the badges: any Flipper could put the conference to sleep */
    if (same_command(command, now))
        return;
    remote_execute(command, "Princeton");
}


void remote_send(uint8_t command) {
    to_send = command;
    sends_left = REPEATS;
    send_nonce = get_rand_32();
    next_send = get_absolute_time();
    ook_pending = command != REMOTE_SLEEP;  /* First the Princeton frames (talk badges listen only in OOK), then the
                                              * network; the sleep only by the network (the talk badges stay awake) */
    ook_pending_ts = get_absolute_time();
    remote_execute(command, "this badge");  /* The admin badge obeys too */
}


void remote_init(void) {
    net_subscribe(NET_COMMAND, handle_command);
    /* Our codes (0xC16Axx) from a single frame: a short press of the Flipper sends ~3 frames, and the moment of
     * listening starts after the first ones (the 16 bits of the address make a false decoding unlikely) */
    ookdec_trust(REMOTE_PRINCETON_ADDRESS, 0xFFFF00);
    audio_set_mute(is_muted());
}


int remote_trigger_dbm(void) {
    const store_t *s = store_get();
    if (s->radio_tuned != STORE_RADIO_TUNED)
        return OOK_TRIGGER_DBM;
    int t = s->radio_noise_dbm + OOK_TRIGGER_ABOVE_NOISE;
    return t < -95 ? -95 : t > -70 ? -70 : t;
}


void remote_debug(void) {
    printf("remote state: window %d, windows paused %d, ook rx %d, ook tx %d, ook pending %d, sends left %d, "
           "period %lu ms\n", window, windows_paused, ook_rx_active(), ook_tx_busy(), ook_pending, sends_left,
           (unsigned long)window_period_ms);
}


void remote_pause_windows(bool pause) {
    windows_paused += pause ? 1 : -1;
    if (windows_paused < 0)
        windows_paused = 0;
}


void remote_task(absolute_time_t now) {
    ook_tx_task();
    /* An admin command: the same code as a Flipper remote, for the badges that only listen in OOK (talk) */
    if (ook_pending && absolute_time_diff_us(ook_pending_ts, now) > OOK_GIVE_UP_MS * 1000ll) {
        ook_pending = false;  /* The radio stayed busy: the network packets go anyway */
        printf("remote: princeton not sent (radio busy)\n");
    }
    if (ook_pending && ook_rx_active() && ! window)
        ook_pending = false;  /* This badge listens in OOK itself (talk badge): the network only */
    if (ook_pending && ! window && radio_tools_idle()) {
        if (ook_tx_princeton(REMOTE_PRINCETON_ADDRESS | to_send, OOK_FRAMES)) {
            ook_pending = false;
            printf("remote: princeton 0x%06lX sent\n", (unsigned long)(REMOTE_PRINCETON_ADDRESS | to_send));
        }
    }
    if (ook_tx_busy() || ook_pending) {
        if (window) {
            ook_rx_stop();  /* The radio is needed to send */
            window = false;
        }
        return;  /* No listening window, no packet while the frames are sent */
    }
    /* The remotes (Flipper Zero, Princeton): the network listens all the time; a transmitter heard (RSSI) without
     * any packet of the network (no sync word) is maybe a remote: then a moment in OOK, ook_rx.c decodes it.
     * No blind window: the packets of the network are not lost any more (with a window every 800 ms, ~15 % were) */
    if (! window) {
        if (remote_enabled() && ! windows_paused && radio_tools_idle() && net_idle() && ! net_chat()
                && absolute_time_diff_us(rssi_ts, now) >= 0) {
            rssi_ts = delayed_by_ms(now, RSSI_POLL_MS);
            uint8_t raw = 0;
            radio_read_registers(CC1101_RSSI, &raw, 1);
            int rssi = (int8_t)raw / 2 - 74;
            loud_polls = rssi >= remote_trigger_dbm() && ! net_transmitting() ? loud_polls + 1 : 0;
            /* The pause after a window without a remote (an interferer) does not stop a strong signal: a remote
             * close to the badge */
            bool allowed = absolute_time_diff_us(window_ts, now) >= 0 || rssi >= OOK_STRONG_DBM;
            if (allowed && (loud_polls >= OOK_TRIGGER_POLLS || absolute_time_diff_us(forced_ts, now) >= 0)) {
                /* A transmitter, or now and then anyway (a remote weaker than the trigger) */
                loud_polls = 0;
                forced_ts = delayed_by_ms(now, OOK_FORCED_MS);
                ook_rx_start();
                window = true;
                window_start = now;
                window_pulses = ook_rx_pulses();
                window_frames = ook_rx_frames();
                window_ts = delayed_by_ms(now, OOK_WINDOW_MS);
            }
        }
    } else if (absolute_time_diff_us(window_ts, now) >= 0 && ook_rx_pulses() - window_pulses >= OOK_ACTIVE_PULSES
               && absolute_time_diff_us(window_start, now) < OOK_WINDOW_MAX_MS * 1000ll
               && radio_tools_idle() && ! windows_paused) {
        window_pulses = ook_rx_pulses();
        window_ts = delayed_by_ms(now, 100);  /* A remote is sending: listen until it stops (the frames repeat) */
    } else if (absolute_time_diff_us(window_ts, now) >= 0 || ! radio_tools_idle() || windows_paused) {
        ook_rx_stop();  /* Decodes what came */
        window = false;
        /* Nothing decoded: a transmitter that is not a remote (the network is deaf during the windows), the next
         * window waits longer and longer; a remote decoded: back to the shortest period */
        if (ook_rx_frames() != window_frames)
            window_period_ms = OOK_PERIOD_MS;
        else if (window_period_ms < OOK_PERIOD_MAX_MS)
            window_period_ms *= 2;
        window_ts = delayed_by_ms(now, window_period_ms);
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
