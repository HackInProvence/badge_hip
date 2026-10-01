/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>
#include <string.h>

#include "vcard.h"

enum { F_FIRST, F_NAME, F_TEL, F_EMAIL, F_ORG, F_TITLE, F_ADDRESS, F_CITY, F_LINKEDIN, F_GIT, F_WEB, F_MASTODON,
       F_NOTE };

/* The simple properties: "PROP:value" (the name, the address are special) */
static const struct {
    int field;
    const char *prop;
} SIMPLE[] = {
    {F_TEL, "TEL"}, {F_EMAIL, "EMAIL"}, {F_ORG, "ORG"}, {F_TITLE, "TITLE"}, {F_LINKEDIN, "URL;TYPE=linkedin"},
    {F_GIT, "URL;TYPE=git"}, {F_WEB, "URL"}, {F_MASTODON, "X-MASTODON"}, {F_NOTE, "NOTE"},
};
#define N_SIMPLE ((int)(sizeof(SIMPLE) / sizeof(SIMPLE[0])))

#define CHECK_PROP "X-SECSEA-CHECK"


uint16_t vcard_crc16(const char *s, int len, uint16_t crc) {
    /* CRC-16/CCITT-FALSE: polynomial 0x1021 (start with 0xFFFF) */
    for (int i = 0; i < len; ++i) {
        crc ^= (uint16_t)(uint8_t)s[i] << 8;
        for (int b = 0; b < 8; ++b)
            crc = crc & 0x8000 ? (uint16_t)(crc << 1) ^ 0x1021 : (uint16_t)(crc << 1);
    }
    return crc;
}


/* ------ Sending ------ */

typedef struct {
    vcard_packet_t *packets;
    int max, n;
    bool overflow;
    int lines;
    uint16_t crc;
} builder_t;

/* A line: in packets of VCARD_PACKET_MAX bytes with "\r\n", the next ones start with a space (folding) */
static void add_line(builder_t *b, const char *line, bool checked) {
    int len = strlen(line), done = 0;
    if (checked) {
        if (b->lines)
            b->crc = vcard_crc16("\n", 1, b->crc);
        b->crc = vcard_crc16(line, len, b->crc);
        ++b->lines;
    }
    while (done < len || (done == 0 && len == 0)) {
        if (b->n == b->max) {
            b->overflow = true;
            return;
        }
        vcard_packet_t *p = &b->packets[b->n++];
        int k = 0;
        if (done)
            p->text[k++] = ' ';
        int room = VCARD_PACKET_MAX - 2 - k;
        int m = len - done < room ? len - done : room;
        memcpy(p->text + k, line + done, m);
        k += m;
        done += m;
        p->text[k++] = '\r';
        p->text[k++] = '\n';
        p->text[k] = 0;
        p->len = k;
        if (len == 0)
            break;
    }
}

int vcard_build(const char *const values[VCARD_FIELDS], uint16_t mask, const char *categories,
                vcard_packet_t *packets, int max) {
    builder_t b = {packets, max, 0, false, 0, 0xFFFF};
    char line[VCARD_LINE_MAX];
    const char *v[VCARD_FIELDS];
    for (int f = 0; f < VCARD_FIELDS; ++f)
        v[f] = (mask & (1u << f)) && values[f] ? values[f] : "";
    add_line(&b, "BEGIN:VCARD", true);
    add_line(&b, "VERSION:3.0", true);
    snprintf(line, sizeof(line), "N:%s;%s;;;", v[F_NAME], v[F_FIRST]);
    add_line(&b, line, true);
    snprintf(line, sizeof(line), "FN:%s%s%s", v[F_FIRST], v[F_FIRST][0] && v[F_NAME][0] ? " " : "", v[F_NAME]);
    add_line(&b, line, true);
    if (v[F_ADDRESS][0] || v[F_CITY][0]) {
        snprintf(line, sizeof(line), "ADR:;;%s;%s;;;", v[F_ADDRESS], v[F_CITY]);
        add_line(&b, line, true);
    }
    for (int i = 0; i < N_SIMPLE; ++i)
        if (v[SIMPLE[i].field][0]) {
            snprintf(line, sizeof(line), "%s:%s", SIMPLE[i].prop, v[SIMPLE[i].field]);
            add_line(&b, line, true);
        }
    if (categories && categories[0]) {
        snprintf(line, sizeof(line), "CATEGORIES:%s", categories);
        add_line(&b, line, true);
    }
    snprintf(line, sizeof(line), CHECK_PROP ":%d-%04X", b.lines, b.crc);
    add_line(&b, line, false);
    add_line(&b, "END:VCARD", false);
    return b.overflow ? 0 : b.n;
}


/* ------ Receiving ------ */

void vcard_rx_init(vcard_rx_t *rx) {
    memset(rx, 0, sizeof(*rx));
}

/* "PROP:..." or "PROP;PARAM...:..." */
static bool is_property(const char *s) {
    int i = 0;
    while ((s[i] >= 'A' && s[i] <= 'Z') || s[i] == '-' || (s[i] >= '0' && s[i] <= '9'))
        ++i;
    return i > 0 && (s[i] == ':' || s[i] == ';');
}

/* Removes the color codes of the terminal ("\x1b[0;33m") and the name of a Flipper ("Ardyaro: TEL:...") */
static void clean(const char *in, int len, char *out, int size) {
    int k = 0;
    for (int i = 0; i < len && k < size - 1; ++i) {
        if (in[i] == 0x1B) {
            while (i < len && ! ((in[i] >= 'A' && in[i] <= 'Z') || (in[i] >= 'a' && in[i] <= 'z')))
                ++i;
            continue;
        }
        if (in[i] == '\r' || in[i] == '\n')
            continue;
        out[k++] = in[i];
    }
    out[k] = 0;
    if (out[0] != ' ' && ! is_property(out)) {
        const char *sep = strstr(out, ": ");
        if (sep && is_property(sep + 2))
            memmove(out, sep + 2, strlen(sep + 2) + 1);
    }
}

