/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The skills (skills.h) and their page: Social > Compétences. */

#include <stdio.h>
#include <string.h>

#include "achievements.h"
#include "app.h"
#include "i18n.h"
#include "skills.h"
#include "social.h"
#include "store.h"

/* Same order as the drawings of tools/skills_icons.py */
static const char *const SKILLS[SKILLS_COUNT] = {
    N_("Électronique"), "Flipper Zero", "Android", "iOS", "Radio / SDR", "Web", N_("Réseau"), "Crypto", "Reverse",
    "Pentest", "Forensic", "OSINT", "Linux", "Windows", "Cloud", N_("IA"), N_("Développement"), "CTF",
    "Lockpicking", N_("Défense"),
};
_Static_assert(SKILLS_COUNT <= 32, "the skills are a 32 bit mask");

const char *skills_name(int i) {
    return i >= 0 && i < SKILLS_COUNT ? SKILLS[i] : "?";
}

int skills_count(uint32_t mask) {
    int n = 0;
    for (int i = 0; i < SKILLS_COUNT; ++i)
        n += mask >> i & 1;
    return n;
}

void skills_draw_icon(uint8_t *fb, int x, int y, int i, gfx_color_t color) {
    if (i < 0 || i >= SKILLS_COUNT)
        return;
    for (int r = 0; r < SKILLS_ICON_SIZE; ++r)
        for (int c = 0; c < SKILLS_ICON_SIZE; ++c)
            if (SKILLS_ICON_BITS[i][r] & (0x8000 >> c))
                gfx_pixel(fb, x + c, y + r, color);
}

int skills_draw_row(uint8_t *fb, int cx, int y, uint32_t mask, int max, gfx_color_t color) {
    const int step = SKILLS_ICON_SIZE + 3;
    int n = skills_count(mask);
    if (n > max)
        n = max;
    int x = cx - (n * step - 3) / 2, k = 0;
    for (int i = 0; i < SKILLS_COUNT && k < n; ++i)
        if (mask >> i & 1)
            skills_draw_icon(fb, x + step * k++, y, i, color);
    return n;
}

void skills_to_text(uint32_t mask, char *buf, int len) {
    int n = 0;
    buf[0] = 0;
    for (int i = 0; i < SKILLS_COUNT; ++i)
        if (mask >> i & 1 && n < len)
            n += snprintf(buf + n, len - n, "%s%s", n ? "," : "", SKILLS[i]);
}

/* Lower case, without the accents of the French letters (UTF-8) and without spaces: "Électronique" -> "electronique" */
static void fold(const char *s, int n, char *out, int size) {
    int k = 0;
    for (int i = 0; i < n && s[i] && k < size - 1; ++i) {
        unsigned char c = s[i];
        if (c == 0xC3 && i + 1 < n) {
            unsigned char d = (unsigned char)s[++i] & ~0x20;  /* Upper and lower case together */
            c = d >= 0x80 && d <= 0x85 ? 'a' : d == 0x87 ? 'c' : d >= 0x88 && d <= 0x8B ? 'e'
              : d >= 0x8C && d <= 0x8F ? 'i' : d >= 0x92 && d <= 0x96 ? 'o' : d >= 0x99 && d <= 0x9C ? 'u' : '?';
        } else if (c >= 'A' && c <= 'Z') {
            c += 'a' - 'A';
        } else if (c == ' ' || c == '/' || c == '-' || c == '.') {
            continue;
        }
        out[k++] = c;
    }
    out[k] = 0;
}

uint32_t skills_from_text(const char *text) {
    uint32_t mask = 0;
    const char *s = text;
    while (*s) {
        int n = strcspn(s, ",;");
        char word[32], name[32];
        fold(s, n, word, sizeof(word));
        for (int i = 0; i < SKILLS_COUNT && word[0]; ++i) {
            fold(SKILLS[i], strlen(SKILLS[i]), name, sizeof(name));
            if (! strcmp(word, name))
                mask |= 1u << i;
        }
        s += n;
        if (*s)
            ++s;
    }
    return mask;
}


/* ------ The page ------ */

enum { V_MAIN, V_MINE, V_NEAR };
static int view = V_MAIN;
static int sel = 0;
static social_neighbour_t near[SOCIAL_MAX_NEIGHBOURS];
static int n_near = 0;
static absolute_time_t refresh_ts = 0;

static void find_near(void) {
    social_neighbour_t all[SOCIAL_MAX_NEIGHBOURS];
    int n = social_neighbours(all, SOCIAL_MAX_NEIGHBOURS);
    uint32_t mine = store_get()->skills;
    n_near = 0;
    for (int i = 0; i < n; ++i)
        if (all[i].skills & mine)
            near[n_near++] = all[i];
}

