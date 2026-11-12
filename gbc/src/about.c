#include "about.h"

#include <gb/gb.h>

#include "app.h"
#include "fmt.h"
#include "hdr.h"
#include "screen.h"
#include "tiles.h"
#include "ui.h"

void about_enter(void) {
    ui_clear();
    hdr_draw("ABOUT");
    ui_text(1, 3, "MEGABOY", PAL_TEXT);
    ui_text(13, 3, "V0.1.0", PAL_DIM);
    ui_text(1, 5, "DATA", PAL_DIM);
    ui_text(8, 5, "SIMULATED", PAL_AMBER);
    ui_text(1, 7, "LINK", PAL_DIM);
    ui_text(8, 7, "NONE", PAL_TEXT);
    ui_text(1, 9, "ECU", PAL_DIM);
    ui_text(8, 9, "MS2EXTRA", PAL_TEXT);
    ui_text(8, 10, "COMMS332M2", PAL_TEXT);
    ui_text(1, 12, "UPTIME", PAL_DIM);
    ui_divider(15);
    ui_text(1, 16, "B:BACK", PAL_DIM);
}

void about_tick(void) {
    char s[5];
    hdr_tick();
    fmt_time(s, app.uptime_s);
    ui_textn(8, 12, s, 5, PAL_TEXT);
}

void about_input(uint8_t pressed) {
    if (pressed & J_B) {
        screen_go(SCR_MENU);
    }
}
