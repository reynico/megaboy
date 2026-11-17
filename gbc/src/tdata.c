#include "tdata.h"

#include "link.h"
#include "linkrx.h"

#define TIMEOUT_TICKS 30u
#define MAX_RETRIES 2u
#define NO_CMD 0u
#define DISC_TRIES 8u

tdata_t td;

static uint8_t pend_cmd;
static uint8_t pend_pl[LINK_WRITE_LEN];
static uint8_t pend_len;
static uint8_t pend_age;
static uint8_t pend_tries;
static uint16_t got;
static uint16_t disc_from;
static uint8_t disc_tries;
static uint8_t keep_orig;
static uint8_t burn_req;

static void send_pending(void) {
    if (linkrx_send(pend_cmd, pend_pl, pend_len)) {
        pend_age = 0;
        pend_tries++;
    }
}

static void start(uint8_t c, uint8_t len) {
    pend_cmd = c;
    pend_pl[0] = td.id;
    pend_len = len;
    pend_tries = 0;
    pend_age = TIMEOUT_TICKS;
    send_pending();
}

void td_load(uint8_t ecu, uint8_t id) {
    td.ecu = ecu;
    td.id = id;
    td.t = tune_get(ecu, id);
    td.err = TUNE_OK;
    td.tgt_cell = TD_NO_CELL;
    td.discarding = 0;
    td.burned = 0;
    td.changed = 1;
    td.unsure = 0;
    td.loaded = 0;
    keep_orig = 0;
    burn_req = 0;
    got = 0;
    if (!td.t) {
        td.state = TD_ERROR;
        td.err = TUNE_E_TABLE;
        return;
    }
    td.state = TD_LOADING;
    start(LINK_C_READ, 1);
}

void td_resync(void) {
    if (!td.loaded) {
        td_load(td.ecu, td.id);
        return;
    }
    td.state = TD_LOADING;
    td.err = TUNE_OK;
    td.tgt_cell = TD_NO_CELL;
    td.discarding = 0;
    td.changed = 1;
    keep_orig = 1;
    burn_req = 0;
    got = 0;
    start(LINK_C_READ, 1);
}

uint8_t td_busy(void) {
    return pend_cmd != NO_CMD || td.tgt_cell != TD_NO_CELL || burn_req;
}

uint8_t td_dirty(void) {
    uint16_t i;
    uint16_t n;
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

int16_t td_value(uint16_t cell) {
    if (cell == td.tgt_cell) {
        return td.tgt_val;
    }
    return td.blob[cell];
}

uint8_t td_set(uint16_t cell, int16_t val) {
    if (td.state != TD_READY || td.discarding || td.unsure || cell >= tune_cells(td.t)) {
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
    if (tune_check_order(td.t, td.blob, cell, val) != TUNE_OK) {
        td.err = TUNE_E_ORDER;
        td.changed = 1;
        return 0;
    }
    td.tgt_cell = cell;
    td.tgt_val = val;
    td.err = TUNE_OK;
    td.burned = 0;
    td.changed = 1;
    return 1;
}

static void next_step(void);

void td_burn(void) {
    if (td.state == TD_READY && !td.unsure) {
        td.err = TUNE_OK;
        burn_req = 1;
        next_step();
    }
}

void td_discard(void) {
    if (td.state == TD_READY) {
        td.discarding = 1;
        td.changed = 1;
        disc_from = 0;
        disc_tries = 0;
    }
}

void td_on_table(const uint8_t *p, uint8_t len) {
    uint16_t off;
    uint8_t n;
    uint8_t i;
    uint16_t blen;
    if (td.state != TD_LOADING || len < LINK_TABLE_HDR || p[0] != td.id || (len - LINK_TABLE_HDR) & 1u) {
        return;
    }
    off = (uint16_t)(p[1] | (uint16_t)p[2] << 8);
    n = (uint8_t)((len - LINK_TABLE_HDR) >> 1);
    blen = tune_blob_len(td.t);
    if (off > blen || n > blen - off) {
        return;
    }
    for (i = 0; i < n; i++) {
        td.blob[off + i] = link_get_s16(p + LINK_TABLE_HDR + i * 2u);
    }
    if (off == got) {
        got = (uint16_t)(got + n);
    }
}

static void ack_read(uint8_t st) {
    uint16_t i;
    if (st != TUNE_OK || got != tune_blob_len(td.t)) {
        td.state = TD_ERROR;
        td.err = st != TUNE_OK ? st : TUNE_E_ECU;
        return;
    }
    if (!keep_orig) {
        for (i = 0; i < tune_cells(td.t); i++) {
            td.orig[i] = td.blob[i];
        }
    }
    keep_orig = 0;
    td.loaded = 1;
    td.unsure = 0;
    td.state = TD_READY;
}

void td_on_ack(const uint8_t *p) {
    uint8_t c = p[0];
    uint8_t cell = p[2];
    uint8_t st = p[3];
    int16_t v = link_get_s16(p + 4);
    uint8_t verified = pend_pl[4] & LINK_WF_VERIFY;
    uint16_t i;
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
        if (cell < tune_cells(td.t)) {
            td.blob[cell] = v;
        }
        if (st == TUNE_E_ORDER && td.discarding && disc_tries < DISC_TRIES) {
            disc_tries++;
            disc_from = (uint16_t)(cell + 1u);
            td.tgt_cell = TD_NO_CELL;
        } else if (st != TUNE_OK) {
            td.err = st;
            td.tgt_cell = TD_NO_CELL;
            td.discarding = 0;
            burn_req = 0;
        } else if (verified && cell == td.tgt_cell && v == td.tgt_val) {
            td.tgt_cell = TD_NO_CELL;
        }
        next_step();
        return;
    }
    if (c == LINK_C_BURN) {
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

static int16_t step_to(int16_t cur, int16_t want) {
    int16_t d = td.t->delta;
    if (want > cur) {
        return want - cur > d ? (int16_t)(cur + d) : want;
    }
    return cur - want > d ? (int16_t)(cur - d) : want;
}

static void next_discard(void) {
    uint16_t k;
    uint16_t i;
    uint16_t n = tune_cells(td.t);
    for (k = 0; k < n; k++) {
        i = (uint16_t)((disc_from + k) % n);
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
    int16_t v;
    if (td.state != TD_READY || pend_cmd != NO_CMD) {
        return;
    }
    if (td.discarding && td.tgt_cell == TD_NO_CELL) {
        next_discard();
    }
    if (td.tgt_cell == TD_NO_CELL) {
        if (burn_req && !td.discarding) {
            burn_req = 0;
            start(LINK_C_BURN, 1);
        }
        return;
    }
    v = step_to(td.blob[td.tgt_cell], td.tgt_val);
    pend_pl[1] = (uint8_t)td.tgt_cell;
    link_put_s16(pend_pl + 2, v);
    pend_pl[4] = v == td.tgt_val ? LINK_WF_VERIFY : 0u;
    start(LINK_C_WRITE, LINK_WRITE_LEN);
}

void td_tick(void) {
    if (pend_cmd != NO_CMD) {
        if (pend_age < TIMEOUT_TICKS) {
            pend_age++;
            return;
        }
        if (pend_tries > MAX_RETRIES) {
            if (pend_cmd != LINK_C_READ) {
                td.unsure = 1;
            }
            pend_cmd = NO_CMD;
            burn_req = 0;
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
