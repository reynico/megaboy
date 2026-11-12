#include "tunemenu.h"

#include <gb/gb.h>

#include "hdr.h"
#include "screen.h"
#include "tiles.h"
#include "ui.h"

void tunemenu_enter(void) {
    ui_clear();
    hdr_draw("TUNE");
    ui_text(1, 3, "BOOST TGT", PAL_DIM);
    ui_text(15, 3, "--", PAL_DIM);
    ui_text(1, 5, "LAUNCH RPM", PAL_DIM);
    ui_text(15, 5, "--", PAL_DIM);
    ui_text(1, 7, "VE TABLE", PAL_DIM);
    ui_text(12, 7, "LOCKED", PAL_DIM);
    ui_text(1, 9, "IGN TABLE", PAL_DIM);
    ui_text(12, 9, "LOCKED", PAL_DIM);
    ui_divider(11);
    ui_text(1, 12, "READ ONLY", PAL_AMBER);
    ui_text(1, 13, "WRITE IN PHASE 4", PAL_DIM);
    ui_divider(15);
    ui_text(1, 16, "B:BACK", PAL_DIM);
}

void tunemenu_tick(void) {
    hdr_tick();
}

void tunemenu_input(uint8_t pressed) {
    if (pressed & J_B) {
        screen_go(SCR_MENU);
    }
}
