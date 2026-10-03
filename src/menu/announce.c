/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Announcements (coffee break, next talk...): written on an admin badge (Admin > Annonces: 6 of them, editable,
 * saved in the store), sent to all the cicadas which build the screen from what they received: the time, the text,
 * a QR code made from a content and a standard type (URL, text, phone, SMS, e-mail, Wi-Fi, position).
 * NET_ANNOUNCE [nonce 2][part][parts][up to 48 bytes]: the announcement "time\0text\0<type>qr\0" cut in parts, the
 * whole sent 3 times; the cicadas keep the last 5 (Social > Annonces). */

#include <stdio.h>
#include <string.h>

#include "pico/rand.h"

#include "announce.h"
#include "app.h"
#include "display.h"
#include "i18n.h"
#include "net.h"
#include "score_code.h"
#include "store.h"

#define PART 48
#define MAX_PARTS 5
#define SERIAL_MAX (PART * MAX_PARTS)
#define ROUNDS 3
#define PART_GAP_MS 70
#define ROUND_GAP_MS 600
#define HISTORY 5

static const char *const QR_NAMES[ANNOUNCE_QR_TYPES] = {N_("Aucun"), N_("Lien (URL)"), N_("Texte"),
                                                        N_("Téléphone"), "SMS", "E-mail", "Wi-Fi",
                                                        N_("Position GPS")};
static const char *const QR_HELP[ANNOUNCE_QR_TYPES] = {"", "https://...", N_("Un texte"), "+33612345678",
                                                       N_("numéro:message"), "adresse@mail.fr",
                                                       N_("réseau;mot de passe"), "43.17,5.60"};

/* The announcements of the admin menu, when none was saved yet */
static const store_announce_t DEFAULTS[STORE_ANNOUNCES] = {
    {"09:00", "Bienvenue à SecSea 2026 ! Accueil et café", ANNOUNCE_QR_URL, "https://www.hackinprovence.fr/"},
    {"10:30", "Pause café : rendez-vous au bar", ANNOUNCE_QR_NONE, ""},
    {"12:30", "Pause déjeuner : bon appétit !", ANNOUNCE_QR_NONE, ""},
    {"14:00", "Reprise des conférences", ANNOUNCE_QR_NONE, ""},
    {"17:30", "Remise des prix du CTF", ANNOUNCE_QR_URL, "https://www.hackinprovence.fr/"},
    {"18:30", "Apéro de clôture : merci à tous !", ANNOUNCE_QR_NONE, ""},
};

store_announce_t *announce_list(void) {
    store_ext_t *e = store_ext_get();
    if (e->announce_magic != STORE_ANNOUNCE_MAGIC) {
        memcpy(e->announces, DEFAULTS, sizeof(DEFAULTS));
        e->announce_magic = STORE_ANNOUNCE_MAGIC;  /* Saved with the next change */
    }
    return e->announces;
}

/* The text of the QR code, in the standard forms that the phones understand */
void announce_qr_text(const store_announce_t *a, char *buf, int len) {
    const char *q = a->qr;
    buf[0] = 0;
    switch (a->qr_type) {
    case ANNOUNCE_QR_URL:
        snprintf(buf, len, "%s%s", strstr(q, "://") ? "" : "https://", q);
        break;
    case ANNOUNCE_QR_TEXT: snprintf(buf, len, "%s", q); break;
    case ANNOUNCE_QR_TEL: snprintf(buf, len, "tel:%s", q); break;
    case ANNOUNCE_QR_SMS: snprintf(buf, len, "SMSTO:%s", q); break;
    case ANNOUNCE_QR_EMAIL: snprintf(buf, len, "mailto:%s", q); break;
    case ANNOUNCE_QR_WIFI: {
        const char *sep = strchr(q, ';');
        if (sep)
            snprintf(buf, len, "WIFI:T:WPA;S:%.*s;P:%s;;", (int)(sep - q), q, sep + 1);
        else
            snprintf(buf, len, "WIFI:T:nopass;S:%s;;", q);
        break;
    }
    case ANNOUNCE_QR_GEO: snprintf(buf, len, "geo:%s", q); break;
    default: break;
    }
    if (! q[0])
        buf[0] = 0;
}

