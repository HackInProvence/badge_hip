/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Decoders of 433.92MHz OOK remotes and thermometers (see ookdec.h).
 *
 * References (timings, bit layouts, checksums):
 * - rtl_433, https://github.com/merbanan/rtl_433/tree/master/src/devices (nexus.c, infactory.c, thermopro_tx2.c,
 *   gt_wt_02.c, lacrosse_tx141x.c, acurite.c),
 * - Flipper Zero firmware, https://github.com/flipperdevices/flipperzero-firmware/tree/dev/lib/subghz/protocols
 *   (princeton.c, came.c, nice_flo.c),
 * - Flipper Zero Weather Station app, https://github.com/flipperdevices/flipperzero-good-faps/tree/dev/weather_station/protocols
 *   (nexus_th.c, infactory.c, thermopro_tx4.c, gt_wt_02.c, lacrosse_tx141thbv2.c, acurite_592txr.c).
 *
 * Vocabulary: a "pulse" is a high duration (carrier on), a "gap" a low one. The durations of the signal are
 * w[0] (pulse), w[1] (gap), w[2] (pulse)...: even indexes are pulses, odd ones gaps.
 *
 * Method: most frames end with a long gap. Each decoder looks for these gaps, takes the fixed number of durations
 * of a frame before it and checks each one (tolerant ranges: real receivers stretch the pulses by tens of µs and
 * the transmitters are not precise). A frame is accepted when its checksum is right, or, for the protocols without
 * checksum, when the same frame is received twice (like the Flipper does for Princeton). The two thermometers
 * with a PWM coding (LaCrosse, Acurite) are found by their preamble instead.
 * */

#include <stdio.h>
#include <string.h>

#include "ookdec.h"


#define INF 0xFFFFu  /* Duration after the end of the signal */

/* The signal without its glitches */
static uint16_t w[OOKDEC_MAX_PULSES];
static int wn;


/* ---- Tools ---- */

/* Copies the signal in w, removing the glitches: a too short duration is merged with the one before and the one
 * after (e.g. a 30µs gap inside a pulse: pulse + gap + pulse becomes a single pulse) */
static void deglitch(const ookdec_signal_t *s) {
    wn = 0;
    int n = s->n > OOKDEC_MAX_PULSES ? OOKDEC_MAX_PULSES : s->n;
    for (int i = 0; i < n; ++i) {
        uint32_t d = s->us[i];
        if (d < OOKDEC_GLITCH_US) {
            if (wn == 0) {
                ++i;  /* A glitch at the start: drop it and the gap after, to start with a pulse */
                continue;
            }
            /* Merge it and the next one into the previous one (same level) */
            d = w[wn - 1] + d + (i + 1 < n ? s->us[i + 1] : 0);
            w[wn - 1] = d > INF ? INF : d;
            ++i;
            continue;
        }
        w[wn++] = d;
    }
}

/* Duration i, "infinite" after the end: the recording ends with a silence. The last duration, when it is a gap,
 * is also a silence (the recorder stops on it) */
static uint32_t dur(int i) {
    if (i < 0)
        return 0;
    if (i >= wn || (i == wn - 1 && (i & 1)))
        return INF;
    return w[i];
}

static bool in(uint32_t v, uint32_t lo, uint32_t hi) {
    return v >= lo && v <= hi;
}

/* Rounded division (b > 0) */
static int div_round(int a, int b) {
    return a >= 0 ? (a + b / 2) / b : -((-a + b / 2) / b);
}

static uint8_t nibble_sum(uint64_t v, int nibbles) {
    uint8_t sum = 0;
    for (int i = 0; i < nibbles; ++i)
        sum += (v >> (4 * i)) & 0xF;
    return sum;
}

/* CRC-4 over whole bytes, MSB first (rtl_433 bit_util.c crc4) */
static uint8_t crc4(const uint8_t *msg, int n, uint8_t poly, uint8_t init) {
    uint8_t rem = init << 4, p = poly << 4;
    while (n--) {
        rem ^= *msg++;
        for (int b = 0; b < 8; ++b)
            rem = rem & 0x80 ? (uint8_t)(rem << 1) ^ p : (uint8_t)(rem << 1);
    }
    return rem >> 4 & 0x0F;
}

