#include "warn.h"

#include <gb/cgb.h>
#include <gb/gb.h>

#include "app.h"
#include "fmt.h"
#include "screen.h"
#include "sound.h"
#include "tiles.h"
#include "ui.h"

#define TOP 2u
#define BOTTOM 16u

static const palette_color_t flash[2][4] = {
    {RGB(31, 27, 4), RGB(31, 27, 4), RGB(1, 1, 1), RGB(1, 1, 1)},
    {RGB(31, 10, 4), RGB(31, 10, 4), RGB(1, 1, 1), RGB(1, 1, 1)},
};

static uint8_t shown;
static uint8_t lit;

void warn_show(uint8_t id) {
    shown = id;
}

static void flash_set(uint8_t on) {
    if (_cpu != CGB_TYPE || lit == on) {
        return;
    }
    lit = on;
    wait_vbl_done();
    set_bkg_palette(PAL_WARN, 1, flash[on]);
}

void warn_enter(void) {
    uint8_t y;
    lit = 0xffu;
    flash_set(0);
    for (y = TOP; y <= BOTTOM; y++) {
        ui_textn(0, y, "                    ", 20, PAL_WARN);
    }
    ui_big(1, 3, "WARNING!!", 9, PAL_WARN);
    switch (shown) {
    case W_CLT:
        ui_text(1, 6, "ENGINE OVERHEATING", PAL_WARN);
        break;
    case W_LEAN:
        ui_text(2, 6, "LEAN UNDER BOOST", PAL_WARN);
        break;
    default:
        ui_text(1, 6, "DANGER TO MANIFOLD", PAL_WARN);
        break;
    }
    ui_big(1, 12, "LIFT OFF!", 9, PAL_WARN);
    ui_text(7, 15, "A:ACK", PAL_WARN);
    sound_alarm(app.set.alarm_snd != 0);
}

static void unit(char *s, char a, char b, char c) {
    s[0] = ' ';
    s[1] = a;
    s[2] = b;
    s[3] = c;
}

void warn_tick(void) {
    char s[9];
    const rt_t *rt = &app.rt;
    flash_set(!app.blink);
    switch (shown) {
    case W_CLT:
        fmt_int(s, 4, fmt_round_x10(rt->clt_c_x10), 'C');
        ui_big(6, 8, s, 4, PAL_WARN);
        break;
    case W_LEAN:
        fmt_x10(s, 4, rt->afr_x10, 0);
        unit(s + 4, 'A', 'F', 'R');
        ui_big(2, 8, s, 8, PAL_WARN);
        break;
    default:
        fmt_x10(s, 5, rt->boost_psi_x10, 1);
        unit(s + 5, 'P', 'S', 'I');
        ui_big(1, 8, s, 9, PAL_WARN);
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
