/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Contact cards: my card (typed with the 4 buttons, each field checked to be sent or not), exchanged with the badges
 * close by when both are in exchange mode (explicit consent), the cards received kept in the flash (store_ext_t)
 * and exported on USB as vCards ("k" key on the serial port).
 * On the air: a vCard in clear, one line per packet, on the profile of the chat of the Flipper Zero (vcard.c): a
 * Flipper ("subghz chat") or any CC1101 reads the cards, and can send one. The card is sent again every few
 * seconds; its line X-SECSEA-CHECK (number of lines, CRC) rejects a card with a lost or mixed line. */

#include <stdio.h>
#include <string.h>

#include "pico/rand.h"

#include "app.h"
#include "net.h"
#include "remote.h"
#include "social.h"
#include "store.h"
#include "vcard.h"

#define SEND_PERIOD_MS 2500  /* The whole card again (+ up to 1 s at random: two badges don't stay in step) */
#define PACKET_GAP_MS 70  /* A line of 60 bytes lasts ~55 ms on the air */
#define LISTEN_MS 2000  /* After a packet heard, our card waits this long (the other one may go on) */

typedef struct {
    const char *label;
    const char *vcard;  /* Property of the vCard (NULL: special) */
    uint8_t size;  /* Bytes in the card, including the final 0 */
    const char *charset;
} field_t;

static const field_t FIELDS[] = {
    {"Prénom", NULL, 20, UI_CHARSET_TEXT},
    {"Nom", NULL, 24, UI_CHARSET_TEXT},
    {"Téléphone", "TEL", 20, UI_CHARSET_PHONE},
    {"E-mail", "EMAIL", 48, UI_CHARSET_TEXT},
    {"Société", "ORG", 32, UI_CHARSET_TEXT},
    {"Poste", "TITLE", 32, UI_CHARSET_TEXT},
    {"Adresse", NULL, 48, UI_CHARSET_TEXT},
    {"Ville", NULL, 24, UI_CHARSET_TEXT},
    {"LinkedIn", "URL;TYPE=linkedin", 56, UI_CHARSET_TEXT},
    {"Git", "URL;TYPE=git", 56, UI_CHARSET_TEXT},
    {"Site web", "URL", 56, UI_CHARSET_TEXT},
    {"Mastodon", "X-MASTODON", 48, UI_CHARSET_TEXT},
    {"Commentaire", "NOTE", 48, UI_CHARSET_TEXT},
};
#define N_FIELDS ((int)(sizeof(FIELDS) / sizeof(FIELDS[0])))
_Static_assert(N_FIELDS == VCARD_FIELDS, "the fields of vcard.c");

static char *field(contact_card_t *c, int f) {
    int offset = 0;
    for (int i = 0; i < f; ++i)
        offset += FIELDS[i].size;
    return c->bytes + offset;
}

/* A short name of the card: first name and name, or the company */
static void card_name(contact_card_t *c, char *buf, size_t len) {
    snprintf(buf, len, "%s %s", field(c, 0), field(c, 1));
    if (! strcmp(buf, " "))
        snprintf(buf, len, "%s", field(c, 4)[0] ? field(c, 4) : "(sans nom)");
}

/* ------ Radio: vCards on the chat profile ------ */

static vcard_packet_t packets[VCARD_PACKETS_MAX];
static int n_packets = 0, send_packet = 0;
static absolute_time_t send_ts = 0;
static bool exchanging = false;
static vcard_rx_t vrx;
static bool rx_done = false;  /* Complete: to accept or not */
static uint16_t last_crc = 0;  /* Last card accepted or ignored (it is sent again and again) */
static bool have_last = false;
static contact_card_t rx_card;
static bool changed = false;

static void on_chat(const char *text, int len, int rssi) {
    if (! exchanging || rssi < SOCIAL_RSSI_CLOSE)
        return;  /* "Badges proches": a card from further away is not taken (the same threshold as a meeting) */
    /* Someone is talking (a badge, a Flipper typing a card): the next sending of our card waits, a radio does not
     * hear while it sends */
    absolute_time_t later = delayed_by_ms(get_absolute_time(), LISTEN_MS);
    if (send_packet == 0 && absolute_time_diff_us(send_ts, later) > 0)
        send_ts = later;
    if (rx_done || ! vcard_rx_packet(&vrx, text, len))
        return;
    if (have_last && vrx.crc == last_crc)
        return;  /* Already accepted or ignored */
    memset(&rx_card, 0, sizeof(rx_card));
    for (int f = 0; f < N_FIELDS; ++f)
        snprintf(field(&rx_card, f), FIELDS[f].size, "%s", vrx.values[f]);
    rx_done = true;
    changed = true;
    char name[48];
    card_name(&rx_card, name, sizeof(name));
    printf("contacts: card of %s received\n", name);
}

void contacts_init(void) {
}

