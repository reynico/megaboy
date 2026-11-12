#include "app.h"

#include "sim.h"

app_t app;
static uint8_t was_running;

void app_init(void) {
    sim_init();
    sim_step(&app.rt);
    app.set.clt_warn_c = 100;
    app.set.afr_lean_x10 = 130;
    app.set.rpm_shift = 6800;
    app.set.boost_warn_x10 = 200;
    app.set.auto_log = 1;
    app.peak_x10 = 0;
    app.uptime_s = 0;
    app.blink = 0;
    app.logging = 0;
    app.log_s = 0;
    app.log_num = 41;
    app.log_bytes = 0;
}

void app_log_start(void) {
    if (app.logging) {
        return;
    }
    app.logging = 1;
    app.log_num++;
    app.log_s = 0;
    app.log_bytes = 0;
}

void app_log_stop(void) {
    app.logging = 0;
}

void app_tick(void) {
    uint8_t running;
    sim_step(&app.rt);
    running = app.rt.rpm > 0u;
    if (app.set.auto_log && running && !was_running) {
        app_log_start();
    }
    was_running = running;
    if (app.rt.boost_psi_x10 > app.peak_x10) {
        app.peak_x10 = app.rt.boost_psi_x10;
    }
}

void app_second(void) {
    app.uptime_s++;
    if (app.logging) {
        app.log_s++;
        app.log_bytes += LOG_BYTES_PER_S;
    }
}
