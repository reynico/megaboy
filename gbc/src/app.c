#include "app.h"

#include "sim.h"

app_t app;

void app_init(void) {
    sim_init();
    sim_step(&app.rt);
    app.peak_x10 = 0;
    app.uptime_s = 0;
    app.blink = 0;
}

void app_tick(void) {
    sim_step(&app.rt);
    if (app.rt.boost_psi_x10 > app.peak_x10) {
        app.peak_x10 = app.rt.boost_psi_x10;
    }
}

void app_second(void) {
    app.uptime_s++;
}
