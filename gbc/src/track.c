#include "track.h"

#include <gb/gb.h>

#include "app.h"
#include "fmt.h"
#include "hdr.h"
#include "tiles.h"
#include "ui.h"

#define BAR_W 18u
#define CLT_MAX 120
#define IAT_MAX 80u

static uint8_t clt_red_from(void) {
    int16_t c = app.set.clt_warn_c;
    if (c <= 0) {
        return 1u;
    }
    if (c >= CLT_MAX) {
        return BAR_W;
    }
    c = (int16_t)(c * BAR_W / CLT_MAX);
    return c < 3 ? 3u : (uint8_t)c;
}

void track_enter(void) {
    uint8_t red = clt_red_from();
    ui_clear();
    hdr_draw("TRACK");
    ui_text(1, 4, "RPM", PAL_DIM);
    ui_bar_zones(1, 5, BAR_W, 11, 15);
    ui_text(1, 9, "CLT", PAL_DIM);
    ui_bar_zones(1, 10, BAR_W, (uint8_t)(red - 2u), red);
    ui_text(1, 14, "IAT", PAL_DIM);
    ui_bar_zones(1, 15, BAR_W, 11, 15);
}

enum { F_RPM, F_CLT, F_IAT };

static void temp(uint8_t f, uint8_t y, int16_t x10, uint16_t max, uint8_t pal) {
    char s[4];
    int16_t c = fmt_round_x10(x10);
    if (ui_changed(f, c, pal)) {
        fmt_int(s, 4, c, 'C');
        ui_big(11, y, s, 4, pal);
        ui_bar(1, (uint8_t)(y + 2u), BAR_W, c, max);
    }
}

void track_tick(void) {
    const rt_t *rt = &app.rt;
    char s[4];
    int16_t rpm = (int16_t)(rt->rpm > 9999u ? 9999u : rt->rpm);
    uint8_t pal;

    hdr_tick();
    pal = rt->rpm >= (uint16_t)app.set.rpm_shift ? PAL_RED : PAL_TEXT;
    if (ui_changed(F_RPM, rpm, pal)) {
        fmt_int(s, 4, rpm, 0);
        ui_big(11, 3, s, 4, pal);
        ui_bar(1, 5, BAR_W, rpm, 8000u);
    }
    pal = app_warn_active(W_CLT) ? PAL_RED : PAL_TEXT;
    temp(F_CLT, 8, rt->clt_c_x10, CLT_MAX, pal);
    temp(F_IAT, 13, rt->iat_c_x10, IAT_MAX, PAL_TEXT);
}

void track_input(uint8_t pressed) {
    (void)pressed;
}
