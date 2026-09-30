/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Host tests of the vCards of the contact cards (menu/vcard.c): packets of the chat of the Flipper, folding,
 * check line, cards typed on a Flipper */

#include <string.h>

#include "test.h"
#include "vcard.h"

static const char *CARD[VCARD_FIELDS] = {
    "Tristan", "SALAUN", "0612345678", "monemail@poste.fr", "Hack In Provence", "Dev",
    "12 avenue des Calanques du Bec de l'Aigle", "La Ciotat", "https://www.linkedin.com/in/tristansalaun",
    "https://github.com/Tristus1er", "https://hackinprovence.fr", "@tristan@mastodon.social",
    "Rencontre a SecSea 2026, parler du badge et des cigales",
};

static bool feed(vcard_rx_t *rx, const vcard_packet_t *p, int n, int skip) {
    bool done = false;
    for (int i = 0; i < n; ++i)
        if (i != skip)
            done |= vcard_rx_packet(rx, p[i].text, p[i].len);
    return done;
}

int main(void) {
    static vcard_packet_t p[VCARD_PACKETS_MAX];
    static vcard_rx_t rx;

    /* Every field: every packet fits the chat, the long lines are folded, the card comes back the same */
    int n = vcard_build(CARD, 0x1FFF, p, VCARD_PACKETS_MAX);
    CHECK(n > 15);
    for (int i = 0; i < n; ++i) {
        CHECK(p[i].len <= VCARD_PACKET_MAX && p[i].len == strlen(p[i].text));
        CHECK(p[i].len >= 2 && p[i].text[p[i].len - 2] == '\r' && p[i].text[p[i].len - 1] == '\n');
    }
    CHECK_STR(p[0].text, "BEGIN:VCARD\r\n");
    CHECK_STR(p[n - 1].text, "END:VCARD\r\n");
    CHECK(! strncmp(p[n - 2].text, "X-SECSEA-CHECK:", 15));
    int folded = 0;
    for (int i = 0; i < n; ++i)
        folded += p[i].text[0] == ' ';
    CHECK(folded >= 1);  /* The address, the LinkedIn URL are longer than a packet */
    vcard_rx_init(&rx);
    CHECK(feed(&rx, p, n, -1));
    for (int f = 0; f < VCARD_FIELDS; ++f)
        CHECK_STR(rx.values[f], CARD[f]);
    uint16_t crc = rx.crc;

    /* Only the fields checked */
    n = vcard_build(CARD, 0x0107, p, VCARD_PACKETS_MAX);  /* First name, name, phone, LinkedIn */
    vcard_rx_init(&rx);
    CHECK(feed(&rx, p, n, -1));
    CHECK_STR(rx.values[0], "Tristan");
    CHECK_STR(rx.values[2], "0612345678");
    CHECK_STR(rx.values[3], "");
    CHECK_STR(rx.values[8], "https://www.linkedin.com/in/tristansalaun");
    CHECK(rx.crc != crc);

    /* A lost line: rejected; the next sending completes it (the receiver starts again on BEGIN:VCARD) */
    n = vcard_build(CARD, 0x1FFF, p, VCARD_PACKETS_MAX);
    vcard_rx_init(&rx);
    CHECK(! feed(&rx, p, n, 5));
    CHECK(feed(&rx, p, n, -1));
    CHECK_EQ(rx.crc, crc);
    /* A lost continuation (folded line): rejected too */
    int cont = -1;
    for (int i = 0; i < n; ++i)
        if (p[i].text[0] == ' ')
            cont = i;
    vcard_rx_init(&rx);
    CHECK(! feed(&rx, p, n, cont));
    /* A corrupted value (same number of lines): rejected by the CRC */
    vcard_packet_t bad[VCARD_PACKETS_MAX];
    memcpy(bad, p, sizeof(bad));
    bad[4].text[5] ^= 1;
    vcard_rx_init(&rx);
    CHECK(! feed(&rx, bad, n, -1));

    /* Typed on a Flipper (subghz chat): its name and colors before each line, no check line: accepted */
    static const char *FLIPPER[] = {
        "\x1b[0;33mArdyaro\x1b[0m: BEGIN:VCARD\r\n", "\x1b[0;33mArdyaro\x1b[0m: N:Dupont;Marie;;;\r\n",
        "\x1b[0;33mArdyaro\x1b[0m: TEL;TYPE=CELL:+33600000000\r\n", "\x1b[0;33mArdyaro\x1b[0m: URL:https://flipper.net\r\n",
        "\x1b[0;33mArdyaro\x1b[0m: END:VCARD\r\n",
    };
    vcard_rx_init(&rx);
    bool done = false;
    for (int i = 0; i < 5; ++i)
        done = vcard_rx_packet(&rx, FLIPPER[i], strlen(FLIPPER[i]));
    CHECK(done);
    CHECK_STR(rx.values[0], "Marie");
    CHECK_STR(rx.values[1], "Dupont");
    CHECK_STR(rx.values[2], "+33600000000");
    CHECK_STR(rx.values[10], "https://flipper.net");

    /* Other texts of the chat are ignored */
    vcard_rx_init(&rx);
    CHECK(! vcard_rx_packet(&rx, "SecSea Tristan coucou #1\n", 25));
    CHECK(! vcard_rx_packet(&rx, "END:VCARD\r\n", 11));  /* Without BEGIN */

    /* The sizes: nothing lost on the longest values (55 characters) */
    const char *longest[VCARD_FIELDS] = {"", "", "", "", "", "", "", "", "", "", "", "", ""};
    char url[56];
    memset(url, 'u', 55);
    url[55] = 0;
    longest[8] = url;
    n = vcard_build(longest, 0x1FFF, p, VCARD_PACKETS_MAX);
    vcard_rx_init(&rx);
    CHECK(feed(&rx, p, n, -1));
    CHECK_STR(rx.values[8], url);
    TEST_END();
}
