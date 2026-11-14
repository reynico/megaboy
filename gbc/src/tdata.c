#include "tdata.h"

#include "link.h"
#include "linkrx.h"

#define TIMEOUT_TICKS 30u
#define MAX_RETRIES 2u
#define NO_CMD 0u

tdata_t td;

static uint8_t pend_cmd;
static uint8_t pend_pl[LINK_WRITE_LEN];
static uint8_t pend_len;
static uint8_t pend_age;
static uint8_t pend_tries;
static uint8_t got;

static void send_pending(void) {
    if (linkrx_send(pend_cmd, pend_pl, pend_len)) {
        pend_age = 0;
        pend_tries++;
    }
}

static void start(uint8_t c, uint8_t a, uint8_t b, uint8_t v, uint8_t f, uint8_t len) {
    pend_cmd = c;
    pend_pl[0] = a;
    pend_pl[1] = b;
    pend_pl[2] = v;
    pend_pl[3] = f;
    pend_len = len;
    pend_tries = 0;
    pend_age = TIMEOUT_TICKS;
    send_pending();
}

void td_load(uint8_t id) {
    td.id = id;
    td.t = tune_get(id);
    td.state = TD_LOADING;
    td.err = TUNE_OK;
    td.tgt_cell = TD_NO_CELL;
    td.discarding = 0;
    td.burned = 0;
    td.changed = 1;
    got = 0;
    start(LINK_C_READ, id, 0, 0, 0, 1);
}

uint8_t td_busy(void) {
    return pend_cmd != NO_CMD || td.tgt_cell != TD_NO_CELL;
}

uint8_t td_dirty(void) {
    uint8_t i;
    uint8_t n;
    if (td.state != TD_READY) {
        return 0;
    }
    n = tune_cells(td.t);
    for (i = 0; i < n; i++) {
        if (td.blob[i] != td.orig[i]) {
            return 1;
        }
    }
    return 0;
}

uint8_t td_value(uint8_t cell) {
    if (cell == td.tgt_cell) {
        return td.tgt_val;
    }
    return td.blob[cell];
}

uint8_t td_set(uint8_t cell, uint8_t val) {
    if (td.state != TD_READY || td.discarding) {
        return 0;
    }
    if (td.tgt_cell != TD_NO_CELL && td.tgt_cell != cell) {
        return 0;
    }
    if (val < td.t->min) {
        val = td.t->min;
    }
    if (val > td.t->max) {
        val = td.t->max;
    }
    td.tgt_cell = cell;
    td.tgt_val = val;
    td.err = TUNE_OK;
    td.burned = 0;
    td.changed = 1;
    return 1;
}

void td_burn(void) {
    if (td.state == TD_READY && !td_busy()) {
        td.err = TUNE_OK;
        start(LINK_C_BURN, td.id, 0, 0, 0, 1);
    }
}

void td_discard(void) {
    if (td.state == TD_READY) {
        td.discarding = 1;
        td.changed = 1;
    }
}

void td_on_table(const uint8_t *p, uint8_t len) {
    uint8_t off;
    uint8_t n;
    uint8_t i;
    if (td.state != TD_LOADING || len < 2u || p[0] != td.id) {
        return;
    }
    off = p[1];
    n = (uint8_t)(len - 2u);
    if ((uint16_t)off + n > tune_blob_len(td.t)) {
        return;
    }
    for (i = 0; i < n; i++) {
        td.blob[off + i] = p[2 + i];
    }
    if (off == got) {
        got = (uint8_t)(got + n);
    }
}

static void ack_read(uint8_t st) {
    uint8_t i;
    if (st != TUNE_OK || got != tune_blob_len(td.t)) {
        td.state = TD_ERROR;
        td.err = st != TUNE_OK ? st : TUNE_E_ECU;
        return;
    }
    for (i = 0; i < tune_cells(td.t); i++) {
        td.orig[i] = td.blob[i];
    }
    td.state = TD_READY;
}

static void next_step(void);

void td_on_ack(const uint8_t *p) {
    uint8_t c = p[0];
    uint8_t st = p[3];
    uint8_t verified = pend_pl[3] & LINK_WF_VERIFY;
    if (c != pend_cmd || p[1] != td.id) {
        return;
    }
    pend_cmd = NO_CMD;
    td.changed = 1;
    if (c == LINK_C_READ) {
        ack_read(st);
        return;
    }
    if (c == LINK_C_WRITE) {
        if (p[2] < tune_cells(td.t)) {
            td.blob[p[2]] = p[4];
        }
        if (st != TUNE_OK) {
            td.err = st;
            td.tgt_cell = TD_NO_CELL;
            td.discarding = 0;
        } else if (verified && p[2] == td.tgt_cell && p[4] == td.tgt_val) {
            td.tgt_cell = TD_NO_CELL;
        }
        next_step();
        return;
    }
    if (c == LINK_C_BURN) {
        uint8_t i;
        if (st != TUNE_OK) {
            td.err = st;
            return;
        }
        for (i = 0; i < tune_cells(td.t); i++) {
            td.orig[i] = td.blob[i];
        }
        td.burned = 1;
    }
}

static uint8_t step_to(uint8_t cur, uint8_t want) {
    if (want > cur) {
        return want - cur > TUNE_MAX_DELTA ? (uint8_t)(cur + TUNE_MAX_DELTA) : want;
    }
    return cur - want > TUNE_MAX_DELTA ? (uint8_t)(cur - TUNE_MAX_DELTA) : want;
}

static void next_discard(void) {
    uint8_t i;
    uint8_t n = tune_cells(td.t);
    for (i = 0; i < n; i++) {
        if (td.blob[i] != td.orig[i]) {
            td.tgt_cell = i;
            td.tgt_val = td.orig[i];
            return;
        }
    }
    td.discarding = 0;
    td.changed = 1;
}

static void next_step(void) {
    uint8_t v;
    if (td.state != TD_READY || pend_cmd != NO_CMD) {
        return;
    }
    if (td.discarding && td.tgt_cell == TD_NO_CELL) {
        next_discard();
    }
    if (td.tgt_cell == TD_NO_CELL) {
        return;
    }
    v = step_to(td.blob[td.tgt_cell], td.tgt_val);
    start(LINK_C_WRITE, td.id, td.tgt_cell, v, v == td.tgt_val ? LINK_WF_VERIFY : 0u, LINK_WRITE_LEN);
}

void td_tick(void) {
    if (pend_cmd != NO_CMD) {
        if (pend_age < TIMEOUT_TICKS) {
            pend_age++;
            return;
        }
        if (pend_tries > MAX_RETRIES) {
            pend_cmd = NO_CMD;
            td.err = TUNE_E_ECU;
            td.tgt_cell = TD_NO_CELL;
            td.discarding = 0;
            if (td.state == TD_LOADING) {
                td.state = TD_ERROR;
            }
            td.changed = 1;
            return;
        }
        send_pending();
        return;
    }
    next_step();
}
