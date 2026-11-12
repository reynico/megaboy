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

void boost_tick(void) {
    const rt_t *rt = &app.rt;
    char s[5];

    hdr_tick();
    fmt_x10(s, 5, rt->boost_psi_x10, 1);
    ui_big(5, 4, s, 5, rt->boost_psi_x10 >= 0 ? PAL_AMBER : PAL_BLUE);
    ui_bar(1, 7, 18, rt->boost_psi_x10, 250);

    fmt_x10(s, 5, app.peak_x10, 1);
    ui_textn(5, 9, s, 5, PAL_AMBER);
    fmt_int(s, 4, fmt_round_x10(rt->map_x10), 0);
    ui_textn(15, 9, s, 4, PAL_TEXT);
    fmt_int(s, 4, fmt_round_x10(rt->iat_c_x10), 'C');
    ui_textn(6, 11, s, 4, PAL_TEXT);
    fmt_x10(s, 4, rt->afr_x10, 0);
    ui_textn(15, 11, s, 4, PAL_TEXT);
    fmt_int(s, 4, (int16_t)(rt->rpm > 9999u ? 9999u : rt->rpm), 0);
    ui_textn(6, 13, s, 4, PAL_TEXT);
    fmt_int(s, 4, fmt_round_x10(rt->tps_x10), '%');
    ui_textn(15, 13, s, 4, PAL_TEXT);
}

void boost_input(uint8_t pressed) {
    if (pressed & J_B) {
        app.peak_x10 = 0;
    }
}
