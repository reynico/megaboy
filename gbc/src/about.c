#include "about.h"

#include <gb/gb.h>

#include "app.h"
#include "fmt.h"
#include "hdr.h"
#include "link.h"
#include "screen.h"
#include "tiles.h"
#include "ui.h"

void about_enter(void) {
    ui_clear();
    hdr_draw("ABOUT");
    ui_text(1, 3, "MEGABOY", PAL_TEXT);
    ui_text(13, 3, "V0.1.0", PAL_DIM);
    ui_text(1, 5, "DATA", PAL_DIM);
    ui_text(1, 7, "LINK", PAL_DIM);
    ui_text(1, 9, "ECU", PAL_DIM);
    ui_text(1, 11, "UPTIME", PAL_DIM);
    ui_text(1, 13, "GITHUB.COM/", PAL_BLUE);
    ui_text(1, 14, "REYNICO/MEGABOY", PAL_BLUE);
    ui_divider(15);
    ui_text(1, 16, "B:BACK", PAL_DIM);
}

static const char *data_text(void) {
    if (app.src == SRC_SIM) {
        return "SIMULATED";
    }
    if (app.src == SRC_LOST) {
        return "STALE    ";
    }
    if (app.demo) {
        return "DEMO     ";
    }
    return app_ecu_missing() ? "STALE    " : "LIVE     ";
}

static const char *link_text(void) {
    if (app.src == SRC_SIM) {
        return "NONE";
    }
    return app.src == SRC_LOST ? "LOST" : "OK  ";
}

static const char *ecu_text(void) {
    if (app.src == SRC_SIM || app.ecu_state != LINK_ST_OK) {
        return "NONE     ";
    }
    if (app.ecu == LINK_ECU_MS1) {
        return "MS1/EXTRA";
    }
    return app.ecu == LINK_ECU_MS2 ? "MS2/EXTRA" : "DEMO     ";
}

void about_tick(void) {
    char s[5];
    hdr_tick();
    ui_text(8, 5, data_text(), app.src == SRC_LINK && !app.demo && !app_ecu_missing() ? PAL_GREEN : PAL_AMBER);
    ui_text(8, 7, link_text(), app.src == SRC_LOST ? PAL_RED : PAL_TEXT);
    ui_text(8, 9, ecu_text(), PAL_TEXT);
    fmt_time(s, app.uptime_s);
    ui_textn(8, 11, s, 5, PAL_TEXT);
}

void about_input(uint8_t pressed) {
    if (pressed & J_B) {
        screen_go(SCR_MENU);
    }
}