static void copy_value(char *dst, const char *src, int n) {
    if (n < 0)
        n = strlen(src);
    if (n > VCARD_VALUE_MAX)
        n = VCARD_VALUE_MAX;
    memcpy(dst, src, n);
    dst[n] = 0;
}

/* The n-th component (0...) of a structured value "a;b;c" */
static void component(const char *value, int n, char *dst) {
    const char *s = value;
    for (int i = 0; i < n && s; ++i) {
        s = strchr(s, ';');
        if (s)
            ++s;
    }
    if (! s) {
        dst[0] = 0;
        return;
    }
    const char *e = strchr(s, ';');
    copy_value(dst, s, e ? (int)(e - s) : -1);
}

static void parse_card(vcard_rx_t *rx) {
    char (*v)[VCARD_VALUE_MAX + 1] = rx->values;
    memset(rx->values, 0, sizeof(rx->values));
    rx->categories[0] = 0;
    bool have_n = false;
    for (int i = 0; i < rx->n; ++i) {
        const char *line = rx->lines[i];
        const char *colon = strchr(line, ':');
        if (! colon)
            continue;
        int name_len = colon - line;
        const char *value = colon + 1;
        char name[40];
        snprintf(name, sizeof(name), "%.*s", name_len, line);
        /* The property without its parameters, and its TYPE */
        char base[24];
        snprintf(base, sizeof(base), "%.*s", (int)strcspn(name, ";"), name);
        const char *type = strstr(name, "TYPE=");
        if (! strcmp(base, "N")) {
            component(value, 0, v[F_NAME]);
            component(value, 1, v[F_FIRST]);
            have_n = v[F_NAME][0] || v[F_FIRST][0];
        } else if (! strcmp(base, "FN") && ! have_n) {
            const char *sp = strchr(value, ' ');
            if (sp) {
                copy_value(v[F_FIRST], value, sp - value);
                copy_value(v[F_NAME], sp + 1, -1);
            } else {
                copy_value(v[F_NAME], value, -1);
            }
        } else if (! strcmp(base, "CATEGORIES")) {
            snprintf(rx->categories, sizeof(rx->categories), "%s", value);
        } else if (! strcmp(base, "ADR")) {
            component(value, 2, v[F_ADDRESS]);
            component(value, 3, v[F_CITY]);
        } else if (! strcmp(base, "URL")) {
            int f = type && ! strncmp(type + 5, "linkedin", 8) ? F_LINKEDIN : type && ! strncmp(type + 5, "git", 3) ? F_GIT
                                                                                                                 : F_WEB;
            copy_value(v[f], value, -1);
        } else {
            for (int k = 0; k < N_SIMPLE; ++k)
                if (! strchr(SIMPLE[k].prop, ';') && ! strcmp(base, SIMPLE[k].prop))
                    copy_value(v[SIMPLE[k].field], value, -1);
        }
    }
}

/* A complete line of the card being received */
static bool end_line(vcard_rx_t *rx) {
    char *line = rx->lines[rx->n - 1];
    if (strcmp(line, "END:VCARD"))
        return false;
    rx->in_card = false;
    /* The check line, when there is one: the number of lines before it and their CRC */
    int lines = rx->n - 1;
    bool checked = false;
    if (lines >= 1 && ! strncmp(rx->lines[lines - 1], CHECK_PROP ":", strlen(CHECK_PROP) + 1)) {
        int want_lines = 0;
        unsigned want_crc = 0;
        if (sscanf(rx->lines[lines - 1] + strlen(CHECK_PROP) + 1, "%d-%x", &want_lines, &want_crc) != 2)
            return false;
        --lines;
        uint16_t crc = 0xFFFF;
        for (int i = 0; i < lines; ++i) {
            if (i)
                crc = vcard_crc16("\n", 1, crc);
            crc = vcard_crc16(rx->lines[i], strlen(rx->lines[i]), crc);
        }
        if (want_lines != lines || want_crc != crc)
            return false;  /* A line lost or mixed with another card: the next sending */
        checked = true;
    }
    rx->n = lines;
    parse_card(rx);
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < lines; ++i)
        crc = vcard_crc16(rx->lines[i], strlen(rx->lines[i]), crc);
    rx->crc = crc;
    (void)checked;
    return true;
}

bool vcard_rx_packet(vcard_rx_t *rx, const char *text, int len) {
    char s[VCARD_PACKET_MAX + 8];
    clean(text, len, s, sizeof(s));
    if (! strcmp(s, "BEGIN:VCARD")) {
        rx->in_card = true;
        rx->n = 0;
    }
    if (! rx->in_card || ! s[0])
        return false;
    if (s[0] == ' ' && rx->n) {
        /* Folded: the continuation of the previous line */
        char *line = rx->lines[rx->n - 1];
        int l = strlen(line);
        snprintf(line + l, VCARD_LINE_MAX - l, "%s", s + 1);
        return false;
    }
    /* The previous line is complete: was it the end? (END:VCARD is never folded, it is checked here) */
    if (rx->n == VCARD_LINES_MAX) {
        rx->in_card = false;  /* Too long: not a card of ours */
        return false;
    }
    snprintf(rx->lines[rx->n++], VCARD_LINE_MAX, "%s", s);
    return end_line(rx);
}
