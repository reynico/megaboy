#include "about.h"

#include <gb/gb.h>

#include "app.h"
#include "fmt.h"
#include "hdr.h"
#include "link.h"
#include "screen.h"
#include "sound.h"
#include "tiles.h"
#include "ui.h"
#include "version.h"

void about_enter(void) {
    ui_clear();
    hdr_draw("ABOUT");
    ui_text(1, 3, "MEGABOY", PAL_TEXT);
    ui_text((uint8_t)(19u - (sizeof MB_VERSION - 1u)), 3, MB_VERSION, PAL_DIM);
    ui_text(1, 5, "LINK", PAL_DIM);
    ui_text(1, 7, "ECU", PAL_DIM);
    ui_text(1, 9, "UPTIME", PAL_DIM);
    ui_text(1, 11, "COMMIT", PAL_DIM);
    ui_text(8, 11, MB_COMMIT, PAL_TEXT);
    ui_text(1, 13, "GITHUB.COM/", PAL_BLUE);
    ui_text(1, 14, "REYNICO/MEGABOY", PAL_BLUE);
    ui_divider(15);
    ui_text(1, 16, "B:BACK", PAL_DIM);
    sound_music(1);
}

static const char *link_text(void) {
    return app.src == SRC_LOST ? "LOST" : "OK  ";
}

static const char *ecu_text(void) {
    if (app.src != SRC_LINK || app.ecu_state != LINK_ST_OK) {
        return "NONE     ";
    }
    if (app.ecu == LINK_ECU_MS1) {
        return "MS1/EXTRA";
    }
    return app.ecu == LINK_ECU_MS2 ? "MS2/EXTRA" : "NONE     ";
}

void about_tick(void) {
    char s[5];
    hdr_tick();
    ui_text(8, 5, link_text(), app.src == SRC_LOST ? PAL_RED : PAL_TEXT);
    ui_text(8, 7, ecu_text(), PAL_TEXT);
    fmt_time(s, app.uptime_s);
    ui_textn(8, 9, s, 5, PAL_TEXT);
}

void about_input(uint8_t pressed) {
    if (pressed & J_B) {
        sound_music(0);
        screen_go(SCR_MENU);
    }
}