static void send_task(absolute_time_t now) {
    if (! exchanging || ! n_packets || absolute_time_diff_us(send_ts, now) < 0 || net_queue_free() < 2)
        return;
    if (! net_send_text(packets[send_packet].text, packets[send_packet].len))
        return;
    if (++send_packet >= n_packets) {
        send_packet = 0;
        send_ts = delayed_by_ms(now, SEND_PERIOD_MS + get_rand_32() % 1000);  /* The whole card again later */
    } else {
        send_ts = delayed_by_ms(now, PACKET_GAP_MS);
    }
}

static void start_exchange(void) {
    store_ext_t *e = store_ext_get();
    const char *values[N_FIELDS];
    for (int f = 0; f < N_FIELDS; ++f)
        values[f] = field(&e->mine, f);
    n_packets = vcard_build(values, e->send_mask, packets, VCARD_PACKETS_MAX);
    send_packet = 0;
    send_ts = get_absolute_time();
    vcard_rx_init(&vrx);
    rx_done = false;
    exchanging = true;
    remote_pause_windows(true);  /* The radio stays on the chat profile */
    net_set_chat(on_chat);
    printf("contacts: exchange, %d packets to send\n", n_packets);
}

static void stop_exchange(void) {
    if (! exchanging)
        return;
    exchanging = false;
    net_set_chat(NULL);  /* Back to the network of the cicadas */
    remote_pause_windows(false);
    printf("contacts: exchange stopped\n");
}

/* USB export: the cards received as vCards */
void contacts_export(void) {
    store_ext_t *e = store_ext_get();
    printf("contacts: %u card(s)\n", e->n_contacts);
    for (int i = 0; i < e->n_contacts; ++i) {
        contact_card_t *c = &e->contacts[i];
        printf("BEGIN:VCARD\nVERSION:3.0\nN:%s;%s;;;\nFN:%s %s\n", field(c, 1), field(c, 0), field(c, 0), field(c, 1));
        if (field(c, 6)[0] || field(c, 7)[0])
            printf("ADR:;;%s;%s;;;\n", field(c, 6), field(c, 7));
        for (int f = 2; f < N_FIELDS; ++f)
            if (FIELDS[f].vcard && field(c, f)[0])
                printf("%s:%s\n", FIELDS[f].vcard, field(c, f));
        printf("END:VCARD\n");
    }
}


/* ------ Pages ------ */

enum { V_MAIN, V_CARD, V_EDIT, V_EXCHANGE, V_LIST, V_VIEW };

static int view = V_MAIN;
static int sel = 0, card_sel = 0, list_sel = 0;
static ui_edit_t edit;

static void main_label(int i, char *buf, size_t len) {
    static const char *L[] = {"Ma carte", "Échanger les cartes", "Contacts reçus"};
    if (i == 2)
        snprintf(buf, len, "%s (%u)", L[i], store_ext_get()->n_contacts);
    else
        snprintf(buf, len, "%s", L[i]);
}

static void card_label(int i, char *buf, size_t len) {
    store_ext_t *e = store_ext_get();
    const char *v = field(&e->mine, i);
    snprintf(buf, len, "%s %s : %s", e->send_mask & (1 << i) ? "[x]" : "[ ]", FIELDS[i].label, v[0] ? v : "-");
}

static void list_label(int i, char *buf, size_t len) {
    card_name(&store_ext_get()->contacts[i], buf, len);
}

static void contacts_start(absolute_time_t now) {
    (void)now;
    view = V_MAIN;
    sel = 0;
}

static void keep_card(void) {
    store_ext_t *e = store_ext_get();
    if (e->n_contacts == STORE_CONTACTS) {
        memmove(&e->contacts[0], &e->contacts[1], sizeof(e->contacts[0]) * (STORE_CONTACTS - 1));  /* Forget the oldest */
        --e->n_contacts;
    }
    e->contacts[e->n_contacts++] = rx_card;
    store_ext_changed();
    printf("contacts: kept (%u)\n", e->n_contacts);
}

