#include "hdr.h"

#include "app.h"
#include "fmt.h"
#include "link.h"
#include "tiles.h"
#include "ui.h"

void hdr_draw(const char *title) {
    ui_text(0, 0, title, PAL_AMBER);
    ui_divider(1);
    hdr_tick();
}

void hdr_tick(void) {
    char s[9];
    if (app.src == SRC_LOST) {
        ui_tile(10, 0, T_SPACE, PAL_TEXT);
        ui_text(11, 0, app.blink ? "  NO LINK" : "         ", PAL_RED);
    } else if (app.src == SRC_LINK && app.ecu == LINK_ECU_NONE) {
        ui_tile(10, 0, T_SPACE, PAL_TEXT);
        ui_text(11, 0, app.blink ? "  SET ECU" : "         ", PAL_RED);
    } else if (app_ecu_missing()) {
        ui_tile(10, 0, T_SPACE, PAL_TEXT);
        ui_text(11, 0, app.blink ? "   NO ECU" : "         ", PAL_RED);
    } else if (app.logging) {
        ui_tile(10, 0, app.blink ? T_DOT : T_SPACE, PAL_RED);
        s[0] = 'R';
        s[1] = 'E';
        s[2] = 'C';
        s[3] = ' ';
        fmt_time(s + 4, app.log_s);
        ui_textn(11, 0, s, 9, PAL_TEXT);
    } else if (app.src == SRC_LINK && app.log_state == LINK_LOG_FULL) {
        ui_tile(10, 0, T_SPACE, PAL_TEXT);
        ui_text(11, 0, "CARD FULL", PAL_RED);
    } else if (app.src == SRC_LINK && app.log_state == LINK_LOG_ERR) {
        ui_tile(10, 0, T_SPACE, PAL_TEXT);
        ui_text(11, 0, " CARD ERR", PAL_RED);
    } else if (app.src == SRC_LINK && app.log_state == LINK_LOG_NOCARD) {
        ui_tile(10, 0, T_SPACE, PAL_TEXT);
        ui_text(11, 0, "  NO CARD", PAL_AMBER);
    } else {
        ui_tile(10, 0, T_SPACE, PAL_TEXT);
        ui_text(11, 0, "  LOG OFF", PAL_DIM);
    }
}
