#include <gb/cgb.h>
#include <gb/gb.h>

#include "app.h"
#include "input.h"
#include "linkrx.h"
#include "save.h"
#include "screen.h"
#include "settings.h"
#include "tiles.h"

#define C_BG RGB(2, 3, 4)
#define C_TRACK RGB(5, 7, 8)
#define C_FG RGB(29, 30, 28)
#define C_DIM RGB(17, 19, 20)
#define C_GREEN RGB(11, 24, 8)
#define C_AMBER RGB(31, 21, 6)
#define C_RED RGB(31, 9, 6)
#define C_BLUE RGB(9, 20, 31)
#define C_WBG RGB(22, 4, 2)
#define C_WFG RGB(31, 31, 31)
#define C_WDIM RGB(31, 27, 25)

static const palette_color_t pals[PAL_COUNT * 4] = {
    C_BG, C_TRACK, C_FG, C_FG,
    C_BG, C_TRACK, C_DIM, C_DIM,
    C_BG, C_TRACK, C_GREEN, C_GREEN,
    C_BG, C_TRACK, C_AMBER, C_AMBER,
    C_BG, C_TRACK, C_RED, C_RED,
    C_BG, C_TRACK, C_BLUE, C_BLUE,
    C_WBG, C_WBG, C_WFG, C_WFG,
    C_WBG, C_WBG, C_WDIM, C_WDIM,
};

#define FRAMES_PER_TICK 6u
#define MIN_DRAW_FRAMES 2u
#define FRAMES_PER_SECOND 60u

void main(void) {
    uint8_t frame = 0;
    uint8_t sec_frames = 0;
    uint8_t since_draw = 0;
    uint8_t draw;

    DISPLAY_OFF;
    if (_cpu == CGB_TYPE) {
        cpu_fast();
        set_bkg_palette(0, PAL_COUNT, pals);
    } else {
        BGP_REG = 0x1bu;
    }
    tiles_load();
    app_init();
    if (save_load(&app.set)) {
        settings_clamp();
    }
    linkrx_init();
    screen_go(SCR_DASH);
    SHOW_BKG;
    DISPLAY_ON;

    while (1) {
        wait_vbl_done();
        linkrx_poll();
        screen_input(input_poll());

        if (++sec_frames >= FRAMES_PER_SECOND) {
            sec_frames = 0;
            app_second();
        }
        app.blink = sec_frames < 30u;

        draw = 0;
        if (++frame >= FRAMES_PER_TICK) {
            frame = 0;
            app_tick();
            draw = 1;
        }
        if (since_draw < 255u) {
            since_draw++;
        }
        if (app.rt_new && since_draw >= MIN_DRAW_FRAMES) {
            draw = 1;
        }
        if (draw) {
            app.rt_new = 0;
            since_draw = 0;
            screen_tick();
        }
    }
}
