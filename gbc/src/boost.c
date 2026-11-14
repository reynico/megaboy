#include "boost.h"

#include <gb/gb.h>

#include "app.h"
#include "fmt.h"
#include "hdr.h"
#include "tiles.h"
#include "ui.h"

void boost_enter(void) {
    ui_clear();
    hdr_draw("BOOST");
    ui_text(1, 3, "BOOST PSI", PAL_DIM);
    ui_bar_zones(1, 7, 18, 0, 15);
    ui_text(1, 9, "PEAK", PAL_DIM);
    ui_text(11, 9, "MAP", PAL_DIM);
    ui_text(1, 11, "IAT", PAL_DIM);
    ui_text(11, 11, "AFR", PAL_DIM);
    ui_text(1, 13, "RPM", PAL_DIM);
    ui_text(11, 13, "TPS", PAL_DIM);
    ui_divider(15);
    ui_text(1, 16, "B:RESET PEAK", PAL_DIM);
}

enum { F_BOOST, F_PEAK, F_MAP, F_IAT, F_AFR, F_RPM, F_TPS };

static void small_int(uint8_t f, uint8_t x, uint8_t y, int16_t v, char suffix) {
    char s[4];
    if (ui_changed(f, v, PAL_TEXT)) {
        fmt_int(s, 4, v, suffix);
        ui_textn(x, y, s, 4, PAL_TEXT);
    }
}

void boost_tick(void) {
    const rt_t *rt = &app.rt;
    char s[5];
    uint8_t pal = rt->boost_psi_x10 >= 0 ? PAL_AMBER : PAL_BLUE;

    hdr_tick();
    if (ui_changed(F_BOOST, rt->boost_psi_x10, pal)) {
        fmt_x10(s, 5, rt->boost_psi_x10, 1);
        ui_big(5, 4, s, 5, pal);
        ui_bar(1, 7, 18, rt->boost_psi_x10, 250u);
    }
    if (ui_changed(F_PEAK, app.peak_x10, PAL_AMBER)) {
        fmt_x10(s, 5, app.peak_x10, 1);
        ui_textn(5, 9, s, 5, PAL_AMBER);
    }
    small_int(F_MAP, 15, 9, fmt_round_x10(rt->map_x10), 0);
    small_int(F_IAT, 6, 11, fmt_round_x10(rt->iat_c_x10), 'C');
    if (ui_changed(F_AFR, rt->afr_x10, PAL_TEXT)) {
        fmt_x10(s, 4, rt->afr_x10, 0);
        ui_textn(15, 11, s, 4, PAL_TEXT);
    }
    small_int(F_RPM, 6, 13, (int16_t)(rt->rpm > 9999u ? 9999u : rt->rpm), 0);
    small_int(F_TPS, 15, 13, fmt_round_x10(rt->tps_x10), '%');
}

void boost_input(uint8_t pressed) {
    if (pressed & J_B) {
        app.peak_x10 = 0;
    }
}