/* LFSR digest, bytes and bits reflected (rtl_433 bit_util.c lfsr_digest8_reflect) */
static uint8_t lfsr_digest8_reflect(const uint8_t *msg, int n, uint8_t gen, uint8_t key) {
    uint8_t sum = 0;
    for (int k = n - 1; k >= 0; --k)
        for (int i = 0; i < 8; ++i) {
            if ((msg[k] >> i) & 1)
                sum ^= key;
            key = key & 0x80 ? (uint8_t)(key << 1) ^ gen : (uint8_t)(key << 1);
        }
    return sum;
}

static uint8_t parity8(uint8_t b) {
    b ^= b >> 4;
    b &= 0xF;
    return (0x6996 >> b) & 1;
}

/* The protocols without checksum must give the same frame twice in the signal */
typedef struct {
    bool seen;
    uint64_t last;
} repeat_t;

static bool repeated(repeat_t *rep, uint64_t v) {
    if (rep->seen && rep->last == v)
        return true;
    rep->seen = true;
    rep->last = v;
    return false;
}

static void set_name(ookdec_result_t *r, const char *protocol, uint64_t code, uint8_t bits) {
    memset(r, 0, sizeof(*r));
    strncpy(r->protocol, protocol, sizeof(r->protocol) - 1);
    r->code = code;
    r->bits = bits;
    r->humidity = OOKDEC_NO_HUMIDITY;
}

/* A thermometer: fills the fields and the text "<tag> ch<channel> <temp>°C <humidity>%" */
static void set_weather(ookdec_result_t *r, const char *protocol, const char *tag, uint64_t code, uint8_t bits,
                        uint8_t channel, uint16_t id, int temp_c10, uint8_t humidity, bool battery_low) {
    set_name(r, protocol, code, bits);
    r->weather = true;
    r->channel = channel;
    r->id = id;
    r->temp_c10 = temp_c10;
    r->humidity = humidity;
    r->battery_low = battery_low;
    int t = temp_c10 < 0 ? -temp_c10 : temp_c10;
    char ch[4];
    if (! strcmp(tag, "Acurite"))
        snprintf(ch, sizeof(ch), "%c", "?ABC"[channel & 3]);  /* Its channels are letters */
    else
        snprintf(ch, sizeof(ch), "%u", channel);
    int len = snprintf(r->text, sizeof(r->text), "%s ch%s %s%d.%d°C", tag, ch,
                       temp_c10 < 0 ? "-" : "", t / 10, t % 10);
    if (humidity != OOKDEC_NO_HUMIDITY && len > 0 && len < (int)sizeof(r->text))
        snprintf(r->text + len, sizeof(r->text) - len, " %u%%", humidity);
}


/* ---- Pulse distance (PPM) thermometers ----
 * A frame is nbits (pulse, gap) pairs, the gap giving the bit, then a last pulse and a long gap:
 *   _   __   _   ____   _   __   _                   _
 *  | |_|  |_| |_|    |_| |_|  |_| |__ ... ___________| |...
 *   p  0     p   1     p  0     p     end gap           next frame
 * */

typedef struct {
    uint16_t pulse_min, pulse_max;
    uint16_t zero_max;  /* Gap of a 0: [pulse_min, zero_max] */
    uint16_t one_max;  /* Gap of a 1: ]zero_max, one_max] */
    uint16_t end_min;  /* Gap at the end of a frame */
} ppm_t;

/* Nexus: 500µs pulses, 0 = 1000µs gap, 1 = 2000µs, sync/end gap 4000µs (rtl_433 nexus.c) */
static const ppm_t NEXUS = {200, 1000, 1350, 2900, 3000};
/* ThermoPRO TX-4, GT-WT02, inFactory: 500µs pulses, 0 = 2000µs gap, 1 = 4000µs, end gap 9000µs (16000µs for
 * inFactory) (rtl_433 thermopro_tx2.c, gt_wt_02.c, infactory.c) */
