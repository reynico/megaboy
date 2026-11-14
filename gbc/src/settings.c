#include "settings.h"

#include <gb/gb.h>

#include "app.h"
#include "fmt.h"
#include "hdr.h"
#include "link.h"
#include "save.h"
#include "screen.h"
#include "tiles.h"
#include "ui.h"

enum { K_TEMP, K_X10, K_INT, K_ONOFF, K_ECU, K_CLOCK };

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
    {"CLOCK", 0, 0, 0, 0, K_CLOCK},
};
#define ITEM_COUNT (sizeof items / sizeof items[0])
#define VISIBLE 8u
#define ITEM_Y0 2u
#define VAL_X 12u
#define VAL_W 6u
#define MARK_X 19u
#define FOOTER_Y 17u

static uint8_t sel;
static uint8_t top;
static uint8_t editing;
static int16_t edit_val;

static uint8_t item_y(uint8_t i) {
    return (uint8_t)(ITEM_Y0 + (i - top) * 2u);
}

static uint8_t visible(uint8_t i) {
    return i >= top && i < top + VISIBLE;
}

static void draw_clock(uint8_t y) {
    char s[VAL_W];
    if (app.clk_state == LINK_CLK_NONE) {
        ui_text(VAL_X, y, "NO RTC", PAL_DIM);
        return;
    }
    if (app.clk_state != LINK_CLK_OK) {
        ui_text(VAL_X, y, " UNSET", PAL_AMBER);
        return;
    }
    s[0] = ' ';
    s[1] = (char)('0' + app.clk.hh / 10u);
    s[2] = (char)('0' + app.clk.hh % 10u);
    s[3] = ':';
    s[4] = (char)('0' + app.clk.mi / 10u);
    s[5] = (char)('0' + app.clk.mi % 10u);
    ui_textn(VAL_X, y, s, VAL_W, PAL_TEXT);
}

static void draw_value(uint8_t i) {
    const item_t *it = &items[i];
    char s[VAL_W];
    uint8_t live = editing && i == sel;
    uint8_t pal = live ? PAL_AMBER : PAL_TEXT;
    uint8_t y = item_y(i);
    int16_t v;
    if (!visible(i)) {
        return;
    }
    if (it->kind == K_CLOCK) {
        draw_clock(y);
        return;
    }
    v = live ? edit_val : *it->val;
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
        ui_text(VAL_X, y, v ? "    ON" : "   OFF", pal);
        return;
    case K_ECU:
        ui_text(VAL_X, y, v == 1 ? "   MS1" : (v == 2 ? "   MS2" : "   ---"), pal);
        return;
    default:
        return;
    }
    ui_textn(VAL_X, y, s, VAL_W, pal);
}

static void draw_cursor(void) {
    uint8_t i;
    for (i = top; i < top + VISIBLE && i < ITEM_COUNT; i++) {
        uint8_t t = T_SPACE;
        if (i == sel) {
            t = tile_small(editing ? '*' : '>');
        }
        ui_tile(0, item_y(i), t, PAL_AMBER);
    }
}

static void draw_list(void) {
    uint8_t i;
    uint8_t y;
    for (y = ITEM_Y0; y < ITEM_Y0 + VISIBLE * 2u; y += 2u) {
        ui_text(1, y, "                  ", PAL_TEXT);
    }
    for (i = top; i < top + VISIBLE && i < ITEM_COUNT; i++) {
        ui_text(1, item_y(i), items[i].label, PAL_TEXT);
        draw_value(i);
    }
    ui_tile(MARK_X, ITEM_Y0, top ? tile_small(CH_UP) : T_SPACE, PAL_DIM);
    ui_tile(MARK_X, (uint8_t)(ITEM_Y0 + (VISIBLE - 1u) * 2u), top + VISIBLE < ITEM_COUNT ? tile_small(CH_DOWN) : T_SPACE,
            PAL_DIM);
    draw_cursor();
}

static void draw_footer(void) {
    ui_text(1, FOOTER_Y, editing ? "A:OK  B:CANCEL" : "A:EDIT  B:BACK", PAL_DIM);
}

static void sel_item(uint8_t i) {
    uint8_t old = top;
    sel = i;
    if (sel < top) {
        top = sel;
    } else if (sel >= top + VISIBLE) {
        top = (uint8_t)(sel - VISIBLE + 1u);
    }
    if (top != old) {
        draw_list();
    } else {
        draw_cursor();
    }
}

void settings_clamp(void) {
    uint8_t i;
    for (i = 0; i < ITEM_COUNT; i++) {
        int16_t *v = items[i].val;
        if (!v) {
            continue;
        }
        if (*v < items[i].min) {
            *v = items[i].min;
        }
        if (*v > items[i].max) {
            *v = items[i].max;
        }
    }
}

void settings_enter(void) {
    editing = 0;
    ui_clear();
    hdr_draw("SETTINGS");
    draw_list();
    draw_footer();
}

void settings_tick(void) {
    uint8_t last = ITEM_COUNT - 1u;
    hdr_tick();
    if (items[last].kind == K_CLOCK) {
        draw_value(last);
    }
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
        sel_item((uint8_t)((sel + ITEM_COUNT - 1u) % ITEM_COUNT));
    } else if (pressed & J_DOWN) {
        sel_item((uint8_t)((sel + 1u) % ITEM_COUNT));
    } else if (pressed & J_A) {
        if (items[sel].kind == K_CLOCK) {
            if (app.src == SRC_LINK && app.clk_state != LINK_CLK_NONE) {
                screen_go(SCR_CLOCK);
            }
            return;
        }
        editing = 1;
        edit_val = *items[sel].val;
        draw_value(sel);
        draw_cursor();
        draw_footer();
    } else if (pressed & J_B) {
        screen_go(SCR_MENU);
    }
}