static bool contacts_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    store_ext_t *e = store_ext_get();
    switch (view) {
    case V_MAIN:
        if (b->pressed & UI_BTN_A)
            return false;
        if (b->pressed & UI_BTN_Y)
            sel = (sel + 2) % 3;
        if (b->pressed & UI_BTN_X)
            sel = (sel + 1) % 3;
        if (b->pressed & UI_BTN_B) {
            view = sel == 0 ? V_CARD : sel == 1 ? V_EXCHANGE : V_LIST;
            if (view == V_EXCHANGE)
                start_exchange();
            list_sel = 0;
        }
        break;
    case V_CARD:
        if (b->pressed & UI_BTN_A)
            view = V_MAIN;
        if (b->pressed & UI_BTN_Y)
            card_sel = (card_sel + N_FIELDS - 1) % N_FIELDS;
        if (b->pressed & UI_BTN_X)
            card_sel = (card_sel + 1) % N_FIELDS;
        if (b->long_pressed & UI_BTN_B) {
            e->send_mask ^= 1 << card_sel;  /* Send this field or not */
            store_ext_changed();
        } else if (b->released_short & UI_BTN_B) {
            ui_edit_start(&edit, field(&e->mine, card_sel), FIELDS[card_sel].size - 1, FIELDS[card_sel].charset);
            view = V_EDIT;
        }
        break;
    case V_EDIT:
        switch (app_edit_buttons(&edit, b)) {
        case UI_EDIT_DONE:
            ui_edit_result(&edit, field(&e->mine, card_sel), FIELDS[card_sel].size);
            e->send_mask |= 1 << card_sel;
            store_ext_changed();
            printf("contacts: %s saved\n", FIELDS[card_sel].label);
            view = V_CARD;
            break;
        case UI_EDIT_CANCEL:
            view = V_CARD;
            break;
        }
        break;
    case V_EXCHANGE:
        if (rx_done) {
            if (b->pressed & UI_BTN_B)
                keep_card();
            if (b->pressed & (UI_BTN_A | UI_BTN_B)) {
                last_crc = vrx.crc;
                have_last = true;
                rx_done = false;
            }
            break;
        }
        if (b->pressed & UI_BTN_A) {
            stop_exchange();
            view = V_MAIN;
        }
        break;
    case V_LIST:
        if (b->pressed & UI_BTN_A)
            view = V_MAIN;
        if (! e->n_contacts)
            break;
        if (b->pressed & UI_BTN_Y)
            list_sel = (list_sel + e->n_contacts - 1) % e->n_contacts;
        if (b->pressed & UI_BTN_X)
            list_sel = (list_sel + 1) % e->n_contacts;
        if (b->long_pressed & UI_BTN_B) {
            memmove(&e->contacts[list_sel], &e->contacts[list_sel + 1],
                    sizeof(e->contacts[0]) * (e->n_contacts - list_sel - 1));
            --e->n_contacts;
            if (list_sel >= e->n_contacts && list_sel)
                --list_sel;
            store_ext_changed();
        } else if (b->released_short & UI_BTN_B) {
            view = V_VIEW;
        }
        break;
    default:
        if (b->pressed & (UI_BTN_A | UI_BTN_B))
            view = V_LIST;
        break;
    }
    return true;
}

static bool contacts_task(absolute_time_t now) {
    send_task(now);
    if (exchanging && view != V_EXCHANGE)
        stop_exchange();
    bool c = changed;
    changed = false;
    return c;
}

static void contacts_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    store_ext_t *e = store_ext_get();
    char text[64];
    switch (view) {
    case V_MAIN:
        ui_title(fb, "Contacts");
        ui_list(fb, 3, sel, main_label);
        ui_footer(fb, "G : retour  D : ouvrir");
        break;
    case V_CARD:
        ui_title(fb, "Ma carte");
        ui_list(fb, N_FIELDS, card_sel, card_label);
        ui_footer(fb, "D : modifier  D long : cocher");
        break;
    case V_EDIT:
        ui_edit_render(fb, &edit, FIELDS[card_sel].label, "");
        break;
    case V_EXCHANGE:
        ui_title(fb, "Échange de cartes");
        if (rx_done) {
            card_name(&rx_card, text, sizeof(text));
            ui_lines(fb, 40, &gfx_font_small, "Carte reçue :");
            ui_lines(fb, 60, &gfx_font_medium, text);
            ui_lines(fb, 90, &gfx_font_small, field(&rx_card, 4));
            ui_footer(fb, "G : ignorer  D : garder");
        } else {
            ui_lines(fb, 40, &gfx_font_small, "Votre carte (vCard) est\nenvoyée aux cigales en mode\néchange, et lisible par un\nFlipper (subghz chat).\nRapprochez les badges !");
            ui_footer(fb, "G : arrêter");
        }
        break;
    case V_LIST:
        snprintf(text, sizeof(text), "Contacts (%u)", e->n_contacts);
        ui_title(fb, text);
        if (e->n_contacts)
            ui_list(fb, e->n_contacts, list_sel, list_label);
        else
            ui_lines(fb, 60, &gfx_font_small, "Aucun contact reçu.\nExport USB : touche k\n(tools/contacts_export.py)");
        ui_footer(fb, e->n_contacts ? "D : voir  D long : supprimer" : "G : retour");
        break;
    default: {
        contact_card_t *c = &e->contacts[list_sel];
        card_name(c, text, sizeof(text));
        ui_title(fb, text);
        int y = UI_TITLE_H + 3;
        for (int f = 2; f < N_FIELDS && y < UI_FOOTER_Y - 16; ++f) {
            if (! field(c, f)[0])
                continue;
            snprintf(text, sizeof(text), "%s : %s", FIELDS[f].label, field(c, f));
            y = ui_text(fb, 3, y, &gfx_font_small, text) - 1;
        }
        ui_footer(fb, "G : retour");
        break;
    }
    }
}

const app_t app_contacts = {
    .name = "Contacts",
    .start = contacts_start,
    .buttons = contacts_buttons,
    .task = contacts_task,
    .render = contacts_render,
    .stop = stop_exchange,
};
