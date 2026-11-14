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

enum { H_LOST, H_SET_ECU, H_NO_ECU, H_REC, H_FULL, H_ERR, H_NO_CARD, H_OFF };

#define HDR_SLOT (UI_SLOTS - 1u)

static uint8_t hdr_mode(void) {
    if (app.src == SRC_LOST) {
        return H_LOST;
    }
    if (app.src == SRC_LINK && app.ecu == LINK_ECU_NONE) {
        return H_SET_ECU;
    }
    if (app_ecu_missing()) {
        return H_NO_ECU;
    }
    if (app.logging) {
        return H_REC;
    }
    if (app.src == SRC_LINK && app.log_state == LINK_LOG_FULL) {
        return H_FULL;
    }
    if (app.src == SRC_LINK && app.log_state == LINK_LOG_ERR) {
        return H_ERR;
    }
    if (app.src == SRC_LINK && app.log_state == LINK_LOG_NOCARD) {
        return H_NO_CARD;
    }
    return H_OFF;
}

void hdr_tick(void) {
    char s[9];
    uint8_t mode = hdr_mode();
    if (!ui_changed(HDR_SLOT, mode == H_REC ? (int16_t)app.log_s : 0, (uint8_t)(mode << 1 | app.blink))) {
        return;
    }
    if (mode == H_REC) {
        ui_tile(10, 0, app.blink ? T_DOT : T_SPACE, PAL_RED);
        s[0] = 'R';
        s[1] = 'E';
        s[2] = 'C';
        s[3] = ' ';
        fmt_time(s + 4, app.log_s);
        ui_textn(11, 0, s, 9, PAL_TEXT);
        return;
    }
    ui_tile(10, 0, T_SPACE, PAL_TEXT);
    switch (mode) {
    case H_LOST:
        ui_text(11, 0, app.blink ? "  NO LINK" : "         ", PAL_RED);
        break;
    case H_SET_ECU:
        ui_text(11, 0, app.blink ? "  SET ECU" : "         ", PAL_RED);
        break;
    case H_NO_ECU:
        ui_text(11, 0, app.blink ? "   NO ECU" : "         ", PAL_RED);
        break;
    case H_FULL:
        ui_text(11, 0, "CARD FULL", PAL_RED);
        break;
    case H_ERR:
        ui_text(11, 0, " CARD ERR", PAL_RED);
        break;
    case H_NO_CARD:
        ui_text(11, 0, "  NO CARD", PAL_AMBER);
        break;
    default:
        ui_text(11, 0, "  LOG OFF", PAL_DIM);
        break;
    }
}
