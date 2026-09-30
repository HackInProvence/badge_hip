/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Short messages between the cicadas, chosen in a list (4 buttons: no keyboard needed), relayed by the badges
 * (flooding with a TTL: a message goes through up to MSG_TTL badges to reach its recipient).
 * A cicada is identified by its name and the end of its id: "Tristan#33EC".
 * NET_MESSAGE [uid 2][ttl][origin id 4][recipient id 4, 0 = everybody][origin name 8][message] */

#include <stdio.h>
#include <string.h>

#include "pico/rand.h"

#include "app.h"
#include "net.h"
#include "social.h"

#define MSG_TTL 3
#define INBOX 10
#define SEEN 32
#define PACKET_LEN 20
#define SENDS 3
#define RESEND_MS 600  /* Longer than a listening window of the OOK remotes (220 ms) */

static const char *MESSAGES[] = {
    "Salut !", "Café ?", "On se retrouve au stand HIP", "Qui fait le CTF ?", "Je cherche un binôme",
    "Super talk !", "Où es-tu ?", "RDV à l'accueil", "Pause déjeuner ?", "Au bar ?", "Merci !", "Bravo !",
    "Besoin d'aide", "Qui a un chargeur USB-C ?", "Photo de groupe !", "A plus tard",
};
#define N_MESSAGES ((int)(sizeof(MESSAGES) / sizeof(MESSAGES[0])))

typedef struct {
    uint32_t from;
    char name[9];
    uint8_t message;
    uint8_t hops;
    bool to_me;  /* Private (not to everybody) */
    absolute_time_t at;
} inbox_t;

typedef struct {
    uint32_t origin;
    uint16_t uid;
} seen_t;

static inbox_t inbox[INBOX];  /* Newest first */
static int n_inbox = 0;
static seen_t seen[SEEN];
static int seen_next = 0;
static bool new_message = false;
static uint32_t n_relayed = 0;

static bool already_seen(uint32_t origin, uint16_t uid) {
    for (int i = 0; i < SEEN; ++i)
        if (seen[i].origin == origin && seen[i].uid == uid)
            return true;
    seen[seen_next] = (seen_t){origin, uid};
    seen_next = (seen_next + 1) % SEEN;
    return false;
}

static void handle_message(const net_packet_t *p) {
    if (p->len < PACKET_LEN)
        return;
    const uint8_t *d = p->data;
    uint16_t uid = d[0] | d[1] << 8;
    uint8_t ttl = d[2];
    uint32_t origin = net_u32(d + 3), recipient = net_u32(d + 7);
    if (origin == net_id() || already_seen(origin, uid))
        return;
    if ((recipient == 0 || recipient == net_id()) && d[19] < N_MESSAGES) {
        memmove(&inbox[1], &inbox[0], sizeof(inbox[0]) * (INBOX - 1));
        inbox[0] = (inbox_t){.from = origin, .message = d[19], .hops = MSG_TTL - ttl, .to_me = recipient != 0,
                             .at = p->at};
        memcpy(inbox[0].name, d + 11, 8);
        inbox[0].name[8] = 0;
        if (n_inbox < INBOX)
            ++n_inbox;
        new_message = true;
        printf("message: from %s#%04X: %s\n", inbox[0].name, (unsigned)(origin & 0xFFFF), MESSAGES[d[19]]);
    }
    /* Relay (also a private message for someone else), after a random delay: the neighbours relay too */
    if (ttl > 0 && recipient != net_id()) {
        uint8_t copy[PACKET_LEN];
        memcpy(copy, d, PACKET_LEN);
        copy[2] = ttl - 1;
        if (net_send(NET_MESSAGE, copy, PACKET_LEN, NET_LOUD | NET_JITTER))
            ++n_relayed;
    }
}

void messages_init(void) {
    net_subscribe(NET_MESSAGE, handle_message);
}

static uint8_t pending[PACKET_LEN];  /* The message written on this badge, sent several times */
static int sends_left = 0;
static absolute_time_t next_send = 0;

/* In the main loop: the copies of the message written on this badge */
void messages_task(absolute_time_t now) {
    if (sends_left && absolute_time_diff_us(next_send, now) >= 0 && net_send(NET_MESSAGE, pending, PACKET_LEN, NET_LOUD)) {
        --sends_left;
        next_send = delayed_by_ms(now, RESEND_MS);
    }
}

/* A message arrived (for the notification): its text */
bool messages_new(char *buf, int len) {
    if (! new_message)
        return false;
    new_message = false;
    snprintf(buf, len, "%s : %s", inbox[0].name, MESSAGES[inbox[0].message]);
    return true;
}

static void send_message(uint32_t recipient, int message) {
    uint8_t d[PACKET_LEN];
    uint16_t uid = get_rand_32();
    d[0] = uid;
    d[1] = uid >> 8;
    d[2] = MSG_TTL;
    net_put_u32(d + 3, net_id());
    net_put_u32(d + 7, recipient);
    memset(d + 11, 0, 8);
    memcpy(d + 11, social_name(), strnlen(social_name(), 8));
    d[19] = message;
    already_seen(net_id(), uid);
    /* Sent SENDS times, RESEND_MS apart (a badge may be listening to the OOK remotes): the uid removes the copies */
    memcpy(pending, d, PACKET_LEN);
    sends_left = SENDS;
    next_send = get_absolute_time();
    printf("message: sent \"%s\" to %08lX\n", MESSAGES[message], (unsigned long)recipient);
}


