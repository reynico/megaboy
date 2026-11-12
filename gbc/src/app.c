#include "app.h"

#include "sim.h"
#include "tdata.h"

app_t app;
static uint8_t was_running;
static uint8_t acked;
static uint8_t clear_ticks[W_COUNT];

#define WARN_CLEAR_TICKS 30u
#define LEAN_MIN_BOOST_X10 20
#define LEAN_MIN_TPS_X10 500

uint8_t app_warn_active(uint8_t id) {
    const rt_t *rt = &app.rt;
    switch (id) {
    case W_CLT:
        return rt->clt_c_x10 >= app.set.clt_warn_c * 10;
    case W_LEAN:
        return rt->boost_psi_x10 > LEAN_MIN_BOOST_X10 && rt->tps_x10 >= LEAN_MIN_TPS_X10 &&
               rt->afr_x10 > app.set.afr_lean_x10;
    case W_BOOST:
        return rt->boost_psi_x10 >= app.set.boost_warn_x10;
    default:
        return 0;
    }
}

uint8_t app_warning(void) {
    uint8_t id;
    for (id = 1; id < W_COUNT; id++) {
        if (app_warn_active(id) && !(acked & (uint8_t)(1u << id))) {
            return id;
        }
    }
    return W_NONE;
}

void app_warn_ack(uint8_t id) {
    acked |= (uint8_t)(1u << id);
    clear_ticks[id] = 0;
}

static void warn_update(void) {
    uint8_t id;
    for (id = 1; id < W_COUNT; id++) {
        if (app_warn_active(id)) {
            clear_ticks[id] = 0;
        } else if (clear_ticks[id] < WARN_CLEAR_TICKS) {
            clear_ticks[id]++;
        } else {
            acked &= (uint8_t)~(1u << id);
        }
    }
}

static void peak_update(void) {
    if (app.rt.boost_psi_x10 > app.peak_x10) {
        app.peak_x10 = app.rt.boost_psi_x10;
    }
}

void app_init(void) {
    sim_init();
    sim_step(&app.rt);
    app.set.clt_warn_c = 100;
    app.set.afr_lean_x10 = 130;
    app.set.rpm_shift = 6800;
    app.set.boost_warn_x10 = 200;
    app.set.auto_log = 1;
    app.set.alarm_snd = 1;
    app.peak_x10 = 0;
    app.uptime_s = 0;
    app.blink = 0;
    app.logging = 0;
    app.log_s = 0;
    app.log_num = 41;
    app.log_bytes = 0;
    app.src = SRC_SIM;
    app.link_age = 0;
    app.rt_age = 0;
    app.ecu = 0;
    app.ecu_state = 0;
    app.rt_new = 0;
}

uint8_t app_ecu_missing(void) {
    return app.src == SRC_LINK && app.rt_age >= RT_STALE_TICKS;
}

static void link_age_step(void) {
    if (app.link_age < 255u) {
        app.link_age++;
    }
    if (app.rt_age < 255u) {
        app.rt_age++;
    }
    if (app.link_age >= LINK_LOST_TICKS) {
        app.src = SRC_LOST;
    }
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
    if (app.src == SRC_SIM) {
        sim_step(&app.rt);
    } else {
        link_age_step();
    }
    running = app.rt.rpm > 0u;
    if (app.set.auto_log && running && !was_running) {
        app_log_start();
    }
    was_running = running;
    warn_update();
    peak_update();
    td_tick();
}

void app_rt_received(void) {
    app.rt_age = 0;
    app.rt_new = 1;
    peak_update();
}

void app_second(void) {
    app.uptime_s++;
    if (app.logging) {
        app.log_s++;
        app.log_bytes += LOG_BYTES_PER_S;
    }
}