/* The screen of an announcement: the time in a black band, the text, the QR code */
void announce_draw(uint8_t *fb, const store_announce_t *a) {
    gfx_fill_rect(fb, 0, 0, GFX_WIDTH, 38, GFX_BLACK);
    gfx_text(fb, GFX_WIDTH/2, 3, &gfx_font_large, a->time[0] ? a->time : N_("Annonce"), GFX_WHITE, GFX_ALIGN_CENTER);
    char qr[96];
    announce_qr_text(a, qr, sizeof(qr));
    int y = ui_wrapped(fb, 44, &gfx_font_medium, a->text, qr[0] ? 3 : 6);
    if (! qr[0])
        return;
    int size = score_code_draw(NULL, qr, 0, 1);
    int room = GFX_HEIGHT - 2 - y;
    int scale = size ? room / size : 0;
    if (scale > 4)
        scale = 4;
    if (scale >= 2)
        score_code_draw(fb, qr, y + (room - size * scale) / 2, scale);
    else
        ui_lines(fb, y + 4, &gfx_font_small, N_("(QR code trop grand)"));
}

/* ------ Radio ------ */

static int serialize(const store_announce_t *a, uint8_t *buf) {
    int n = 0;
    n += snprintf((char *)buf + n, SERIAL_MAX - n, "%s", a->time) + 1;
    n += snprintf((char *)buf + n, SERIAL_MAX - n, "%s", a->text) + 1;
    buf[n++] = a->qr_type;
    n += snprintf((char *)buf + n, SERIAL_MAX - n, "%s", a->qr) + 1;
    return n;
}

static bool deserialize(const uint8_t *buf, int len, store_announce_t *a) {
    memset(a, 0, sizeof(*a));
    const char *p = (const char *)buf, *end = p + len;
    const char *fields[2];
    for (int i = 0; i < 2; ++i) {
        fields[i] = p;
        p = memchr(p, 0, end - p);
        if (! p)
            return false;
        ++p;
    }
    if (p >= end)
        return false;
    uint8_t type = *p++;
    const char *qr = p;
    if (! memchr(qr, 0, end - qr) || type >= ANNOUNCE_QR_TYPES)
        return false;
    snprintf(a->time, sizeof(a->time), "%s", fields[0]);
    snprintf(a->text, sizeof(a->text), "%s", fields[1]);
    a->qr_type = type;
    snprintf(a->qr, sizeof(a->qr), "%s", qr);
    return true;
}

/* Sending */
static uint8_t tx_buf[SERIAL_MAX];
static int tx_len = 0, tx_part = 0, tx_rounds = 0;
static uint16_t tx_nonce = 0;
static absolute_time_t tx_ts = 0;

void announce_send(const store_announce_t *a) {
    tx_len = serialize(a, tx_buf);
    tx_nonce = get_rand_32();
    tx_part = 0;
    tx_rounds = ROUNDS;
    tx_ts = get_absolute_time();
    printf("announce: sending \"%s\" (%d bytes)\n", a->text, tx_len);
}

bool announce_sending(void) {
    return tx_rounds > 0;
}

/* Receiving */
static store_announce_t received[HISTORY];  /* The newest first */
static int n_received = 0;
static bool fresh = false;  /* A new one: the main loop notifies it */
static uint8_t rx_buf[SERIAL_MAX];
static uint32_t rx_src = 0, done_src = 0;
static uint16_t rx_nonce = 0, done_nonce = 0;
static uint8_t rx_have = 0, rx_parts = 0;
static int rx_len = 0;

static void handle_announce(const net_packet_t *p) {
    if (p->len < 5)
        return;
    const uint8_t *d = p->data;
    uint16_t nonce = d[0] | d[1] << 8;
    uint8_t part = d[2], parts = d[3];
    int n = p->len - 4;
    if (parts == 0 || parts > MAX_PARTS || part >= parts || n > PART)
        return;
    if (p->src == done_src && nonce == done_nonce)
        return;  /* Already shown (it is sent 3 times) */
    if (p->src != rx_src || nonce != rx_nonce || parts != rx_parts) {
        rx_src = p->src;  /* Another announcement */
        rx_nonce = nonce;
        rx_parts = parts;
        rx_have = 0;
        rx_len = 0;
    }
    memcpy(rx_buf + part * PART, d + 4, n);
    if (part == parts - 1)
        rx_len = part * PART + n;
    rx_have |= 1 << part;
    if (rx_have != (1 << parts) - 1 || ! rx_len)
        return;
    store_announce_t a;
    if (! deserialize(rx_buf, rx_len, &a))
        return;
    done_src = p->src;
    done_nonce = nonce;
    memmove(&received[1], &received[0], sizeof(received[0]) * (HISTORY - 1));
    received[0] = a;
    if (n_received < HISTORY)
        ++n_received;
    fresh = true;
    printf("announce: received \"%s\" at %s, QR %s \"%s\"\n", a.text, a.time, QR_NAMES[a.qr_type], a.qr);
}

void announce_init(void) {
    net_subscribe(NET_ANNOUNCE, handle_announce);
}

