#include "warn.h"

#include <gb/gb.h>

#include "app.h"
#include "fmt.h"
#include "screen.h"
#include "sound.h"
#include "tiles.h"
#include "ui.h"

static uint8_t shown;

static void stripe(uint8_t y) {
    uint8_t x;
    for (x = 0; x < 20u; x++) {
        ui_tile(x, y, T_STRIPE, PAL_WARN);
    }
}

void warn_show(uint8_t id) {
    shown = id;
}

void warn_enter(void) {
    char s[6];
    ui_clear();
    ui_attr(0, 0, 20, 18, PAL_WARN);
    stripe(0);
    stripe(17);
    ui_big(3, 2, "WARNING", 7, PAL_WARN);
    switch (shown) {
    case W_CLT:
        ui_text(4, 6, "COOLANT TEMP", PAL_WARN);
        fmt_int(s, 4, app.set.clt_warn_c, 'C');
        ui_text(1, 13, "LIFT AND COOL DOWN", PAL_WARN);
        break;
    case W_LEAN:
        ui_text(2, 6, "LEAN UNDER BOOST", PAL_WARN);
        fmt_x10(s, 4, app.set.afr_lean_x10, 0);
        ui_text(6, 13, "LIFT OFF", PAL_WARN);
        break;
    default:
        ui_text(5, 6, "OVERBOOST", PAL_WARN);
        fmt_x10(s, 4, app.set.boost_warn_x10, 0);
        ui_text(6, 13, "LIFT OFF", PAL_WARN);
        break;
    }
    ui_text(4, 11, "LIMIT", PAL_WARN);
    ui_textn(10, 11, s, 4, PAL_WARN);
    ui_text(7, 15, "A:ACK", PAL_WARN);
    sound_alarm(app.set.alarm_snd != 0);
}

void warn_tick(void) {
    char s[5];
    const rt_t *rt = &app.rt;
    switch (shown) {
    case W_CLT:
        fmt_int(s, 4, fmt_round_x10(rt->clt_c_x10), 'C');
        ui_big(6, 8, s, 4, PAL_WARN);
        break;
    case W_LEAN:
        fmt_x10(s, 4, rt->afr_x10, 0);
        ui_big(6, 8, s, 4, PAL_WARN);
        break;
    default:
        fmt_x10(s, 5, rt->boost_psi_x10, 1);
        ui_big(5, 8, s, 5, PAL_WARN);
        break;
    }
}

void warn_input(uint8_t pressed) {
    if (pressed & J_A) {
        sound_alarm(0);
        app_warn_ack(shown);
        screen_return();
    }
}
