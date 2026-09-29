/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Host tests of the 433MHz OOK decoders: ideal frames of each protocol (same timings as tools/ook_sub.py) with
 * noise before, jitter, receiver bias and glitches; wrong checksums, single frames without checksum and noise
 * must decode to nothing. */

#include "pico_host.h"
#include "../../radio/ookdec.c"
#include "test.h"

static ookdec_signal_t sig;
static ookdec_result_t res;
static uint32_t seed = 1;
static int jitter = 0;  /* ± % applied to each duration */
static int bias = 0;  /* µs added to the pulses and removed from the gaps (receivers stretch the pulses) */
static int scale = 100;  /* % applied to all the durations (transmitter clock) */

static uint32_t rnd(void) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

static void start(void) {
    sig.n = 0;
}

/* Appends a duration (a pulse when sig.n is even) */
static void put(uint32_t d) {
    int32_t v = (int32_t)(d * scale / 100);
    if (jitter)
        v = v * (100 - jitter + (int32_t)(rnd() % (2 * jitter + 1))) / 100;
    v += sig.n & 1 ? -bias : bias;
    if (v < 1)
        v = 1;
    if (v > 0xFFFF)
        v = 0xFFFF;
    if (sig.n < OOKDEC_MAX_PULSES)
        sig.us[sig.n++] = v;
}

static void pair(uint32_t pulse, uint32_t gap) {
    put(pulse);
    put(gap);
}

/* Random durations (pulse, gap pairs, so that the next one is a pulse), like the CC1101 output without signal */
static void noise(int pairs) {
    for (int i = 0; i < pairs && sig.n + 2 <= OOKDEC_MAX_PULSES; ++i) {
        sig.us[sig.n++] = 20 + rnd() % 3000;
        sig.us[sig.n++] = 20 + rnd() % 3000;
    }
}

static bool decode(void) {
    memset(&res, 0x55, sizeof(res));
    return ookdec_decode(&sig, &res);
}


/* ---- Encoders ---- */

static void princeton(uint32_t code, uint32_t te, int repeats) {
    for (int r = 0; r < repeats; ++r) {
        for (int i = 23; i >= 0; --i)
            if ((code >> i) & 1)
                pair(3 * te, te);
            else
                pair(te, 3 * te);
        pair(te, 30 * te);
    }
}

/* CAME / Nice FLO: start pulse, then (gap, pulse) per bit, guard gap */
static void came(uint32_t code, int nbits, uint32_t te, uint32_t guard_te, int repeats) {
    for (int r = 0; r < repeats; ++r) {
        put(te);
        for (int i = nbits - 1; i >= 0; --i)
            if ((code >> i) & 1)
                pair(2 * te, te);
            else
                pair(te, 2 * te);
        put(guard_te * te);
    }
}

/* Pulse distance frame: (500, gap) per bit, then (500, end) */
static void ppm(uint64_t v, int nbits, uint32_t zero, uint32_t one, uint32_t end) {
    for (int i = nbits - 1; i >= 0; --i)
        pair(500, (v >> i) & 1 ? one : zero);
    pair(500, end);
}

static uint64_t nexus_bits(uint8_t id, bool battery_ok, uint8_t channel, int temp_c10, uint8_t hum) {
    return (uint64_t)id << 28 | (uint64_t)(battery_ok << 3 | (channel - 1)) << 24 | (uint64_t)(temp_c10 & 0xFFF) << 12
           | 0xF << 8 | hum;
}

static void nexus(uint64_t v, int repeats) {
    pair(500, 4000);  /* Sync before the first frame */
    for (int r = 0; r < repeats; ++r)
        ppm(v, 36, 1000, 2000, 4000);
}

static uint64_t tx4_bits(uint8_t id, bool battery_low, uint8_t channel, int temp_c10, uint8_t hum) {
    return (uint64_t)9 << 33 | (uint64_t)id << 25 | (uint64_t)battery_low << 24 | (uint64_t)(channel - 1) << 21
           | (uint64_t)(temp_c10 & 0xFFF) << 9 | (uint64_t)hum << 1;
}

static uint64_t gt_bits(uint8_t id, bool battery_low, uint8_t channel, int temp_c10, uint8_t hum) {
    uint64_t v = (uint64_t)id << 23 | (uint64_t)battery_low << 22 | (uint64_t)(channel - 1) << 19
                 | (uint64_t)(temp_c10 & 0xFFF) << 7 | hum;
    return v << 6 | (nibble_sum(v << 1, 8) & 0x3F);
}

