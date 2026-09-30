/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Contact cards: my card (typed with the 4 buttons, each field checked to be sent or not), exchanged with the badges
 * close by when both are in exchange mode (explicit consent), the cards received kept in the flash (store_ext_t)
 * and exported on USB as vCards ("k" key on the serial port).
 * NET_CONTACT [card uid 2][chunk][chunks][data]: the card is [field][length][bytes]... cut in chunks of 48 bytes. */

#include <stdio.h>
#include <string.h>

#include "pico/rand.h"

#include "app.h"
#include "net.h"
#include "social.h"
#include "store.h"

#define CHUNK 48
#define MAX_SERIAL (CONTACT_BYTES + 2 * 16)
#define MAX_CHUNKS ((MAX_SERIAL + CHUNK - 1) / CHUNK)
#define SEND_PERIOD_MS 3000
#define ASSEMBLY_TIMEOUT_MS 10000

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
    {"E-mail", "EMAIL", 40, UI_CHARSET_TEXT},
    {"Société", "ORG", 32, UI_CHARSET_TEXT},
    {"Poste", "TITLE", 32, UI_CHARSET_TEXT},
    {"Adresse", NULL, 48, UI_CHARSET_TEXT},
    {"Ville", NULL, 24, UI_CHARSET_TEXT},
    {"LinkedIn", "URL;TYPE=linkedin", 40, UI_CHARSET_TEXT},
    {"Git", "URL;TYPE=git", 40, UI_CHARSET_TEXT},
    {"Site web", "URL", 40, UI_CHARSET_TEXT},
    {"Mastodon", "X-MASTODON", 40, UI_CHARSET_TEXT},
    {"Commentaire", "NOTE", 48, UI_CHARSET_TEXT},
};
#define N_FIELDS ((int)(sizeof(FIELDS) / sizeof(FIELDS[0])))

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

/* ------ Radio ------ */

static uint8_t serial_buf[MAX_SERIAL];
static int serial_len = 0;
static uint16_t my_uid = 0;
static int send_chunk = 0;
static absolute_time_t send_ts = 0;
static bool exchanging = false;

/* Received card being assembled */
static uint32_t rx_src = 0;
static uint16_t rx_uid = 0;
static uint8_t rx_chunks = 0;
static uint32_t rx_have = 0;  /* Bit per chunk */
static uint8_t rx_buf[MAX_CHUNKS * CHUNK];
static absolute_time_t rx_ts = 0;
static bool rx_done = false;  /* Complete: to accept or not */
static uint16_t last_uid = 0;  /* Last card accepted or ignored (it is sent again and again) */
static uint32_t last_src = 0;
static contact_card_t rx_card;
static bool changed = false;

static void serialize(void) {
    store_ext_t *e = store_ext_get();
    serial_len = 0;
    for (int f = 0; f < N_FIELDS; ++f) {
        const char *v = field(&e->mine, f);
        int n = strnlen(v, FIELDS[f].size - 1);
        if (! n || ! (e->send_mask & (1 << f)))
            continue;
        serial_buf[serial_len++] = f;
        serial_buf[serial_len++] = n;
        memcpy(serial_buf + serial_len, v, n);
        serial_len += n;
    }
    my_uid = (get_rand_32() & 0xFFFE) + 1;
}

static void deserialize(const uint8_t *d, int len, contact_card_t *c) {
    memset(c, 0, sizeof(*c));
    for (int i = 0; i + 2 <= len; ) {
        int f = d[i], n = d[i + 1];
        i += 2;
        if (f >= N_FIELDS || i + n > len)
            break;
        int m = n < FIELDS[f].size - 1 ? n : FIELDS[f].size - 1;
        memcpy(field(c, f), d + i, m);
        i += n;
    }
}

static void handle_contact(const net_packet_t *p) {
    if (! exchanging || rx_done || p->len < 5)
        return;
    uint16_t uid = p->data[0] | p->data[1] << 8;
    uint8_t i = p->data[2], n = p->data[3];
    if (n == 0 || n > MAX_CHUNKS || i >= n)
        return;
    if (p->src == last_src && uid == last_uid)
        return;  /* Already accepted or ignored */
    if (p->src != rx_src || uid != rx_uid) {
        /* Another card: only when the previous one stalled */
        if (rx_src && absolute_time_diff_us(rx_ts, p->at) < ASSEMBLY_TIMEOUT_MS * 1000ll)
            return;
        rx_src = p->src;
        rx_uid = uid;
        rx_chunks = n;
        rx_have = 0;
        memset(rx_buf, 0, sizeof(rx_buf));
    }
    rx_ts = p->at;
    memcpy(rx_buf + i * CHUNK, p->data + 4, p->len - 4);
    rx_have |= 1u << i;
    if (rx_have == (1u << rx_chunks) - 1) {
        deserialize(rx_buf, rx_chunks * CHUNK, &rx_card);
        rx_done = true;
        changed = true;
        char name[48];
        card_name(&rx_card, name, sizeof(name));
        printf("contacts: card of %s received\n", name);
    }
}

