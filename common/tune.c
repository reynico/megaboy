#include "tune.h"

static const uint8_t limit_offs[] = {2u, 6u, 20u, 128u};

const tune_table_t tune_tables[TUNE_COUNT] = {
    {0x01u, 12u, 12u, 0u, 158u, 170u, 10u, 250u, 0},
    {0x03u, 12u, 12u, 0u, 144u, 156u, 0u, 156u, 0},
    {0x06u, 8u, 8u, 0u, 64u, 72u, 26u, 230u, 0},
    {0x00u, 4u, 1u, 0u, 0u, 0u, 20u, 90u, limit_offs},
    {0x07u, 6u, 6u, 0u, 36u, 42u, 90u, 250u, 0},
    {0x07u, 6u, 6u, 48u, 84u, 90u, 0u, 242u, 0},
};

const tune_table_t *tune_get(uint8_t id) {
    return id < TUNE_COUNT ? &tune_tables[id] : 0;
}

uint8_t tune_cells(const tune_table_t *t) {
    return (uint8_t)(t->rows * t->cols);
}

uint8_t tune_blob_len(const tune_table_t *t) {
    if (t->offs) {
        return tune_cells(t);
    }
    return (uint8_t)(tune_cells(t) + t->cols + t->rows);
}

uint8_t tune_cell_offset(const tune_table_t *t, uint8_t cell) {
    return t->offs ? t->offs[cell] : (uint8_t)(t->val_off + cell);
}

uint8_t tune_blob_offset(const tune_table_t *t, uint8_t i) {
    uint8_t n = tune_cells(t);
    if (i < n) {
        return tune_cell_offset(t, i);
    }
    if (i < (uint8_t)(n + t->cols)) {
        return (uint8_t)(t->rpm_off + (i - n));
    }
    return (uint8_t)(t->map_off + (i - n - t->cols));
}

uint8_t tune_check_write(uint8_t id, uint8_t cell, uint8_t cur, uint8_t val) {
    const tune_table_t *t = tune_get(id);
    uint8_t d;
    if (!t) {
        return TUNE_E_TABLE;
    }
    if (cell >= tune_cells(t)) {
        return TUNE_E_CELL;
    }
    if (val < t->min || val > t->max) {
        return TUNE_E_RANGE;
    }
    d = val > cur ? (uint8_t)(val - cur) : (uint8_t)(cur - val);
    if (d > TUNE_MAX_DELTA) {
        return TUNE_E_DELTA;
    }
    return TUNE_OK;
}