/* ThermoPRO TX-4 and GT-WT02: sync, then 6 frames of 37 bits */
static void ppm37(uint64_t v, int repeats) {
    pair(500, 9000);
    for (int r = 0; r < repeats; ++r)
        ppm(v, 37, 2000, 4000, 9000);
}

static uint64_t infactory_bits(uint8_t id, bool battery_low, uint8_t channel, int temp_c10, uint8_t hum) {
    int f10 = div_round(temp_c10 * 9, 5) + 320 + 900;
    uint8_t b[5] = {id, battery_low << 2, f10 >> 4, (f10 & 0xF) << 4 | hum / 10, (hum % 10) << 4 | channel};
    uint8_t m[5];
    memcpy(m, b, 5);
    m[1] = (m[1] & 0x0F) | (m[4] & 0x0F) << 4;
    b[1] |= (crc4(m, 4, 0x13, 0) ^ (b[4] >> 4)) << 4;
    uint64_t v = 0;
    for (int i = 0; i < 5; ++i)
        v = v << 8 | b[i];
    return v;
}

static void infactory(uint64_t v, int repeats) {
    for (int r = 0; r < repeats; ++r) {
        for (int i = 0; i < 4; ++i)
            pair(1000, 1000);
        pair(500, 8000);
        ppm(v, 40, 2000, 4000, 16000);
    }
}

/* PWM bits: 1 = (p1, g1), 0 = (p0, g0), the gap of the last bit replaced by last_gap when not 0 */
static void pwm(const uint8_t *b, int nbits, uint32_t p1, uint32_t g1, uint32_t p0, uint32_t g0, uint32_t last_gap) {
    for (int i = 0; i < nbits; ++i) {
        bool bit = (b[i / 8] >> (7 - i % 8)) & 1;
        put(bit ? p1 : p0);
        put(i == nbits - 1 && last_gap ? last_gap : bit ? g1 : g0);
    }
}

static void lacrosse_bytes(uint8_t *b, uint8_t id, bool battery_low, uint8_t channel, int temp_c10, uint8_t hum) {
    int t = temp_c10 + 500;
    b[0] = id;
    b[1] = battery_low << 7 | (channel - 1) << 4 | t >> 8;
    b[2] = t;
    b[3] = hum;
    b[4] = lfsr_digest8_reflect(b, 4, 0x31, 0xF4);
}

static void lacrosse(const uint8_t *b, int repeats) {
    for (int r = 0; r < repeats; ++r) {
        for (int i = 0; i < 4; ++i)
            pair(833, 833);
        pwm(b, 40, 417, 208, 208, 417, 0);
    }
    pair(833, 833);  /* Postamble */
    pair(833, 833);
}

static uint8_t even(uint8_t b) {
    return b | parity8(b) << 7;
}

static void acurite_bytes(uint8_t *b, uint16_t id, bool battery_low, uint8_t channel, int temp_c10, uint8_t hum) {
    static const uint8_t raw_channel[4] = {0, 3, 2, 0};  /* A = 1 = 11, B = 10, C = 00 */
    int t = temp_c10 + 1000;
    b[0] = raw_channel[channel] << 6 | (id >> 8 & 0x3F);
    b[1] = id;
    b[2] = even(! battery_low << 6 | 0x04);
    b[3] = even(hum);
    b[4] = even(t >> 7 & 0x1F);
    b[5] = even(t & 0x7F);
    b[6] = 0;
    for (int i = 0; i < 6; ++i)
        b[6] += b[i];
}

static void acurite(const uint8_t *b, int repeats) {
    for (int r = 0; r < repeats; ++r) {
        for (int i = 0; i < 4; ++i)
            pair(620, 596);
        pwm(b, 56, 408, 204, 220, 392, 2192);
    }
}


/* ---- Checks ---- */

static void check_weather(const char *protocol, uint8_t channel, uint16_t id, int temp, uint8_t hum, bool low,
                          const char *text) {
    CHECK(decode());
    CHECK_STR(res.protocol, protocol);
    CHECK(res.weather);
    CHECK_EQ(res.channel, channel);
    CHECK_EQ(res.id, id);
    CHECK_EQ(res.temp_c10, temp);
    CHECK_EQ(res.humidity, hum);
    CHECK_EQ(res.battery_low, low);
    if (text)
        CHECK_STR(res.text, text);
}