static const ppm_t PPM_2K4K = {200, 1000, 2700, 5500, 6000};

/* The frame of nbits ending at the gap g (odd index); false when a duration is not right */
static bool ppm_frame(const ppm_t *p, int g, int nbits, uint64_t *bits) {
    int s = g - 2 * nbits - 1;
    if (s < 0 || dur(g) < p->end_min || ! in(dur(g - 1), p->pulse_min, p->pulse_max))
        return false;
    uint64_t v = 0;
    for (int i = 0; i < nbits; ++i) {
        uint32_t pulse = w[s + 2 * i], gap = w[s + 2 * i + 1];
        if (! in(pulse, p->pulse_min, p->pulse_max) || gap < p->pulse_min || gap > p->one_max)
            return false;
        v = v << 1 | (gap > p->zero_max);
    }
    *bits = v;
    return true;
}

/* Nexus-TH, also FreeTec NC-7345, infactory NX-3980, Solight TE82S, TFA 30.3209 (rtl_433 nexus.c,
 * Flipper nexus_th.c). 36 bits sent 12 times: [id:8] [battery ok:1] [test:1] [channel-1:2] [temp °C*10:12 signed]
 * [1111] [humidity:8]. No checksum: the constant nibble is checked and the frame must be repeated. */
static bool dec_nexus(ookdec_result_t *r) {
    repeat_t rep = {0};
    for (int g = 1; g <= wn; g += 2) {
        uint64_t v;
        if (! ppm_frame(&NEXUS, g, 36, &v))
            continue;
        uint8_t id = v >> 28, flags = (v >> 24) & 0xF, hum = v & 0xFF;
        int16_t temp = (int16_t)((v >> 12 & 0xFFF) << 4) >> 4;  /* Sign extension of 12 bits */
        if ((v >> 8 & 0xF) != 0xF || (flags & 3) == 3 || (hum > 100) || v == 0xFFFFFFFFFull
            || temp < -500 || temp > 800)
            continue;
        if (! repeated(&rep, v))
            continue;
        set_weather(r, "Nexus-TH", "Nexus-TH", v, 36, (flags & 3) + 1, id, temp,
                    hum == 0 ? OOKDEC_NO_HUMIDITY : hum, ! (flags & 8));
        return true;
    }
    return false;
}

/* inFactory NC-3982-913, nor-tec 73383, DAY 73365 (rtl_433 infactory.c, Flipper infactory.c).
 * Each of the 6 packets: 4 x (1000µs pulse, 1000µs gap) preamble, 500µs pulse + 8000µs gap, 40 bits, 500µs pulse
 * + 16000µs gap. [id:8] [crc:4] [button:1] [battery low:1] [?:2] [temp °F*10+900:12] [humidity BCD:8] [?:2]
 * [channel:2]. CRC-4 poly 0x13 over the 4 first bytes, with the channel nibble in place of the CRC, XORed with the
 * first nibble of the last byte. */
static bool infactory_crc_ok(uint64_t v) {
    uint8_t b[5];
    for (int i = 0; i < 5; ++i)
        b[i] = v >> (32 - 8 * i);
    uint8_t crc_in = b[1] >> 4;
    b[1] = (b[1] & 0x0F) | (b[4] & 0x0F) << 4;
    return (crc4(b, 4, 0x13, 0) ^ (b[4] >> 4)) == crc_in;
}

static bool dec_infactory(ookdec_result_t *r) {
    for (int g = 1; g <= wn; g += 2) {
        uint64_t v;
        if (! ppm_frame(&PPM_2K4K, g, 40, &v))
            continue;
        uint8_t channel = v & 3, h10 = (v >> 8) & 0xF, h1 = (v >> 4) & 0xF;
        int f10 = (int)((v >> 12) & 0xFFF) - 900;
        if (channel == 0 || h1 > 9 || h10 * 10 + h1 > 100 || f10 < -400 || f10 > 1580 || ! infactory_crc_ok(v))
            continue;
        set_weather(r, "inFactory-TH", "inFactory", v, 40, channel, (uint16_t)(v >> 32),
                    div_round((f10 - 320) * 5, 9), h10 * 10 + h1, (v >> 26) & 1);
        return true;
    }
    return false;
}

