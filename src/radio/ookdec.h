/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file ookdec.h
 *
 * \brief Receive only decoders of common 433.92MHz OOK (on-off keying) devices: remote controls and wireless
 * thermometers, like a light version of rtl_433 or of the Sub-GHz / Weather Station apps of the Flipper Zero.
 *
 * The CC1101 in asynchronous OOK receive mode outputs the demodulated signal on a GPIO: high while it receives
 * the carrier. An interrupt records the durations between the edges; this module only computes on these
 * durations (no hardware access, tested on the PC: tests/host/test_ookdec.c).
 *
 * Supported (details and references in ookdec.c):
 * - remotes: Princeton (PT2262, EV1527, HS1527... 24 bits), CAME 12/24 bits, Nice FLO 12/24 bits,
 * - thermometers: Nexus-TH, inFactory-TH, ThermoPRO-TX4, GT-WT02, LaCrosse TX141TH-Bv2, Acurite 592TXR.
 *
 * Feeding the decoder:
 * - us[0] is a high (carrier on) duration, then low, high, low... Drop the low level before the first edge.
 * - Durations are clamped to 65535µs. Glitches shorter than OOKDEC_GLITCH_US are ignored.
 * - The signal may contain noise before and after, and several repeats of a frame: the thermometers repeat their
 *   frame 3 to 12 times, the remotes as long as the button is pressed. The protocols without checksum (Princeton,
 *   CAME, Nice FLO, Nexus-TH, ThermoPRO-TX4) must be received twice identically, like the Flipper does.
 * - The longest silence inside a transmission is 25ms (Nice FLO guard time, inFactory 16ms), so a capture can end
 *   after ~40ms without edge, or when the buffer is full. A buffer of 512 durations holds at least 2 frames of
 *   every protocol (a Nexus frame is 74 durations, a LaCrosse one 88, an Acurite one 120).
 * - The decoding does not allocate: it uses a static work buffer of 1KB (not reentrant). A full buffer of noise
 *   (the worst case) takes 80µs on a PC, a few milliseconds on the RP2040: call it from the main loop, not from
 *   the interrupt, e.g. after each silence longer than 5ms or when the buffer is full.
 * - Known ambiguity: ThermoPRO-TX4 and GT-WT02 have the same timings and size; a ThermoPRO frame happens to have a
 *   right GT-WT02 checksum about once in 300 (the Flipper Weather Station app has the same problem).
 * */

#ifndef _OOKDEC_H
#define _OOKDEC_H

#include <stdbool.h>
#include <stdint.h>

#define OOKDEC_MAX_PULSES 512
#define OOKDEC_GLITCH_US 80  /* Shorter pulses and gaps are receiver glitches: merged with their neighbours */
#define OOKDEC_NO_HUMIDITY 0xFF

typedef struct {
    uint16_t n;  /* Number of durations */
    uint16_t us[OOKDEC_MAX_PULSES];  /* Alternating high (carrier on), low, high... starting with high, in µs */
} ookdec_signal_t;

typedef struct {
    char protocol[20];  /* Name as in the Flipper Zero: "Princeton", "Nexus-TH", "LaCrosse_TX141THBv2"... */
    uint64_t code;  /* Raw bits of the frame (MSB first), the key of a remote */
    uint8_t bits;  /* Number of bits in code */
    uint16_t te;  /* Remotes: measured elementary duration (µs) */
    bool weather;  /* The next fields are valid */
    int16_t temp_c10;  /* Temperature in tenths of °C */
    uint8_t humidity;  /* %, OOKDEC_NO_HUMIDITY when the sensor has none */
    uint8_t channel;  /* 1 to 3 (4 for the "X" channel of some sensors) */
    uint16_t id;  /* Random ID of the sensor, changes when the batteries are replaced (14 bits for Acurite) */
    bool battery_low;
    char text[48];  /* One line summary for the screen (UTF-8, < 30 characters), e.g. "Nexus-TH ch2 21.3°C 45%" */
} ookdec_result_t;

/** \brief Try all the protocols on a signal.
 * \return true when a frame was decoded (and its checksum, or its repeat, validated), described in \p r */
bool ookdec_decode(const ookdec_signal_t *s, ookdec_result_t *r);

#endif /* _OOKDEC_H */
