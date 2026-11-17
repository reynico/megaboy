#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t *buf;
    unsigned size;
    unsigned cap;
    unsigned head;
    unsigned n;
} queue_t;

static inline void queue_init(queue_t *q, unsigned size, unsigned cap) {
    q->buf = calloc(cap, size);
    q->size = size;
    q->cap = cap;
    q->head = 0;
    q->n = 0;
}

static inline bool queue_try_add(queue_t *q, const void *d) {
    if (q->n == q->cap) {
        return false;
    }
    memcpy(q->buf + ((q->head + q->n) % q->cap) * q->size, d, q->size);
    q->n++;
    return true;
}

static inline bool queue_try_remove(queue_t *q, void *d) {
    if (!q->n) {
        return false;
    }
    memcpy(d, q->buf + q->head * q->size, q->size);
    q->head = (q->head + 1u) % q->cap;
    q->n--;
    return true;
}
