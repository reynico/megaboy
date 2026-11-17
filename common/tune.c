#include "tune.h"

static const tune_item_t ms1_limits[] = {{0x00u, 2u}, {0x00u, 6u}, {0x00u, 20u}, {0x00u, 128u}};
static const tune_item_t ms2_limits[] = {{0x04u, 656u}, {0x0au, 678u}, {0x0au, 685u}, {0x0au, 691u}};
static const tune_order_t ms1_order[] = {{0u, 1u}, {2u, 1u}, {3u, 1u}};
static const tune_order_t ms2_order[] = {{1u, 0u}, {3u, 0u}};

static const tune_table_t ms1_tables[TUNE_COUNT] = {
    {0x01u, 12u, 12u, TUNE_U08, TUNE_U08, TUNE_U08, 0u, 158u, 170u, 10, 250, 10, 1, TUNE_F_INT, TUNE_A_X100,
     TUNE_A_RAW, 0u, 0, 0, 0u},
    {0x03u, 12u, 12u, TUNE_U08, TUNE_U08, TUNE_U08, 0u, 144u, 156u, 0, 156, 10, 1, TUNE_F_MS1_ADV, TUNE_A_X100,
     TUNE_A_RAW, 0u, 0, 0, 0u},
    {0x06u, 8u, 8u, TUNE_U08, TUNE_U08, TUNE_U08, 0u, 64u, 72u, 26, 230, 10, 1, TUNE_F_MS1_AFR, TUNE_A_X100,
     TUNE_A_RAW, 0u, 0, 0, 0u},
    {0x00u, 4u, 1u, TUNE_U08, TUNE_U08, TUNE_U08, 0u, 0u, 0u, 20, 90, 10, 1, TUNE_F_X100, TUNE_A_RAW, TUNE_A_RAW, 0u,
     ms1_limits, ms1_order, 3u},
    {0x07u, 6u, 6u, TUNE_U08, TUNE_U08, TUNE_U08, 0u, 36u, 42u, 90, 250, 10, 1, TUNE_F_INT, TUNE_A_X100,
     TUNE_A_MS1_TPS, 1u, 0, 0, 0u},
    {0x07u, 6u, 6u, TUNE_U08, TUNE_U08, TUNE_U08, 48u, 84u, 90u, 0, 242, 10, 1, TUNE_F_MS1_DUTY, TUNE_A_X100,
     TUNE_A_MS1_TPS, 1u, 0, 0, 0u},
};

static const tune_table_t ms2_tables[TUNE_COUNT] = {
    {0x09u, 16u, 16u, TUNE_U08, TUNE_U16, TUNE_S16, 0u, 768u, 864u, 10, 250, 10, 1, TUNE_F_INT, TUNE_A_RAW,
     TUNE_A_DIV10, 0u, 0, 0, 0u},
    {0x0au, 12u, 12u, TUNE_S16, TUNE_U16, TUNE_S16, 0u, 576u, 624u, -100, 500, 30, 5, TUNE_F_X10, TUNE_A_RAW,
     TUNE_A_DIV10, 0u, 0, 0, 0u},
    {0x04u, 12u, 12u, TUNE_U08, TUNE_U16, TUNE_S16, 48u, 374u, 422u, 90, 200, 10, 1, TUNE_F_X10, TUNE_A_RAW,
     TUNE_A_DIV10, 0u, 0, 0, 0u},
    {0x04u, 4u, 1u, TUNE_S16, TUNE_S16, TUNE_S16, 0u, 0u, 0u, 2000, 9000, 1000, 100, TUNE_F_INT, TUNE_A_RAW,
     TUNE_A_RAW, 0u, ms2_limits, ms2_order, 2u},
    {0x05u, 8u, 8u, TUNE_S16, TUNE_U16, TUNE_S16, 76u, 220u, 204u, 900, 2500, 100, 10, TUNE_F_X10, TUNE_A_RAW,
     TUNE_A_DIV10, 1u, 0, 0, 0u},
    {0x05u, 8u, 8u, TUNE_U08, TUNE_U16, TUNE_S16, 236u, 316u, 300u, 0, 100, 10, 1, TUNE_F_INT, TUNE_A_RAW,
     TUNE_A_DIV10, 1u, 0, 0, 0u},
};

