#include "settings.h"

#include <gb/gb.h>

#include "app.h"
#include "fmt.h"
#include "hdr.h"
#include "screen.h"
#include "tiles.h"
#include "ui.h"

enum { K_TEMP, K_X10, K_INT, K_ONOFF, K_RTC };

typedef struct {
    const char *label;
    int16_t *val;
    int16_t min;
    int16_t max;
    int16_t step;
    uint8_t kind;
} item_t;

static const item_t items[] = {
    {"CLT WARN", &app.set.clt_warn_c, 80, 120, 1, K_TEMP},
    {"AFR LEAN", &app.set.afr_lean_x10, 120, 160, 1, K_X10},
    {"SHIFT RPM", &app.set.rpm_shift, 4000, 8000, 100, K_INT},
    {"BOOST WRN", &app.set.boost_warn_x10, 50, 300, 5, K_X10},
    {"AUTO LOG", &app.set.auto_log, 0, 1, 1, K_ONOFF},
    {"CLOCK", 0, 0, 0, 0, K_RTC},
};
#define ITEM_COUNT (sizeof items / sizeof items[0])
#define ITEM_Y0 3u
#define VAL_X 13u
#define VAL_W 6u

static uint8_t sel;
static uint8_t editing;
static int16_t saved;

static uint8_t item_y(uint8_t i) {
    return (uint8_t)(ITEM_Y0 + i * 2u);
}

static void draw_value(uint8_t i) {
    const item_t *it = &items[i];
    char s[VAL_W];
    uint8_t pal = (editing && i == sel) ? PAL_AMBER : PAL_TEXT;
    switch (it->kind) {
    case K_TEMP:
        fmt_int(s, VAL_W, *it->val, 'C');
        break;
    case K_X10:
        fmt_x10(s, VAL_W, *it->val, 0);
        break;
    case K_INT:
        fmt_int(s, VAL_W, *it->val, 0);
        break;
    case K_ONOFF:
        ui_text(VAL_X, item_y(i), *it->val ? "    ON" : "   OFF", pal);
        return;
    default:
        ui_text(VAL_X, item_y(i), "NO RTC", PAL_DIM);
        return;
    }
    ui_textn(VAL_X, item_y(i), s, VAL_W, pal);
}

static void draw_cursor(void) {
    uint8_t i;
    for (i = 0; i < ITEM_COUNT; i++) {
        uint8_t t = T_SPACE;
        if (i == sel) {
            t = tile_small(editing ? '*' : '>');
        }
        ui_tile(0, item_y(i), t, PAL_AMBER);
    }
}

static void draw_footer(void) {
    ui_text(1, 16, editing ? "A:OK  B:CANCEL" : "A:EDIT  B:BACK", PAL_DIM);
}

void settings_enter(void) {
    uint8_t i;
    editing = 0;
    ui_clear();
    hdr_draw("SETTINGS");
    for (i = 0; i < ITEM_COUNT; i++) {
        ui_text(1, item_y(i), items[i].label, items[i].kind == K_RTC ? PAL_DIM : PAL_TEXT);
        draw_value(i);
    }
    ui_divider(15);
    draw_footer();
    draw_cursor();
}

void settings_tick(void) {
    hdr_tick();
}

static void edit_input(uint8_t pressed) {
    const item_t *it = &items[sel];
    int16_t v = *it->val;
    if (pressed & J_UP) {
        v = (int16_t)(v + it->step);
    } else if (pressed & J_DOWN) {
        v = (int16_t)(v - it->step);
    } else if (pressed & J_A) {
        editing = 0;
    } else if (pressed & J_B) {
        v = saved;
        editing = 0;
    }
    if (v < it->min) {
        v = it->min;
    }
    if (v > it->max) {
        v = it->max;
    }
    *it->val = v;
    draw_value(sel);
    draw_cursor();
    draw_footer();
}

void settings_input(uint8_t pressed) {
    if (editing) {
        edit_input(pressed);
        return;
    }
    if (pressed & J_UP) {
        sel = (uint8_t)((sel + ITEM_COUNT - 1u) % ITEM_COUNT);
        draw_cursor();
    } else if (pressed & J_DOWN) {
        sel = (uint8_t)((sel + 1u) % ITEM_COUNT);
        draw_cursor();
    } else if ((pressed & J_A) && items[sel].kind != K_RTC) {
        editing = 1;
        saved = *items[sel].val;
        draw_value(sel);
        draw_cursor();
        draw_footer();
    } else if (pressed & J_B) {
        screen_go(SCR_MENU);
    }
}