/* GT-WT02 (ALDI Globaltronics) (rtl_433 gt_wt_02.c, Flipper gt_wt_02.c). 37 bits: [id:8] [battery low:1]
 * [button:1] [channel-1:2] [temp °C*10:12 signed] [humidity:7] [checksum:6]. Checksum: sum of the 8 nibbles of the
 * 31 first bits (completed with a 0), modulo 64. Humidity 10 means "LL" (below 20%), 110 "HH" (above 90%). */
static bool dec_gt_wt02(ookdec_result_t *r) {
    for (int g = 1; g <= wn; g += 2) {
        uint64_t v;
        if (! ppm_frame(&PPM_2K4K, g, 37, &v) || v == 0)
            continue;
        uint8_t channel = (v >> 25) & 3, hum = (v >> 6) & 0x7F;
        int16_t temp = (int16_t)((v >> 13 & 0xFFF) << 4) >> 4;
        if ((nibble_sum((v >> 6) << 1, 8) & 0x3F) != (v & 0x3F) || channel == 3 || temp < -200 || temp > 600
            || (hum != 10 && hum != 110 && (hum < 20 || hum > 90)))
            continue;
        set_weather(r, "GT-WT02", "GT-WT02", v, 37, channel + 1, (v >> 29) & 0xFF, temp,
                    hum == 10 ? 0 : hum == 110 ? 100 : hum, (v >> 28) & 1);
        return true;
    }
    return false;
}

/* ThermoPRO TX-4 / TX-2, Prologue (rtl_433 thermopro_tx2.c, Flipper thermopro_tx4.c). 36 bits and a trailing 0,
 * sent 6 times: [type:4 = 9 or 5] [id:8] [battery low:1] [button:1] [channel-1:2] [temp °C*10:12 signed]
 * [humidity:8, 0xCC = none] [0]. No checksum: the frame must be repeated. rtl_433 accepts the types 9 and 5, the
 * Flipper 9 and 6 (0b0110): all three are accepted. */
static bool dec_thermopro_tx4(ookdec_result_t *r) {
    repeat_t rep = {0};
    for (int g = 1; g <= wn; g += 2) {
        uint64_t v;
        int nbits = 37;
        if (! ppm_frame(&PPM_2K4K, g, 37, &v)) {
            nbits = 36;  /* Some sensors don't send the trailing bit */
            if (! ppm_frame(&PPM_2K4K, g, 36, &v))
                continue;
            v <<= 1;
        }
        uint8_t type = v >> 33, hum = (v >> 1) & 0xFF;
        int16_t temp = (int16_t)((v >> 9 & 0xFFF) << 4) >> 4;
        if ((type != 9 && type != 5 && type != 6) || (v & 1) || (hum > 100 && hum != 0xCC)
            || temp < -400 || temp > 800)
            continue;
        if (! repeated(&rep, v))
            continue;
        set_weather(r, "ThermoPRO-TX4", "ThermoPro", v, nbits, ((v >> 21) & 3) + 1, (v >> 25) & 0xFF, temp,
                    hum == 0xCC ? OOKDEC_NO_HUMIDITY : hum, (v >> 24) & 1);
        return true;
    }
    return false;
}


/* ---- Pulse width (PWM) thermometers ----
 * A preamble of long pulses and gaps, then the bits: a long pulse and a short gap for 1, a short pulse and a long gap
 * for 0, with a constant period. */

typedef struct {
    uint16_t pre_min, pre_max;  /* Pulse and gap of the preamble */
    uint16_t pre_sum_min, pre_sum_max;  /* Pulse + gap of the preamble */
    uint8_t pre_count;  /* Minimum number of (pulse, gap) of the preamble */
    uint16_t period_min, period_max;  /* Pulse + gap of a bit */
    uint16_t one_min;  /* The last bit (its gap may be merged with the next silence) is 1 when its pulse is longer */
} pwm_t;