/* Each protocol with random values and conditions */
static void random_rounds(int rounds) {
    int fails = 0, ambiguous = 0;
    for (int k = 0; k < rounds; ++k) {
        int temp = (int)(rnd() % 600) - 200, ch = 1 + rnd() % 3, hum = 20 + rnd() % 70;
        uint8_t id = rnd(), b[7];
        bool low = rnd() & 1;
        jitter = rnd() % 16;
        bias = (int)(rnd() % 61) - 30;
        uint32_t te = 250 + rnd() % 300, code = rnd() & 0xFFFFFF;
        for (int p = 0; p < 8; ++p) {
            start();
            noise(rnd() % 20);
            switch (p) {
            case 0: nexus(nexus_bits(id, ! low, ch, temp, hum), 12); break;
            case 1: ppm37(tx4_bits(id, low, ch, temp, hum), 6); break;
            case 2: ppm37(gt_bits(id, low, ch, temp, hum), 6); break;
            case 3: infactory(infactory_bits(id, low, ch, temp, hum), 6); break;
            case 4: lacrosse_bytes(b, id, low, ch, temp, hum); lacrosse(b, 12); break;
            case 5: acurite_bytes(b, id | (rnd() & 0x3F) << 8, low, ch, temp, hum); acurite(b, 3); break;
            case 6: princeton(code, te, 6); break;
            case 7: came(code & 0xFFF, 12, 320, 47, 6); break;
            }
            noise(rnd() % 5);
            bool ok = decode();
            uint64_t v = tx4_bits(id, low, ch, temp, hum);
            if (p == 1 && ok && ! strcmp(res.protocol, "GT-WT02") && (nibble_sum((v >> 6) << 1, 8) & 0x3F) == (v & 0x3F)) {
                ++ambiguous;  /* Same timings and size, the ThermoPRO frame happens to have a right GT-WT02 checksum */
                continue;
            }
            if (p < 6)
                ok = ok && res.weather && res.channel == ch && res.humidity == hum && res.battery_low == low
                     && (res.temp_c10 == temp || (p == 3 && (res.temp_c10 - temp) * (res.temp_c10 - temp) <= 1));
            else
                ok = ok && res.code == (p == 6 ? code : (code & 0xFFF));
            if (! ok) {
                ++fails;
                printf("random round %d protocol %d: temp %d ch %d hum %d jitter %d bias %d -> %s\n", k, p, temp,
                       ch, hum, jitter, bias, res.text);
            }
        }
    }
    CHECK_EQ(fails, 0);
    CHECK(ambiguous < rounds / 30);
    printf("%d random rounds, %d ThermoPRO-TX4 frames taken for GT-WT02 frames\n", rounds, ambiguous);
    jitter = bias = 0;
}