void announce_task(absolute_time_t now) {
    if (! tx_rounds || absolute_time_diff_us(tx_ts, now) < 0)
        return;
    int parts = (tx_len + PART - 1) / PART;
    uint8_t d[4 + PART] = {tx_nonce, tx_nonce >> 8, tx_part, parts};
    int n = tx_len - tx_part * PART < PART ? tx_len - tx_part * PART : PART;
    memcpy(d + 4, tx_buf + tx_part * PART, n);
    if (! net_send(NET_ANNOUNCE, d, 4 + n, NET_LOUD))
        return;
    if (++tx_part == parts) {
        tx_part = 0;
        --tx_rounds;
        tx_ts = delayed_by_ms(now, ROUND_GAP_MS);
    } else {
        tx_ts = delayed_by_ms(now, PART_GAP_MS);
    }
}

bool announce_new(char *buf, int len) {
    if (! fresh)
        return false;
    fresh = false;
    snprintf(buf, len, _("Annonce : %s"), received[0].text);
    return true;
}

/* ------ The page of the cicadas: the last announcements (Social > Annonces) ------ */

static int shown = 0;  /* In received[] */
static int list_sel = 0;
static bool open_newest = false;

static void still_render(uint8_t *fb) {
    announce_draw(fb, &received[shown]);
}

static void received_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s %s", received[i].time, received[i].text);
}

void announce_open_newest(void) {
    open_newest = true;  /* The notification opens the page: the announcement right away */
}

static void announces_start(absolute_time_t now) {
    (void)now;
    list_sel = 0;
    if (open_newest && n_received) {
        open_newest = false;
        shown = 0;
        app_show_still(still_render);  /* Like the screensaver: no ghost, stays; any button leaves */
    }
}

static bool announces_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_A)
        return false;
    if (! n_received)
        return true;
    if (b->pressed & UI_BTN_Y)
        list_sel = (list_sel + n_received - 1) % n_received;
    if (b->pressed & UI_BTN_X)
        list_sel = (list_sel + 1) % n_received;
    if (b->pressed & UI_BTN_B) {
        shown = list_sel;
        app_show_still(still_render);
    }
    return true;
}

static void announces_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    ui_title(fb, N_("Annonces"));
    if (! n_received) {
        ui_lines(fb, 60, &gfx_font_small,
                 N_("Aucune annonce reçue.\nElles s'affichent toutes\nseules quand elles arrivent."));
        ui_footer(fb, N_("G : retour"));
        return;
    }
    ui_list(fb, n_received, list_sel, received_label);
    ui_footer(fb, N_("G : retour  D : afficher"));
}

const app_t app_announces = {
    .name = N_("Annonces"),
    .start = announces_start,
    .buttons = announces_buttons,
    .render = announces_render,
};

/* ------ Admin: write and send (Admin > Annonces) ------ */

enum { V_LIST, V_EDIT, V_FIELD, V_PREVIEW };
enum { ROW_TIME, ROW_TEXT, ROW_QR_TYPE, ROW_QR, ROW_PREVIEW, ROW_SEND, N_ROWS };

static int view = V_LIST, sel = 0, row = 0;
static ui_edit_t edit;
static char status[40] = "";

static void admin_label(int i, char *buf, size_t len) {
    const store_announce_t *a = &announce_list()[i];
    snprintf(buf, len, "%s %s", a->time, a->text);
}

static void row_label(int i, char *buf, size_t len) {
    const store_announce_t *a = &announce_list()[sel];
    switch (i) {
    case ROW_TIME: snprintf(buf, len, _("Heure : %s"), a->time); break;
    case ROW_TEXT: snprintf(buf, len, _("Texte : %s"), a->text); break;
    case ROW_QR_TYPE:
        snprintf(buf, len, _("QR code : %s"), tr(QR_NAMES[a->qr_type < ANNOUNCE_QR_TYPES ? a->qr_type : 0]));
        break;
    case ROW_QR: snprintf(buf, len, _("Contenu : %s"), a->qr[0] ? a->qr : "-"); break;
    case ROW_PREVIEW: snprintf(buf, len, N_("> Aperçu")); break;
    default: snprintf(buf, len, N_("> Envoyer à tous")); break;
    }
}

static void admin_start(absolute_time_t now) {
    (void)now;
    view = V_LIST;
    status[0] = 0;
}