/* LaCrosse: preamble 4 x (833µs, 833µs), 1 = (417µs, 208µs), 0 = (208µs, 417µs) */
static const pwm_t LACROSSE = {580, 1150, 1300, 2200, 3, 420, 850, 312};
/* Acurite: preamble 4 x (620µs, 596µs), 1 = (408µs, 204µs), 0 = (220µs, 392µs), 2192µs between the packets */
static const pwm_t ACURITE = {400, 850, 850, 1500, 3, 420, 850, 300};

/* Decodes nbits after the preamble at the index i (even). \return the index of the next preamble to try */
static int pwm_frame(const pwm_t *p, int i, int nbits, uint8_t *bytes, bool *ok) {
    *ok = false;
    int k = 0;
    while (in(dur(i + 2 * k), p->pre_min, p->pre_max) && in(dur(i + 2 * k + 1), p->pre_min, p->pre_max)
           && in(dur(i + 2 * k) + dur(i + 2 * k + 1), p->pre_sum_min, p->pre_sum_max))
        ++k;
    if (k < p->pre_count)
        return i + 2;
    int j = i + 2 * k;
    memset(bytes, 0, (nbits + 7) / 8);
    for (int b = 0; b < nbits; ++b) {
        uint32_t pulse = dur(j + 2 * b), gap = dur(j + 2 * b + 1);
        if (pulse < 100 || pulse > 800)
            return j;
        bool one;
        if (b < nbits - 1) {
            if (gap < 60 || ! in(pulse + gap, p->period_min, p->period_max))
                return j;
            one = pulse > gap;
        } else
            one = pulse >= p->one_min;
        if (one)
            bytes[b / 8] |= 0x80 >> (b % 8);
    }
    *ok = true;
    return j + 2 * nbits;
}

/* LaCrosse TX141TH-Bv2, TFA 30.3221.02... (rtl_433 lacrosse_tx141x.c, Flipper lacrosse_tx141thbv2.c).
 * 12 packets of 4 x (833µs, 833µs) and 40 bits: [id:8] [battery low:1] [test:1] [channel-1:2]
 * [temp °C*10+500:12] [humidity:8] [digest:8]. The digest is lfsr_digest8_reflect(4 bytes, 0x31, 0xF4).
 * The TX141TH-Bv3 sends 41 bits: the extra bit is ignored. */
static bool dec_lacrosse(ookdec_result_t *r) {
    uint8_t b[5];
    for (int i = 0; i < wn;) {
        bool ok;
        i = pwm_frame(&LACROSSE, i, 40, b, &ok);
        if (! ok || lfsr_digest8_reflect(b, 4, 0x31, 0xF4) != b[4] || (b[0] | b[1] | b[2] | b[3]) == 0)
            continue;
        int temp = (((b[1] & 0x0F) << 8) | b[2]) - 500;
        if (b[3] > 100 || temp < -400 || temp > 700)  /* Specified from -40 to 60°C */
            continue;
        uint64_t v = 0;
        for (int k = 0; k < 5; ++k)
            v = v << 8 | b[k];
        set_weather(r, "LaCrosse_TX141THBv2", "LaCrosse", v, 40, ((b[1] >> 4) & 3) + 1, b[0], temp, b[3],
                    b[1] >> 7);
        return true;
    }
    return false;
}

/* Acurite 592TXR, 06002RM, 6044m tower sensors (rtl_433 acurite.c, Flipper acurite_592txr.c).
 * 3 packets of 4 x (620µs, 596µs) and 56 bits (7 bytes), p = even parity of the byte:
 * [channel:2 (11 = A, 10 = B, 00 = C)] [id:14] [p] [battery ok:1] [message type:6 = 0x04] [p] [humidity:7]
 * [p] [?:2] [temp high:5] [p] [temp low:7] [sum of the 6 bytes]. Temperature: °C*10 + 1000. */