/* ------ The page: inbox, then write (choose the recipient and the message) ------ */

enum { V_INBOX, V_RECIPIENT, V_MESSAGE, V_SENT };

typedef struct {
    uint32_t id;
    char name[9];
} contact_t;

static int view = V_INBOX;
static int sel = 0;
static contact_t recipients[1 + SOCIAL_MAX_NEIGHBOURS + INBOX];  /* Everybody, the neighbours, the senders */
static int n_recipients = 0;
static int recipient = 0;
static const char EVERYBODY[] = "Tout le monde";

static void add_recipient(uint32_t id, const char *name) {
    for (int i = 0; i < n_recipients; ++i)
        if (recipients[i].id == id)
            return;
    recipients[n_recipients].id = id;
    snprintf(recipients[n_recipients].name, sizeof(recipients[0].name), "%s", name);
    ++n_recipients;
}

static void list_recipients(void) {
    n_recipients = 0;
    add_recipient(0, "");  /* Everybody: EVERYBODY on the screen */
    social_neighbour_t nb[SOCIAL_MAX_NEIGHBOURS];
    int n = social_neighbours(nb, SOCIAL_MAX_NEIGHBOURS);
    for (int i = 0; i < n; ++i)
        add_recipient(nb[i].id, nb[i].name);
    for (int i = 0; i < n_inbox; ++i)
        add_recipient(inbox[i].from, inbox[i].name);
}

static void inbox_label(int i, char *buf, size_t len) {
    if (i == 0) {
        snprintf(buf, len, "> Écrire un message");
        return;
    }
    const inbox_t *m = &inbox[i - 1];
    snprintf(buf, len, "%s%s: %s", m->to_me ? "(privé) " : "", m->name, MESSAGES[m->message]);
}

static void recipient_label(int i, char *buf, size_t len) {
    if (recipients[i].id)
        snprintf(buf, len, "%s#%04X", recipients[i].name, (unsigned)(recipients[i].id & 0xFFFF));
    else
        snprintf(buf, len, "%s", EVERYBODY);  /* Longer than a name of a badge (9 bytes) */
}

static void message_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s", MESSAGES[i]);
}

static void msg_start(absolute_time_t now) {
    (void)now;
    view = V_INBOX;
    sel = 0;
    new_message = false;
}

static int count(void) {
    return view == V_INBOX ? n_inbox + 1 : view == V_RECIPIENT ? n_recipients : view == V_MESSAGE ? N_MESSAGES : 1;
}

static bool msg_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_A) {
        if (view == V_INBOX)
            return false;
        view = view == V_MESSAGE ? V_RECIPIENT : V_INBOX;
        sel = 0;
        return true;
    }
    int n = count();
    if (b->pressed & UI_BTN_Y)
        sel = (sel + n - 1) % n;
    if (b->pressed & UI_BTN_X)
        sel = (sel + 1) % n;
    if (b->pressed & UI_BTN_B) {
        switch (view) {
        case V_INBOX:
            list_recipients();
            if (sel > 0) {
                /* Answer the sender of this message */
                for (int i = 0; i < n_recipients; ++i)
                    if (recipients[i].id == inbox[sel - 1].from)
                        recipient = i;
                view = V_MESSAGE;
            } else {
                view = V_RECIPIENT;
            }
            sel = 0;
            break;
        case V_RECIPIENT:
            recipient = sel;
            view = V_MESSAGE;
            sel = 0;
            break;
        case V_MESSAGE:
            send_message(recipients[recipient].id, sel);
            view = V_SENT;
            break;
        default:
            view = V_INBOX;
            sel = 0;
            break;
        }
    }
    return true;
}

static bool msg_task(absolute_time_t now) {
    (void)now;
    if (view == V_INBOX && new_message) {
        new_message = false;
        return true;
    }
    return false;
}

static void msg_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    char text[48];
    switch (view) {
    case V_INBOX:
        snprintf(text, sizeof(text), "Messages (%d)", n_inbox);
        ui_title(fb, text);
        ui_list(fb, n_inbox + 1, sel, inbox_label);
        ui_footer(fb, sel ? "G : retour  D : répondre" : "G : retour  D : écrire");
        break;
    case V_RECIPIENT:
        ui_title(fb, "A qui ?");
        ui_list(fb, n_recipients, sel, recipient_label);
        ui_footer(fb, "G : retour  D : choisir");
        break;
    case V_MESSAGE:
        recipient_label(recipient, text, sizeof(text));
        ui_title(fb, text);
        ui_list(fb, N_MESSAGES, sel, message_label);
        ui_footer(fb, "G : retour  D : envoyer");
        break;
    default:
        ui_title(fb, "Message envoyé");
        ui_lines(fb, 60, &gfx_font_small, "Les cigales le relaient\njusqu'à 3 fois pour qu'il\narrive à destination.");
        ui_footer(fb, "D : boîte de réception");
        break;
    }
}

const app_t app_messages = {
    .name = "Messages",
    .start = msg_start,
    .buttons = msg_buttons,
    .task = msg_task,
    .render = msg_render,
};
