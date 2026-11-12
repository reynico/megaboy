#include "dash.h"

#include "fmt.h"
#include "tiles.h"
#include "ui.h"

void dash_draw_static(void) {
    ui_clear();
    ui_text(0, 0, "MS2 OK", PAL_GREEN);
    ui_divider(1);
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

void dash_update(const rt_t *rt, int16_t peak_x10, uint8_t blink, uint16_t rec_s) {
    char s[9];

    ui_tile(10, 0, blink ? T_DOT : T_SPACE, PAL_RED);
    s[0] = 'R';
    s[1] = 'E';
    s[2] = 'C';
    s[3] = ' ';
    s[4] = (char)('0' + (rec_s / 600u) % 10u);
    s[5] = (char)('0' + (rec_s / 60u) % 10u);
    s[6] = ':';
    s[7] = (char)('0' + (rec_s % 60u) / 10u);
    s[8] = (char)('0' + rec_s % 10u);
    ui_textn(11, 0, s, 9, PAL_TEXT);

    fmt_int(s, 4, (int16_t)(rt->rpm > 9999u ? 9999u : rt->rpm), 0);
    ui_big(1, 3, s, 4, rt->rpm >= 6800u ? PAL_RED : PAL_TEXT);
    fmt_x10(s, 4, rt->afr_x10, 0);
    ui_big(11, 3, s, 4, PAL_TEXT);
    ui_bar(1, 5, 18, rt->rpm, 8000);

    fmt_x10(s, 5, rt->boost_psi_x10, 1);
    ui_big(1, 8, s, 5, rt->boost_psi_x10 >= 0 ? PAL_AMBER : PAL_BLUE);
    fmt_x10(s, 5, peak_x10, 1);
    ui_textn(13, 8, s, 5, PAL_AMBER);
    ui_bar(1, 10, 18, rt->boost_psi_x10, 250);

    fmt_x10(s, 4, rt->adv_x10, 0);
    ui_textn(5, 12, s, 4, PAL_TEXT);
    fmt_int(s, 4, fmt_round_x10(rt->clt_c_x10), 'C');
    ui_textn(15, 12, s, 4, PAL_TEXT);
    fmt_int(s, 4, fmt_round_x10(rt->iat_c_x10), 'C');
    ui_textn(5, 14, s, 4, PAL_TEXT);
    fmt_int(s, 4, fmt_round_x10(rt->tps_x10), '%');
    ui_textn(15, 14, s, 4, PAL_TEXT);
    fmt_x10(s, 4, rt->batt_x10, 0);
    ui_textn(5, 16, s, 4, PAL_TEXT);
    fmt_int(s, 4, fmt_round_x10(rt->map_x10), 0);
    ui_textn(15, 16, s, 4, PAL_TEXT);
}
