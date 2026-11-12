#include "tunemenu.h"

#include <gb/gb.h>

#include "app.h"
#include "hdr.h"
#include "link.h"
#include "screen.h"
#include "tdata.h"
#include "tiles.h"
#include "ui.h"

static const char *const names[TUNE_COUNT] = {"VE TABLE 1", "SPARK TABLE 1", "AFR TARGET 1", "REV LIMITS", "BOOST TARGET", "BOOST DUTY"};
#define ITEM_Y0 2u
#define NOTE_Y 14u

static uint8_t sel;

static uint8_t can_tune(void) {
    return app.src == SRC_LINK && !app_ecu_missing() && app.ecu == LINK_ECU_MS1;
}

static void draw_cursor(void) {
    uint8_t i;
    for (i = 0; i < TUNE_COUNT; i++) {
        ui_tile(1, (uint8_t)(ITEM_Y0 + i * 2u), i == sel ? tile_small('>') : T_SPACE, PAL_AMBER);
    }
}

static void draw_note(void) {
    if (can_tune()) {
        ui_text(1, NOTE_Y, "MS1 RAM, LIVE   ", PAL_GREEN);
    } else {
        ui_text(1, NOTE_Y, "NEEDS MS1 + LINK", PAL_RED);
    }
}

void tunemenu_enter(void) {
    uint8_t i;
    ui_clear();
    hdr_draw("TUNE");
    for (i = 0; i < TUNE_COUNT; i++) {
        ui_text(3, (uint8_t)(ITEM_Y0 + i * 2u), names[i], PAL_TEXT);
    }
    ui_divider(15);
    ui_text(1, 16, "A:OPEN  B:BACK", PAL_DIM);
    draw_cursor();
    draw_note();
}

void tunemenu_tick(void) {
    hdr_tick();
    draw_note();
}

void tunemenu_input(uint8_t pressed) {
    if (pressed & J_UP) {
        sel = (uint8_t)((sel + TUNE_COUNT - 1u) % TUNE_COUNT);
        draw_cursor();
    } else if (pressed & J_DOWN) {
        sel = (uint8_t)((sel + 1u) % TUNE_COUNT);
        draw_cursor();
    } else if ((pressed & J_A) && can_tune()) {
        td_load(sel);
        screen_go(SCR_TEDIT);
    } else if (pressed & J_B) {
        screen_go(SCR_MENU);
    }
}