int main(void) {
    uint8_t b[7];

    /* ---- Remotes ---- */

    /* Princeton, as sent by the Flipper (Add Manually > Princeton_433: te = 400µs) */
    start();
    princeton(0x123454, 400, 4);
    CHECK(decode());
    CHECK_STR(res.protocol, "Princeton");
    CHECK_EQ(res.code, 0x123454);
    CHECK_EQ(res.bits, 24);
    CHECK(! res.weather);
    CHECK(res.te >= 390 && res.te <= 410);
    CHECK_STR(res.text, "Princeton 0x123454 400us");

    /* Other te, jitter, receiver bias, noise before */
    seed = 7;
    jitter = 15;
    bias = 40;
    start();
    noise(15);
    princeton(0xA5F00F, 250, 5);
    CHECK(decode());
    CHECK_EQ(res.code, 0xA5F00F);
    start();
    noise(15);
    princeton(0x000001, 150, 5);
    CHECK(decode());
    CHECK_EQ(res.code, 0x000001);
    jitter = bias = 0;

    /* A single frame is not enough (no checksum), two different ones neither */
    start();
    noise(10);
    princeton(0x123454, 400, 1);
    CHECK(! decode());
    start();
    princeton(0x123454, 400, 1);
    princeton(0x123458, 400, 1);
    CHECK(! decode());

    /* Glitch: a pulse cut by a 30µs gap, a gap cut by a 20µs pulse */
    start();
    princeton(0x55AA11, 350, 3);
    memmove(&sig.us[12], &sig.us[10], (sig.n - 10) * sizeof(uint16_t));
    sig.us[10] = 500;  /* The 1050µs pulse of bit 5 (a 1) becomes 500 + 30 + 520 */
    sig.us[11] = 30;
    sig.us[12] = 520;
    sig.n += 2;
    memmove(&sig.us[62], &sig.us[60], (sig.n - 60) * sizeof(uint16_t));
    sig.us[60] = 100;  /* The 1050µs gap of bit 6 of the 2nd frame (a 0): 100 + 20 + 930 */
    sig.us[61] = 20;
    sig.us[62] = 930;
    sig.n += 2;
    CHECK(decode());
    CHECK_EQ(res.code, 0x55AA11);

    /* CAME 12 bits (te 320µs, guard 47te), 24 bits (guard 76te) */
    start();
    came(0x5A1, 12, 320, 47, 4);
    CHECK(decode());
    CHECK_STR(res.protocol, "CAME");
    CHECK_EQ(res.code, 0x5A1);
    CHECK_EQ(res.bits, 12);
    CHECK_STR(res.text, "CAME 12b 0x5A1 320us");
    start();
    noise(10);
    jitter = 15;
    bias = 30;
    came(0xABCDE4, 24, 320, 76, 4);
    CHECK(decode());
    CHECK_STR(res.protocol, "CAME");
    CHECK_EQ(res.code, 0xABCDE4);
    CHECK_EQ(res.bits, 24);

    /* Nice FLO 12 bits (te 700µs, guard 36te) */
    start();
    noise(10);
    came(0x3F1, 12, 700, 36, 4);
    CHECK(decode());
    CHECK_STR(res.protocol, "Nice FLO");
    CHECK_EQ(res.code, 0x3F1);
    jitter = bias = 0;
    start();
    came(0x3F1, 12, 700, 36, 1);
    CHECK(! decode());

    /* ---- Thermometers ---- */

    /* Nexus-TH: ch2, 21.3°C, 45%, battery OK */
    start();
    nexus(nexus_bits(0x5A, true, 2, 213, 45), 12);
    check_weather("Nexus-TH", 2, 0x5A, 213, 45, false, "Nexus-TH ch2 21.3°C 45%");
    CHECK_EQ(res.bits, 36);
    /* Negative, battery low, jitter, noise; temperature only (humidity 0) */
    jitter = 15;
    bias = -30;
    start();
    noise(12);
    nexus(nexus_bits(0xC3, false, 3, -57, 0), 12);
    check_weather("Nexus-TH", 3, 0xC3, -57, OOKDEC_NO_HUMIDITY, true, "Nexus-TH ch3 -5.7°C");
    jitter = bias = 0;
    /* No checksum: a single frame is not enough; the constant nibble is checked */
    start();
    nexus(nexus_bits(0x5A, true, 2, 213, 45), 1);
    CHECK(! decode());
    start();
    nexus(nexus_bits(0x5A, true, 2, 213, 45) ^ 0x100, 12);
    CHECK(! decode());

    /* ThermoPRO TX-4 */
    start();
    ppm37(tx4_bits(0x81, false, 1, 237, 52), 6);
    check_weather("ThermoPRO-TX4", 1, 0x81, 237, 52, false, "ThermoPro ch1 23.7°C 52%");
    start();
    noise(8);
    jitter = 15;
    bias = 40;
    ppm37(tx4_bits(0x12, true, 3, -121, 0xCC), 6);  /* 0xCC: no humidity sensor */
    check_weather("ThermoPRO-TX4", 3, 0x12, -121, OOKDEC_NO_HUMIDITY, true, "ThermoPro ch3 -12.1°C");
    jitter = bias = 0;
    start();
    ppm37(tx4_bits(0x81, false, 1, 237, 52), 1);
    CHECK(! decode());

    /* GT-WT02: the examples of rtl_433 gt_wt_02.c: {37} 34 00 ed 47 60 = ch1 23.7°C 35%, {37} 34 8f 87 15 90 */
    CHECK_EQ(gt_bits(0x34, false, 1, 237, 35), 0x3400ed4760ull >> 3);
    CHECK_EQ(gt_bits(0x34, true, 1, -121, 10), 0x348f871590ull >> 3);
    start();
    ppm37(0x3400ed4760ull >> 3, 6);
    check_weather("GT-WT02", 1, 0x34, 237, 35, false, "GT-WT02 ch1 23.7°C 35%");
    start();
    noise(10);
    jitter = 15;
    bias = 30;
    ppm37(0x348f871590ull >> 3, 6);
    check_weather("GT-WT02", 1, 0x34, -121, 0, true, NULL);  /* "LL": below 20% */
    jitter = bias = 0;
    /* Wrong checksum: rejected (and not a ThermoPRO either: its type nibble is not 9, 5 or 6) */
    start();
    ppm37((0x3400ed4760ull >> 3) ^ 1, 6);
    CHECK(! decode());

    /* inFactory: 21.5°C = 70.7°F (raw 707 + 900), ch2, 45% */
    CHECK(infactory_crc_ok(infactory_bits(0x0F, false, 2, 215, 45)));
    start();
    infactory(infactory_bits(0x0F, false, 2, 215, 45), 6);
    check_weather("inFactory-TH", 2, 0x0F, 215, 45, false, "inFactory ch2 21.5°C 45%");
    CHECK_EQ((res.code >> 12) & 0xFFF, 707 + 900);
    start();
    noise(20);
    jitter = 15;
    bias = 40;
    infactory(infactory_bits(0xA7, true, 3, -100, 100), 6);  /* -10°C = 14°F */
    check_weather("inFactory-TH", 3, 0xA7, -100, 100, true, NULL);
    jitter = bias = 0;
    /* A single frame is enough (CRC), a wrong CRC is not */
    start();
    infactory(infactory_bits(0x0F, false, 2, 215, 45), 1);
    CHECK(decode());
    start();
    infactory(infactory_bits(0x0F, false, 2, 215, 45) ^ (1ull << 29), 6);
    CHECK(! decode());

    /* LaCrosse TX141TH-Bv2 */
    lacrosse_bytes(b, 0x9B, false, 1, 198, 61);
    start();
    lacrosse(b, 12);
    check_weather("LaCrosse_TX141THBv2", 1, 0x9B, 198, 61, false, "LaCrosse ch1 19.8°C 61%");
    lacrosse_bytes(b, 0x21, true, 2, -85, 90);
    start();
    noise(20);
    jitter = 15;
    bias = 30;
    lacrosse(b, 4);
    check_weather("LaCrosse_TX141THBv2", 2, 0x21, -85, 90, true, NULL);
    jitter = bias = 0;
    b[4] ^= 0x10;  /* Wrong digest */
    start();
    lacrosse(b, 12);
    CHECK(! decode());

    /* Acurite 592TXR: channel A, 14 bits ID */
    acurite_bytes(b, 0x2ABC, false, 1, 215, 45);
    start();
    acurite(b, 3);
    check_weather("Acurite_592TXR", 1, 0x2ABC, 215, 45, false, "Acurite chA 21.5°C 45%");
    acurite_bytes(b, 0x0123, true, 3, -155, 18);
    start();
    noise(20);
    jitter = 15;
    bias = -30;
    acurite(b, 3);
    check_weather("Acurite_592TXR", 3, 0x0123, -155, 18, true, "Acurite chC -15.5°C 18%");
    jitter = bias = 0;
    b[6] ^= 1;  /* Wrong checksum */
    start();
    acurite(b, 3);
    CHECK(! decode());
    acurite_bytes(b, 0x0123, true, 3, -155, 18);
    b[3] ^= 0x80;  /* Wrong parity (the checksum fixed) */
    b[6] += 0x80;
    start();
    acurite(b, 3);
    CHECK(! decode());

    /* Transmitter clock ±20% (the fixed timings of the thermometers) */
    for (scale = 80; scale <= 120; scale += 40) {
        start();
        nexus(nexus_bits(0x5A, true, 2, 213, 45), 12);
        CHECK(decode() && ! strcmp(res.protocol, "Nexus-TH"));
        start();
        ppm37(tx4_bits(0x81, false, 1, 237, 52), 6);
        CHECK(decode() && ! strcmp(res.protocol, "ThermoPRO-TX4"));
        start();
        infactory(infactory_bits(0x0F, false, 2, 215, 45), 6);
        CHECK(decode() && ! strcmp(res.protocol, "inFactory-TH"));
        lacrosse_bytes(b, 0x9B, false, 1, 198, 61);
        start();
        lacrosse(b, 12);
        CHECK(decode() && ! strcmp(res.protocol, "LaCrosse_TX141THBv2"));
        acurite_bytes(b, 0x2ABC, false, 1, 215, 45);
        start();
        acurite(b, 3);
        CHECK(decode() && ! strcmp(res.protocol, "Acurite_592TXR"));
    }
    scale = 100;

    /* ---- Noise decodes to nothing ---- */
    int false_positives = 0;
    for (int k = 0; k < 2000; ++k) {
        start();
        noise(OOKDEC_MAX_PULSES / 2);
        if (decode()) {
            ++false_positives;
            printf("noise %d decoded as %s\n", k, res.text);
        }
    }
    CHECK_EQ(false_positives, 0);
    start();
    CHECK(! decode());
    sig.n = 1;
    sig.us[0] = 500;
    CHECK(! decode());

    /* ---- Many random values and conditions ---- */
    seed = 12345;
    random_rounds(300);

    TEST_END();
}
