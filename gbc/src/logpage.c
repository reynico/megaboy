#include "logpage.h"

#include <gb/gb.h>

#include "app.h"
#include "fmt.h"
#include "hdr.h"
#include "tiles.h"
#include "ui.h"

void logpage_enter(void) {
    ui_clear();
    hdr_draw("LOG");
    ui_text(1, 3, "FILE", PAL_DIM);
    ui_text(1, 5, "TIME", PAL_DIM);
    ui_text(1, 7, "RATE", PAL_DIM);
    ui_text(8, 7, "20 HZ", PAL_TEXT);
    ui_text(1, 9, "SIZE", PAL_DIM);
    ui_text(1, 11, "CARD", PAL_DIM);
    ui_text(8, 11, "SIM", PAL_AMBER);
    ui_divider(15);
}

void logpage_tick(void) {
    char s[11];
    uint32_t kb = app.log_bytes / 1024u;

    hdr_tick();
    s[0] = 'L';
    s[1] = 'O';
    s[2] = 'G';
    s[3] = (char)('0' + (app.log_num / 1000u) % 10u);
    s[4] = (char)('0' + (app.log_num / 100u) % 10u);
    s[5] = (char)('0' + (app.log_num / 10u) % 10u);
    s[6] = (char)('0' + app.log_num % 10u);
    s[7] = '.';
    s[8] = 'M';
    s[9] = 'S';
    s[10] = 'L';
    ui_textn(8, 3, s, 11, app.logging ? PAL_TEXT : PAL_DIM);

    fmt_time(s, app.log_s);
    ui_textn(8, 5, s, 5, PAL_TEXT);

    if (kb < 1024u) {
        fmt_int(s, 4, (int16_t)kb, 0);
        s[4] = ' ';
        s[5] = 'K';
        s[6] = 'B';
    } else {
        fmt_x10(s, 4, (int16_t)(kb * 10u / 1024u), 0);
        s[4] = ' ';
        s[5] = 'M';
        s[6] = 'B';
    }
    ui_textn(8, 9, s, 7, PAL_TEXT);

    ui_text(1, 16, app.logging ? "A:STOP " : "A:START", PAL_DIM);
}

void logpage_input(uint8_t pressed) {
    if (pressed & J_A) {
        if (app.logging) {
            app_log_stop();
        } else {
            app_log_start();
        }
        logpage_tick();
    }
}