static bool dec_acurite_592txr(ookdec_result_t *r) {
    uint8_t b[7];
    for (int i = 0; i < wn;) {
        bool ok;
        i = pwm_frame(&ACURITE, i, 56, b, &ok);
        if (! ok)
            continue;
        uint8_t sum = 0, parity = 0;
        for (int k = 0; k < 6; ++k)
            sum += b[k];
        for (int k = 2; k < 6; ++k)
            parity ^= parity8(b[k]);  /* Like rtl_433: the parity of the 4 bytes together */
        if (sum != b[6] || parity || (b[2] & 0x3F) != 0x04 || (b[0] >> 6) == 1)
            continue;
        int temp = (((b[4] & 0x1F) << 7) | (b[5] & 0x7F)) - 1000;
        uint8_t hum = b[3] & 0x7F;
        if (temp < -400 || temp > 700 || (hum > 100 && hum != 127))
            continue;
        static const uint8_t channels[4] = {3, 0, 2, 1};  /* C, invalid, B, A: A = 1 */
        uint64_t v = 0;
        for (int k = 0; k < 7; ++k)
            v = v << 8 | b[k];
        set_weather(r, "Acurite_592TXR", "Acurite", v, 56, channels[b[0] >> 6], ((b[0] & 0x3F) << 8) | b[1], temp,
                    hum == 127 ? OOKDEC_NO_HUMIDITY : hum, ! (b[2] & 0x40));
        return true;
    }
    return false;
}


/* ---- Remotes: the elementary duration (te) varies from one remote to the other, it is measured ---- */

/* Sorts n durations from w[s] in short and long, around the threshold, and checks that each class is regular.
 * \return false when the classes are not clean */
static bool two_classes(int s, int n, uint32_t threshold, int want_short, uint32_t *short_mean, uint32_t *long_mean) {
    uint32_t ss = 0, sl = 0;
    int ns = 0;
    for (int i = 0; i < n; ++i) {
        if (w[s + i] < threshold) {
            ss += w[s + i];
            ++ns;
        } else
            sl += w[s + i];
    }
    if (ns != want_short || ns == n)
        return false;
    *short_mean = ss / ns;
    *long_mean = sl / (n - ns);
    for (int i = 0; i < n; ++i) {
        uint32_t d = w[s + i];
        if (d < threshold ? ! in(d, *short_mean / 2, *short_mean * 16 / 10)
                          : ! in(d, *long_mean * 7 / 10, *long_mean * 13 / 10))
            return false;
    }
    return true;
}

/* Princeton: PT2262, EV1527, HS1527, SC5262... (Flipper princeton.c, https://phreakerclub.com/447).
 * 24 bits, MSB first: 1 = (3te pulse, te gap), 0 = (te pulse, 3te gap), then a te stop pulse and a guard gap of
 * ~30te. te is 100 to 700µs (400µs for the Flipper). EV1527: 20 bits of ID and 4 bits of buttons. PT2262: 12
 * trits (00 = 0, 11 = 1, 01 = floating), seen here as 24 bits. No checksum: the same code must be received twice
 * (like the Flipper decoder, which compares with its last_data).
 * The Flipper sends it from Sub-GHz > Add Manually > Princeton_433: random 20 bits key, button 0x4 in the low
 * nibble, te = 400µs, AM650 preset; the code can then be edited in the saved file ("Key: 00 00 00 00 00 12 34 54"). */
static uint32_t trusted_value = 0, trusted_mask = 0;

void ookdec_trust(uint32_t value, uint32_t mask) {
    trusted_value = value;
    trusted_mask = mask;
}