const tune_table_t *tune_get(uint8_t ecu, uint8_t id) {
    if (id >= TUNE_COUNT) {
        return 0;
    }
    if (ecu == TUNE_ECU_MS1) {
        return &ms1_tables[id];
    }
    if (ecu == TUNE_ECU_MS2) {
        return &ms2_tables[id];
    }
    return 0;
}

uint16_t tune_cells(const tune_table_t *t) {
    return (uint16_t)t->rows * t->cols;
}

uint16_t tune_blob_len(const tune_table_t *t) {
    if (t->items) {
        return tune_cells(t);
    }
    return (uint16_t)(tune_cells(t) + t->cols + t->rows);
}

uint8_t tune_size(uint8_t type) {
    return type == TUNE_U08 ? 1u : 2u;
}

void tune_loc(const tune_table_t *t, uint16_t i, uint8_t *page, uint16_t *off, uint8_t *type) {
    uint16_t n = tune_cells(t);
    *page = t->page;
    if (i < n) {
        *type = t->type;
        if (t->items) {
            *page = t->items[i].page;
            *off = t->items[i].off;
        } else {
            *off = (uint16_t)(t->val_off + i * tune_size(t->type));
        }
    } else if (i < n + t->cols) {
        *type = t->xtype;
        *off = (uint16_t)(t->x_off + (i - n) * tune_size(t->xtype));
    } else {
        *type = t->ytype;
        *off = (uint16_t)(t->y_off + (i - n - t->cols) * tune_size(t->ytype));
    }
}

int16_t tune_decode(uint8_t type, const uint8_t *p) {
    if (type == TUNE_U08) {
        return p[0];
    }
    return (int16_t)((uint16_t)p[0] << 8 | p[1]);
}

void tune_encode(uint8_t type, int16_t v, uint8_t *p) {
    if (type == TUNE_U08) {
        p[0] = (uint8_t)v;
        return;
    }
    p[0] = (uint8_t)((uint16_t)v >> 8);
    p[1] = (uint8_t)v;
}

int16_t tune_axis(uint8_t axis, int16_t raw) {
    switch (axis) {
    case TUNE_A_X100:
        return (int16_t)(raw * 100);
    case TUNE_A_DIV10:
        return (int16_t)(raw / 10);
    case TUNE_A_MS1_TPS:
        return (int16_t)(((int32_t)raw * 392 + 500) / 1000);
    default:
        return raw;
    }
}

uint8_t tune_check_write(const tune_table_t *t, uint16_t cell, int16_t cur, int16_t val) {
    int32_t d;
    if (!t) {
        return TUNE_E_TABLE;
    }
    if (cell >= tune_cells(t)) {
        return TUNE_E_CELL;
    }
    if (val < t->min || val > t->max) {
        return TUNE_E_RANGE;
    }
    d = (int32_t)val - cur;
    if (d < 0) {
        d = -d;
    }
    if (d > t->delta) {
        return TUNE_E_DELTA;
    }
    return TUNE_OK;
}

uint8_t tune_check_order(const tune_table_t *t, const int16_t *vals, uint16_t cell, int16_t val) {
    uint8_t i;
    for (i = 0; i < t->norder; i++) {
        const tune_order_t *o = &t->order[i];
        int16_t lo = o->lo == cell ? val : vals[o->lo];
        int16_t hi = o->hi == cell ? val : vals[o->hi];
        if (lo > hi) {
            return TUNE_E_ORDER;
        }
    }
    return TUNE_OK;
}
