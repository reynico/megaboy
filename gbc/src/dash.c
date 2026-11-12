#include "dash.h"

#include <gb/cgb.h>
#include <gb/gb.h>
#include <string.h>

#include "fmt.h"
#include "tiles.h"

#define BAR_X 1u
#define BAR_W 18u
#define BAR_PX (BAR_W * 7u)

static uint8_t row[20];

static void set_attr(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t pal) {
    uint8_t i;
    if (_cpu != CGB_TYPE || w == 0u) {
        return;
    }
    for (i = 0; i < w; i++) {
        row[i] = pal;
    }
    VBK_REG = VBK_ATTRIBUTES;
    for (i = 0; i < h; i++) {
        set_bkg_tiles(x, (uint8_t)(y + i), w, 1, row);
    }
    VBK_REG = VBK_TILES;
}

static void put_small(uint8_t x, uint8_t y, const char *s, uint8_t n, uint8_t pal) {
    uint8_t i;
    for (i = 0; i < n; i++) {
        row[i] = tile_small(s[i]);
    }
    set_bkg_tiles(x, y, n, 1, row);
    set_attr(x, y, n, 1, pal);
}

static void label(uint8_t x, uint8_t y, const char *s, uint8_t pal) {
    put_small(x, y, s, (uint8_t)strlen(s), pal);
}

static void put_big(uint8_t x, uint8_t y, const char *s, uint8_t n, uint8_t pal) {
    uint8_t i;
    uint8_t half;
    for (half = 0; half < 2u; half++) {
        for (i = 0; i < n; i++) {
            row[i * 2u] = tile_big(s[i], (uint8_t)(half * 2u));
            row[i * 2u + 1u] = tile_big(s[i], (uint8_t)(half * 2u + 1u));
        }
        set_bkg_tiles(x, (uint8_t)(y + half), (uint8_t)(n * 2u), 1, row);
    }
    set_attr(x, y, (uint8_t)(n * 2u), 2, pal);
}

static void put_bar(uint8_t y, int32_t value, int32_t max) {
    uint8_t i;
    int16_t px;
    if (value < 0) {
        value = 0;
    }
    if (value > max) {
        value = max;
    }
    px = (int16_t)(value * BAR_PX / max);
    for (i = 0; i < BAR_W; i++) {
        int16_t l = (int16_t)(px - (int16_t)i * 7);
        if (l < 0) {
            l = 0;
        }
        if (l > 7) {
            l = 7;
        }
        row[i] = (uint8_t)(T_BAR0 + (uint8_t)l);
    }
    set_bkg_tiles(BAR_X, y, BAR_W, 1, row);
}

static void bar_zones(uint8_t y, uint8_t amber_from, uint8_t red_from) {
    set_attr(BAR_X, y, amber_from, 1, PAL_GREEN);
    set_attr((uint8_t)(BAR_X + amber_from), y, (uint8_t)(red_from - amber_from), 1, PAL_AMBER);
    set_attr((uint8_t)(BAR_X + red_from), y, (uint8_t)(BAR_W - red_from), 1, PAL_RED);
}

static void divider(uint8_t y) {
    uint8_t i;
    for (i = 0; i < 20u; i++) {
        row[i] = T_DIV;
    }
    set_bkg_tiles(0, y, 20, 1, row);
    set_attr(0, y, 20, 1, PAL_TEXT);
}

void dash_draw_static(void) {
    uint8_t y;
    memset(row, T_SPACE, sizeof row);
    for (y = 0; y < 18u; y++) {
        set_bkg_tiles(0, y, 20, 1, row);
    }
    set_attr(0, 0, 20, 18, PAL_TEXT);

    label(0, 0, "MS2 OK", PAL_GREEN);
    divider(1);
    label(1, 2, "RPM", PAL_DIM);
    label(11, 2, "AFR", PAL_DIM);
    bar_zones(5, 11, 15);
    label(1, 7, "BOOST PSI", PAL_DIM);
    label(13, 7, "PEAK", PAL_DIM);
    bar_zones(10, 0, 15);
    divider(11);
    label(1, 12, "ADV", PAL_DIM);
    label(11, 12, "CLT", PAL_DIM);
    label(1, 14, "IAT", PAL_DIM);
    label(11, 14, "TPS", PAL_DIM);
    label(1, 16, "BAT", PAL_DIM);
    label(11, 16, "MAP", PAL_DIM);
}

void dash_update(const rt_t *rt, int16_t peak_x10, uint8_t blink, uint16_t rec_s) {
    char s[9];

    row[0] = blink ? T_DOT : T_SPACE;
    set_bkg_tiles(10, 0, 1, 1, row);
    set_attr(10, 0, 1, 1, PAL_RED);
    s[0] = 'R';
    s[1] = 'E';
    s[2] = 'C';
    s[3] = ' ';
    s[4] = (char)('0' + (rec_s / 600u) % 10u);
    s[5] = (char)('0' + (rec_s / 60u) % 10u);
    s[6] = ':';
    s[7] = (char)('0' + (rec_s % 60u) / 10u);
    s[8] = (char)('0' + rec_s % 10u);
    put_small(11, 0, s, 9, PAL_TEXT);

    fmt_int(s, 4, (int16_t)(rt->rpm > 9999u ? 9999u : rt->rpm), 0);
    put_big(1, 3, s, 4, rt->rpm >= 6800u ? PAL_RED : PAL_TEXT);
    fmt_x10(s, 4, rt->afr_x10, 0);
    put_big(11, 3, s, 4, PAL_TEXT);
    put_bar(5, rt->rpm, 8000);

    fmt_x10(s, 5, rt->boost_psi_x10, 1);
    put_big(1, 8, s, 5, rt->boost_psi_x10 >= 0 ? PAL_AMBER : PAL_BLUE);
    fmt_x10(s, 5, peak_x10, 1);
    put_small(13, 8, s, 5, PAL_AMBER);
    put_bar(10, rt->boost_psi_x10, 250);

    fmt_x10(s, 4, rt->adv_x10, 0);
    put_small(5, 12, s, 4, PAL_TEXT);
    fmt_int(s, 4, fmt_round_x10(rt->clt_c_x10), 'C');
    put_small(15, 12, s, 4, PAL_TEXT);
    fmt_int(s, 4, fmt_round_x10(rt->iat_c_x10), 'C');
    put_small(5, 14, s, 4, PAL_TEXT);
    fmt_int(s, 4, fmt_round_x10(rt->tps_x10), '%');
    put_small(15, 14, s, 4, PAL_TEXT);
    fmt_x10(s, 4, rt->batt_x10, 0);
    put_small(5, 16, s, 4, PAL_TEXT);
    fmt_int(s, 4, fmt_round_x10(rt->map_x10), 0);
    put_small(15, 16, s, 4, PAL_TEXT);
}