void contacts_init(void) {
    net_subscribe(NET_CONTACT, handle_contact);
}

static void send_task(absolute_time_t now) {
    if (! exchanging || ! serial_len || absolute_time_diff_us(send_ts, now) < 0)
        return;
    int n = (serial_len + CHUNK - 1) / CHUNK;
    uint8_t d[4 + CHUNK];
    d[0] = my_uid;
    d[1] = my_uid >> 8;
    d[2] = send_chunk;
    d[3] = n;
    int len = serial_len - send_chunk * CHUNK < CHUNK ? serial_len - send_chunk * CHUNK : CHUNK;
    memcpy(d + 4, serial_buf + send_chunk * CHUNK, len);
    if (! net_send(NET_CONTACT, d, 4 + len, NET_MEDIUM))
        return;
    if (++send_chunk >= n) {
        send_chunk = 0;
        send_ts = delayed_by_ms(now, SEND_PERIOD_MS);  /* The whole card again later */
    } else {
        send_ts = delayed_by_ms(now, 60);
    }
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
    static const char *L[] = {"Ma carte", "Echanger (badges proches)", "Contacts reçus"};
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

static void start_exchange(void) {
    serialize();
    exchanging = true;
    send_chunk = 0;
    send_ts = get_absolute_time();
    rx_src = 0;
    rx_done = false;
    printf("contacts: exchange, %d bytes to send\n", serial_len);
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
        if (b->long_pressed & UI_BTN_B) {
            ui_edit_result(&edit, field(&e->mine, card_sel), FIELDS[card_sel].size);
            e->send_mask |= 1 << card_sel;
            store_ext_changed();
            printf("contacts: %s saved\n", FIELDS[card_sel].label);
            view = V_CARD;
            break;
        }
        if (b->released_short & UI_BTN_B)
            ui_edit_move(&edit, 1);
        if ((b->pressed & UI_BTN_A) && ! ui_edit_move(&edit, -1))
            view = V_CARD;
        /* Flanks: the characters, faster while held */
        for (int f = 0; f < 2; ++f) {
            uint8_t bit = f ? UI_BTN_X : UI_BTN_Y;
            static absolute_time_t repeat_ts[2];
            if (b->pressed & bit) {
                ui_edit_change(&edit, f ? 1 : -1);
                repeat_ts[f] = delayed_by_ms(now, 400);
            } else if ((b->held & bit) && absolute_time_diff_us(repeat_ts[f], now) >= 0) {
                ui_edit_change(&edit, f ? 1 : -1);
                repeat_ts[f] = delayed_by_ms(now, 90);
            }
        }
        break;
    case V_EXCHANGE:
        if (rx_done) {
            if (b->pressed & UI_BTN_B)
                keep_card();
            if (b->pressed & (UI_BTN_A | UI_BTN_B)) {
                last_src = rx_src;
                last_uid = rx_uid;
                rx_done = false;
                rx_src = 0;
            }
            break;
        }
        if (b->pressed & UI_BTN_A) {
            exchanging = false;
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
        exchanging = false;
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
        ui_footer(fb, "D : modifier  D long : [x] envoyer");
        break;
    case V_EDIT:
        ui_edit_render(fb, &edit, FIELDS[card_sel].label, "");
        break;
    case V_EXCHANGE:
        ui_title(fb, "Echange de cartes");
        if (rx_done) {
            card_name(&rx_card, text, sizeof(text));
            ui_lines(fb, 40, &gfx_font_small, "Carte reçue :");
            ui_lines(fb, 60, &gfx_font_medium, text);
            ui_lines(fb, 90, &gfx_font_small, field(&rx_card, 4));
            ui_footer(fb, "G : ignorer  D : garder");
        } else {
            ui_lines(fb, 40, &gfx_font_small, "Votre carte est envoyée\nà la cigale d'à côté si elle\nest aussi en mode échange.\n\nRapprochez les badges !");
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
};
