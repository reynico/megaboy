#include "dash.h"

#include <gb/gb.h>

#include "app.h"
#include "fmt.h"
#include "hdr.h"
#include "tiles.h"
#include "ui.h"

void dash_enter(void) {
    ui_clear();
    hdr_draw("DASH");
    ui_text(1, 2, "RPM", PAL_DIM);
    ui_text(11, 2, "AFR", PAL_DIM);
    ui_bar_zones(1, 5, 18, 11, 15);
    ui_text(1, 7, "BOOST PSI", PAL_DIM);
    ui_text(13, 7, "PEAK", PAL_DIM);
    ui_bar_zones(1, 10, 18, 0, 15);
    ui_divider(11);
    ui_text(1, 12, "ADV", PAL_DIM);
    ui_text(11, 12, "CLT", PAL_DIM);
    ui_text(1, 14, "IAT", PAL_DIM);
    ui_text(11, 14, "TPS", PAL_DIM);
    ui_text(1, 16, "BAT", PAL_DIM);
    ui_text(11, 16, "MAP", PAL_DIM);
}

enum { F_RPM, F_AFR, F_BOOST, F_PEAK, F_ADV, F_CLT, F_IAT, F_TPS, F_BAT, F_MAP };

static void small_x10(uint8_t f, uint8_t x, uint8_t y, int16_t v) {
    char s[4];
    if (ui_changed(f, v, PAL_TEXT)) {
        fmt_x10(s, 4, v, 0);
        ui_textn(x, y, s, 4, PAL_TEXT);
    }
}

static void small_int(uint8_t f, uint8_t x, uint8_t y, int16_t v, char suffix) {
    char s[4];
    if (ui_changed(f, v, PAL_TEXT)) {
        fmt_int(s, 4, v, suffix);
        ui_textn(x, y, s, 4, PAL_TEXT);
    }
}

void dash_tick(void) {
    const rt_t *rt = &app.rt;
    char s[5];
    int16_t rpm = (int16_t)(rt->rpm > 9999u ? 9999u : rt->rpm);
    uint8_t pal;

    hdr_tick();
    pal = rt->rpm >= (uint16_t)app.set.rpm_shift ? PAL_RED : PAL_TEXT;
    if (ui_changed(F_RPM, rpm, pal)) {
        fmt_int(s, 4, rpm, 0);
        ui_big(1, 3, s, 4, pal);
        ui_bar(1, 5, 18, rpm, 8000u);
    }
    if (ui_changed(F_AFR, rt->afr_x10, PAL_TEXT)) {
        fmt_x10(s, 4, rt->afr_x10, 0);
        ui_big(11, 3, s, 4, PAL_TEXT);
    }
    pal = rt->boost_psi_x10 >= 0 ? PAL_AMBER : PAL_BLUE;
    if (ui_changed(F_BOOST, rt->boost_psi_x10, pal)) {
        fmt_x10(s, 5, rt->boost_psi_x10, 1);
        ui_big(1, 8, s, 5, pal);
        ui_bar(1, 10, 18, rt->boost_psi_x10, 250u);
    }
    if (ui_changed(F_PEAK, app.peak_x10, PAL_AMBER)) {
        fmt_x10(s, 5, app.peak_x10, 1);
        ui_textn(13, 8, s, 5, PAL_AMBER);
    }
    small_x10(F_ADV, 5, 12, rt->adv_x10);
    small_int(F_CLT, 15, 12, fmt_round_x10(rt->clt_c_x10), 'C');
    small_int(F_IAT, 5, 14, fmt_round_x10(rt->iat_c_x10), 'C');
    small_int(F_TPS, 15, 14, fmt_round_x10(rt->tps_x10), '%');
    small_x10(F_BAT, 5, 16, rt->batt_x10);
    small_int(F_MAP, 15, 16, fmt_round_x10(rt->map_x10), 0);
}

void dash_input(uint8_t pressed) {
    if (pressed & J_B) {
        app.peak_x10 = 0;
    }
}
