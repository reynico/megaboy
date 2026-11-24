#include <gb/cgb.h>
#include <gb/gb.h>

#include "app.h"
#include "gauge.h"
#include "input.h"
#include "linkrx.h"
#include "save.h"
#include "screen.h"
#include "settings.h"
#include "sound.h"
#include "tiles.h"

#define C_BG RGB(2, 3, 4)
#define C_TRACK RGB(5, 7, 8)
#define C_FG RGB(29, 30, 28)
#define C_DIM RGB(17, 19, 20)
#define C_GREEN RGB(11, 24, 8)
#define C_AMBER RGB(31, 21, 6)
#define C_RED RGB(31, 9, 6)
#define C_BLUE RGB(9, 20, 31)
#define C_WBG RGB(31, 27, 4)
#define C_WFG RGB(1, 1, 1)

static const palette_color_t needle_pal[4] = {C_BG, C_RED, C_AMBER, C_FG};

static const palette_color_t pals[PAL_COUNT * 4] = {
    C_BG, C_TRACK, C_FG, C_FG,
    C_BG, C_TRACK, C_DIM, C_DIM,
    C_BG, C_TRACK, C_GREEN, C_GREEN,
    C_BG, C_TRACK, C_AMBER, C_AMBER,
    C_BG, C_TRACK, C_RED, C_RED,
    C_BG, C_TRACK, C_BLUE, C_BLUE,
    C_WBG, C_WBG, C_WFG, C_WFG,
    C_GREEN, C_GREEN, C_BG, C_BG,
};

#define FRAMES_PER_TICK 6u
#define MIN_DRAW_FRAMES 2u
#define FRAMES_PER_SECOND 60u
#define MAX_CATCH_UP 8u

void main(void) {
    uint8_t frame = 0;
    uint8_t sec_frames = 0;
    uint8_t since_draw = 0;
    uint8_t draw;
    uint8_t pressed;
    uint8_t elapsed;
    uint8_t i;
    uint16_t last;

    DISPLAY_OFF;
    if (_cpu == CGB_TYPE) {
        cpu_fast();
        set_bkg_palette(0, PAL_COUNT, pals);
        set_sprite_palette(0, 1, needle_pal);
    } else {
        BGP_REG = 0x1bu;
    }
    tiles_load();
    gauge_load();
    sound_init();
    app_init();
    if (save_load(&app.set)) {
        settings_clamp();
    }
    linkrx_init();
    screen_go(SCR_DASH);
    SHOW_BKG;
    DISPLAY_ON;

    last = sys_time;
    while (1) {
        wait_vbl_done();
        elapsed = (uint8_t)(sys_time - last);
        last += elapsed;
        if (elapsed > MAX_CATCH_UP) {
            elapsed = MAX_CATCH_UP;
        }
        linkrx_poll();
        for (i = 0; i < elapsed; i++) {
            sound_tick();
        }
        pressed = input_poll();
        if (app.set.key_snd) {
            sound_key(pressed);
        }
        screen_input(pressed);

        sec_frames += elapsed;
        if (sec_frames >= FRAMES_PER_SECOND) {
            sec_frames -= FRAMES_PER_SECOND;
            app_second();
        }
        app.blink = sec_frames < 30u;

        draw = 0;
        frame += elapsed;
        if (frame >= FRAMES_PER_TICK) {
            frame = frame >= 2u * FRAMES_PER_TICK ? 0u : (uint8_t)(frame - FRAMES_PER_TICK);
            app_tick();
            draw = 1;
        }
        since_draw = since_draw > 255u - MAX_CATCH_UP ? 255u : (uint8_t)(since_draw + elapsed);
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
