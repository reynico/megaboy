#include "settings.h"

#include <gb/gb.h>

#include "app.h"
#include "fmt.h"
#include "hdr.h"
#include "save.h"
#include "screen.h"
#include "tiles.h"
#include "ui.h"

enum { K_TEMP, K_X10, K_INT, K_ONOFF, K_ECU };

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
    {"ALARM SND", &app.set.alarm_snd, 0, 1, 1, K_ONOFF},
    {"KEY SND", &app.set.key_snd, 0, 1, 1, K_ONOFF},
    {"ECU", &app.set.ecu, 0, 2, 1, K_ECU},
};
#define ITEM_COUNT (sizeof items / sizeof items[0])
#define ITEM_Y0 2u
#define VAL_X 13u
#define VAL_W 6u
#define FOOTER_Y 17u

static uint8_t sel;
static uint8_t editing;
static int16_t edit_val;

static uint8_t item_y(uint8_t i) {
    return (uint8_t)(ITEM_Y0 + i * 2u);
}

static void draw_value(uint8_t i) {
    const item_t *it = &items[i];
    char s[VAL_W];
    uint8_t live = editing && i == sel;
    uint8_t pal = live ? PAL_AMBER : PAL_TEXT;
    int16_t v = live ? edit_val : *it->val;
    switch (it->kind) {
    case K_TEMP:
        fmt_int(s, VAL_W, v, 'C');
        break;
    case K_X10:
        fmt_x10(s, VAL_W, v, 0);
        break;
    case K_INT:
        fmt_int(s, VAL_W, v, 0);
        break;
    case K_ONOFF:
        ui_text(VAL_X, item_y(i), v ? "    ON" : "   OFF", pal);
        return;
    case K_ECU:
        ui_text(VAL_X, item_y(i), v == 1 ? "   MS1" : (v == 2 ? "   MS2" : "   ---"), pal);
        return;
    default:
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
    ui_text(1, FOOTER_Y, editing ? "A:OK  B:CANCEL" : "A:EDIT  B:BACK", PAL_DIM);
}

void settings_clamp(void) {
    uint8_t i;
    for (i = 0; i < ITEM_COUNT; i++) {
        int16_t *v = items[i].val;
        if (*v < items[i].min) {
            *v = items[i].min;
        }
        if (*v > items[i].max) {
            *v = items[i].max;
        }
    }
}

void settings_enter(void) {
    uint8_t i;
    editing = 0;
    ui_clear();
    hdr_draw("SETTINGS");
    for (i = 0; i < ITEM_COUNT; i++) {
        ui_text(1, item_y(i), items[i].label, PAL_TEXT);
        draw_value(i);
    }
    draw_footer();
    draw_cursor();
}

void settings_tick(void) {
    hdr_tick();
}

static void edit_input(uint8_t pressed) {
    const item_t *it = &items[sel];
    int16_t v = edit_val;
    if (pressed & J_UP) {
        v = (int16_t)(v + it->step);
    } else if (pressed & J_DOWN) {
        v = (int16_t)(v - it->step);
    } else if (pressed & J_A) {
        editing = 0;
        if (v != *it->val) {
            *it->val = v;
            save_store(&app.set);
        }
    } else if (pressed & J_B) {
        editing = 0;
    }
    if (v < it->min) {
        v = it->min;
    }
    if (v > it->max) {
        v = it->max;
    }
    edit_val = v;
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
    } else if (pressed & J_A) {
        editing = 1;
        edit_val = *items[sel].val;
        draw_value(sel);
        draw_cursor();
        draw_footer();
    } else if (pressed & J_B) {
        screen_go(SCR_MENU);
    }
}