static bool admin_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    store_announce_t *a = &announce_list()[sel];
    switch (view) {
    case V_LIST:
        if (b->pressed & UI_BTN_A)
            return false;
        if (b->pressed & UI_BTN_Y)
            sel = (sel + STORE_ANNOUNCES - 1) % STORE_ANNOUNCES;
        if (b->pressed & UI_BTN_X)
            sel = (sel + 1) % STORE_ANNOUNCES;
        if (b->pressed & UI_BTN_B) {
            view = V_EDIT;
            row = 0;
            status[0] = 0;
        }
        break;
    case V_EDIT:
        if (b->pressed & UI_BTN_Y)
            row = (row + N_ROWS - 1) % N_ROWS;
        if (b->pressed & UI_BTN_X)
            row = (row + 1) % N_ROWS;
        if (b->pressed & (UI_BTN_X | UI_BTN_Y))
            status[0] = 0;
        if (row == ROW_QR_TYPE && (b->pressed & (UI_BTN_A | UI_BTN_B))) {
            a->qr_type = (a->qr_type + ((b->pressed & UI_BTN_B) ? 1 : ANNOUNCE_QR_TYPES - 1)) % ANNOUNCE_QR_TYPES;
            store_ext_changed();
            break;
        }
        if (b->pressed & UI_BTN_A) {
            view = V_LIST;
            break;
        }
        if (b->pressed & UI_BTN_B) {
            switch (row) {
            case ROW_TIME: ui_edit_start(&edit, a->time, sizeof(a->time) - 1, UI_CHARSET_TEXT); view = V_FIELD; break;
            case ROW_TEXT: ui_edit_start(&edit, a->text, UI_EDIT_MAX, UI_CHARSET_LONG); view = V_FIELD; break;
            case ROW_QR: ui_edit_start(&edit, a->qr, UI_EDIT_MAX, UI_CHARSET_LONG); view = V_FIELD; break;
            case ROW_PREVIEW: view = V_PREVIEW; break;
            case ROW_SEND:
                announce_send(a);
                snprintf(status, sizeof(status), N_("Envoyée à toutes les cigales"));
                break;
            }
        }
        break;
    case V_FIELD:
        switch (app_edit_buttons(&edit, b)) {
        case UI_EDIT_DONE: {
            char *dst = row == ROW_TIME ? a->time : row == ROW_TEXT ? a->text : a->qr;
            int size = row == ROW_TIME ? (int)sizeof(a->time) : row == ROW_TEXT ? (int)sizeof(a->text) : (int)sizeof(a->qr);
            ui_edit_result(&edit, dst, size);
            store_ext_changed();
            printf("announce: %d edited\n", sel + 1);
            view = V_EDIT;
            break;
        }
        case UI_EDIT_CANCEL:
            view = V_EDIT;
            break;
        }
        break;
    default:
        if (b->pressed & (UI_BTN_A | UI_BTN_B))
            view = V_EDIT;
        break;
    }
    return true;
}

static bool admin_task(absolute_time_t now) {
    (void)now;
    static bool was_sending = false;
    bool c = was_sending != announce_sending();
    was_sending = announce_sending();
    return c;
}

static void admin_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    const store_announce_t *a = &announce_list()[sel];
    switch (view) {
    case V_LIST:
        ui_title(fb, N_("Annonces"));
        ui_list(fb, STORE_ANNOUNCES, sel, admin_label);
        ui_footer(fb, N_("G : retour  D : modifier, envoyer"));
        break;
    case V_EDIT: {
        char title[24];
        snprintf(title, sizeof(title), _("Annonce %d"), sel + 1);
        ui_title(fb, title);
        ui_list(fb, N_ROWS, row, row_label);
        const char *help = row == ROW_QR_TYPE ? N_("Ailes : type de QR code") : row == ROW_PREVIEW || row == ROW_SEND
                           ? N_("G : liste  D : valider") : N_("G : liste  D : modifier");
        ui_footer(fb, status[0] ? status : help);
        break;
    }
    case V_FIELD: {
        const char *prompts[] = {N_("Heure (ex. 10:30)"), N_("Texte de l'annonce"), ""};
        char prompt[64];
        if (row == ROW_QR)
            snprintf(prompt, sizeof(prompt), _("%s : %s"), tr(QR_NAMES[a->qr_type]), tr(QR_HELP[a->qr_type]));
        ui_edit_render(fb, &edit, row == ROW_TIME ? N_("Heure") : row == ROW_TEXT ? N_("Texte") : N_("QR code"),
                       row == ROW_QR ? prompt : prompts[row == ROW_TIME ? 0 : 1]);
        break;
    }
    default:
        announce_draw(fb, a);  /* Like on the cicadas (here with the fast refresh, cleaned soon) */
        display_settle_soon();
        break;
    }
}

const app_t app_announce_admin = {
    .name = N_("Annonces (admin)"),
    .start = admin_start,
    .buttons = admin_buttons,
    .task = admin_task,
    .render = admin_render,
};
