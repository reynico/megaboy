#pragma once

static inline void multicore_launch_core1(void (*entry)(void)) {
    (void)entry;
}

static inline void multicore_lockout_victim_init(void) {
}