static void skills_start(absolute_time_t now) {
    view = V_MAIN;
    sel = 0;
    refresh_ts = now;
}

static bool skills_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    int count = view == V_MAIN ? 2 : view == V_MINE ? SKILLS_COUNT : n_near;
    if (count) {
        if (b->pressed & UI_BTN_X)
            sel = (sel + 1) % count;
        if (b->pressed & UI_BTN_Y)
            sel = (sel + count - 1) % count;
    }
    if (b->pressed & UI_BTN_A) {
        if (view == V_MAIN)
            return false;
        sel = view == V_MINE ? 0 : 1;
        view = V_MAIN;
        return true;
    }
    if (! (b->pressed & UI_BTN_B))
        return true;
    if (view == V_MAIN) {
        view = sel ? V_NEAR : V_MINE;
        sel = 0;
        if (view == V_NEAR)
            find_near();
    } else if (view == V_MINE) {
        store_t *s = store_get();
        s->skills ^= 1u << sel;
        store_changed();
        printf("skills: %s %s\n", SKILLS[sel], s->skills >> sel & 1 ? "on" : "off");
        if (s->skills)
            achv_unlock(ACHV_SKILLS);
    }
    return true;
}

static bool skills_task(absolute_time_t now) {
    if (view != V_NEAR || absolute_time_diff_us(refresh_ts, now) < 0)
        return false;
    refresh_ts = delayed_by_ms(now, 3000);
    int before = n_near;
    find_near();
    if (sel >= n_near)
        sel = n_near ? n_near - 1 : 0;
    return n_near || before;
}

static void main_label(int i, char *buf, size_t len) {
    if (i == 0)
        snprintf(buf, len, _("Mes compétences (%d)"), skills_count(store_get()->skills));
    else
        snprintf(buf, len, N_("Qui les partage ?"));
}

static void skills_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    if (view == V_MAIN) {
        ui_title(fb, N_("Compétences"));
        ui_list(fb, 2, sel, main_label);
        skills_draw_row(fb, GFX_WIDTH/2, UI_FOOTER_Y - 24, store_get()->skills, 9, GFX_BLACK);
        ui_footer(fb, N_("D : ouvrir  G : retour"));
        return;
    }
    const int rows = 7, row_h = UI_ROW_H, y0 = UI_TITLE_H + 3;
    int count = view == V_MINE ? SKILLS_COUNT : n_near;
    ui_title(fb, view == V_MINE ? N_("Mes compétences") : N_("Qui les partage ?"));
    if (view == V_NEAR && ! n_near) {
        ui_lines(fb, 60, &gfx_font_small, store_get()->skills ? N_("Aucune cigale proche\nne partage vos\ncompétences.")
                                                              : N_("Cochez d'abord vos\ncompétences."));
        ui_footer(fb, N_("G : retour"));
        return;
    }
    int first = sel - rows / 2;
    if (first > count - rows)
        first = count - rows;
    if (first < 0)
        first = 0;
    uint32_t mine = store_get()->skills;
    for (int i = first; i < first + rows && i < count; ++i) {
        int y = y0 + (i - first) * row_h;
        bool on = i == sel;
        gfx_color_t fg = on ? GFX_WHITE : GFX_BLACK;
        if (on)
            gfx_fill_rect(fb, 2, y, GFX_WIDTH - 4, row_h - 1, GFX_BLACK);
        if (view == V_MINE) {
            gfx_rect(fb, 6, y + 5, 10, 10, fg);  /* The check box */
            if (mine >> i & 1)
                gfx_fill_rect(fb, 8, y + 7, 6, 6, fg);
            skills_draw_icon(fb, 22, y + 2, i, fg);
            gfx_text(fb, 44, y + 1, &gfx_font_small, SKILLS[i], fg, GFX_ALIGN_LEFT);
        } else {
            char name[12];
            snprintf(name, sizeof(name), "%s", near[i].name);
            gfx_text(fb, 6, y + 1, &gfx_font_small, name, fg, GFX_ALIGN_LEFT);
            skills_draw_row(fb, 150, y + 2, near[i].skills & mine, 4, fg);
        }
    }
    ui_footer(fb, view == V_MINE ? N_("D : cocher  G : retour") : N_("G : retour"));
}

const app_t app_skills = {
    .name = N_("Compétences"),
    .start = skills_start,
    .buttons = skills_buttons,
    .task = skills_task,
    .render = skills_render,
};
