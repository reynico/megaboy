#include "menu.h"

#include <gb/gb.h>

#include "hdr.h"
#include "screen.h"
#include "tiles.h"
#include "ui.h"

typedef struct {
    const char *label;
    uint8_t target;
} item_t;

static const item_t items[] = {
    {"DATALOG", SCR_LOG},
    {"SETTINGS", SCR_SETTINGS},
    {"TUNE", SCR_TUNE},
    {"ABOUT", SCR_ABOUT},
};
#define ITEM_COUNT (sizeof items / sizeof items[0])
#define ITEM_Y0 3u

static uint8_t sel;

static void draw_cursor(void) {
    uint8_t i;
    for (i = 0; i < ITEM_COUNT; i++) {
        ui_tile(1, (uint8_t)(ITEM_Y0 + i * 2u), i == sel ? tile_small('>') : T_SPACE, PAL_AMBER);
    }
}

void menu_enter(void) {
    uint8_t i;
    ui_clear();
    hdr_draw("MENU");
    for (i = 0; i < ITEM_COUNT; i++) {
        ui_text(3, (uint8_t)(ITEM_Y0 + i * 2u), items[i].label, PAL_TEXT);
    }
    ui_divider(15);
    ui_text(1, 16, "A:OK  B:BACK", PAL_DIM);
    draw_cursor();
}

void menu_tick(void) {
    hdr_tick();
}

void menu_input(uint8_t pressed) {
    if (pressed & J_UP) {
        sel = (uint8_t)((sel + ITEM_COUNT - 1u) % ITEM_COUNT);
        draw_cursor();
    } else if (pressed & J_DOWN) {
        sel = (uint8_t)((sel + 1u) % ITEM_COUNT);
        draw_cursor();
    } else if (pressed & J_A) {
        screen_go(items[sel].target);
    } else if (pressed & (J_B | J_START)) {
        screen_back();
    }
}