static bool dec_princeton(ookdec_result_t *r) {
    repeat_t rep = {0};
    for (int g = 49; g <= wn; g += 2) {
        int s = g - 49;
        if (dur(g) < 1000)
            continue;
        uint32_t sum = 0;
        for (int i = 0; i < 49; ++i)
            sum += w[s + i];
        uint32_t te = sum / 97, ms, ml;  /* 24 x 4te + te */
        if (te < 80 || te > 900 || ! two_classes(s, 49, 2 * te, 25, &ms, &ml))
            continue;
        /* The ratio long / short is 3 (CAME and Nice FLO: 2), less with a receiver bias */
        if (ml * 10 < ms * 21 || ml * 10 > ms * 45 || dur(g) < 8 * ms || w[g - 1] >= 2 * te)
            continue;
        uint32_t code = 0;
        bool ok = true;
        for (int i = 0; i < 24 && ok; ++i) {
            uint32_t pulse = w[s + 2 * i], gap = w[s + 2 * i + 1];
            bool pulse_long = pulse >= 2 * te, gap_long = gap >= 2 * te;
            ok = pulse_long != gap_long && in(pulse + gap, te * 4 * 3 / 4, te * 4 * 5 / 4);  /* Period 4te */
            code = code << 1 | pulse_long;
        }
        if (! ok)
            continue;
        /* Seen twice, or once for the trusted codes (a single frame of a short press, see ookdec_trust()) */
        bool twice = repeated(&rep, code);
        if (! twice && ! (trusted_mask && (code & trusted_mask) == trusted_value))
            continue;
        set_name(r, "Princeton", code, 24);
        r->te = ms;
        snprintf(r->text, sizeof(r->text), "Princeton 0x%06lX %uus", (unsigned long)code, (unsigned)ms);
        return true;
    }
    return false;
}

/* CAME and Nice FLO (Flipper came.c and nice_flo.c, https://phreakerclub.com/447): same coding, different te.
 * Guard gap, te start pulse, then for each bit (MSB first) a gap and a pulse: 1 = (2te gap, te pulse),
 * 0 = (te gap, 2te pulse). CAME: te = 320µs, 12 or 24 bits, guard 47te (12 bits) or 76te (24 bits).
 * Nice FLO: te = 700µs, 12 or 24 bits, guard 36te. No checksum: the frame must be repeated. */
static bool dec_came_nice(ookdec_result_t *r) {
    static const uint8_t sizes[2] = {12, 24};
    for (int k = 0; k < 2; ++k) {
        int nbits = sizes[k], n = 2 * nbits + 1;
        repeat_t rep = {0};
        for (int g = n; g <= wn; g += 2) {
            int s = g - n;
            if (dur(g) < 1000 || (s > 0 && dur(s - 1) < 1000))
                continue;
            uint32_t sum = 0;
            for (int i = 0; i < n; ++i)
                sum += w[s + i];
            uint32_t te = sum / (3 * nbits + 1), ms, ml;
            if (te < 150 || te > 1100 || ! two_classes(s, n, te * 3 / 2, nbits + 1, &ms, &ml))
                continue;
            if (ml * 10 < ms * 13 || ml * 10 > ms * 26 || dur(g) < 6 * ms || (s > 0 && dur(s - 1) < 6 * ms)
                || w[s] >= te * 3 / 2)
                continue;
            uint32_t code = 0;
            bool ok = true;
            for (int i = 0; i < nbits && ok; ++i) {
                uint32_t gap = w[s + 1 + 2 * i], pulse = w[s + 2 + 2 * i];
                bool gap_long = gap >= te * 3 / 2, pulse_long = pulse >= te * 3 / 2;
                ok = pulse_long != gap_long && in(pulse + gap, te * 3 * 3 / 4, te * 3 * 5 / 4);  /* Period 3te */
                code = code << 1 | gap_long;
            }
            if (! ok || ! repeated(&rep, code))
                continue;
            const char *name = ms < 500 ? "CAME" : "Nice FLO";
            set_name(r, name, code, nbits);
            r->te = ms;
            snprintf(r->text, sizeof(r->text), "%s %db 0x%0*lX %uus", name, nbits, nbits / 4, (unsigned long)code,
                     (unsigned)ms);
            return true;
        }
    }
    return false;
}


/* ---- All ---- */

bool ookdec_decode(const ookdec_signal_t *s, ookdec_result_t *r) {
    deglitch(s);
    if (wn < 20)
        return false;
    /* The protocols with a checksum first, the strongest first: their decoding is sure */
    return dec_acurite_592txr(r) || dec_lacrosse(r) || dec_infactory(r) || dec_gt_wt02(r)
           || dec_nexus(r) || dec_thermopro_tx4(r) || dec_princeton(r) || dec_came_nice(r);
}
