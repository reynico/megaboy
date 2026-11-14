#include "fmt.h"

static void emit(char *out, uint8_t w, const char *s, uint8_t n) {
    uint8_t i;
    if (n > w) {
        for (i = 0; i < w; i++) {
            out[i] = '*';
        }
        return;
    }
    for (i = 0; i < w - n; i++) {
        out[i] = ' ';
    }
    for (i = 0; i < n; i++) {
        out[w - n + i] = s[i];
    }
}

static const uint16_t pow10[4] = {10000u, 1000u, 100u, 10u};

#define DIGIT(p)                       \
    d = 0;                             \
    while (v >= (p)) {                 \
        v -= (p);                      \
        d++;                           \
    }                                  \
    if (d || n) {                      \
        buf[n++] = (char)('0' + d);    \
    }

static uint8_t digits(char *buf, uint16_t v) {
    uint8_t n = 0;
    uint8_t d = 0;
    while (v >= 10000u) {
        v -= 10000u;
        d++;
    }
    if (d) {
        buf[n++] = (char)('0' + d);
    }
    DIGIT(1000u)
    DIGIT(100u)
    DIGIT(10u)
    buf[n++] = (char)('0' + (uint8_t)v);
    return n;
}

static const uint16_t pow10_q[4] = {1000u, 100u, 10u, 1u};

static uint16_t div10(uint16_t v, uint8_t *rem) {
    uint16_t q = 0;
    uint8_t i;
    for (i = 0; i < 4u; i++) {
        while (v >= pow10[i]) {
            v -= pow10[i];
            q += pow10_q[i];
        }
    }
    *rem = (uint8_t)v;
    return q;
}

static uint16_t mag(int16_t v) {
    return v < 0 ? (uint16_t)(0u - (uint16_t)v) : (uint16_t)v;
}

int16_t fmt_round_x10(int16_t v) {
    uint8_t r;
    uint16_t q = div10((uint16_t)(mag(v) + 5u), &r);
    return v < 0 ? (int16_t)(0u - q) : (int16_t)q;
}

void fmt_int(char *out, uint8_t w, int16_t v, char suffix) {
    char s[8];
    uint8_t n = 0;
    if (v < 0) {
        s[n++] = '-';
    }
    n += digits(s + n, mag(v));
    if (suffix) {
        s[n++] = suffix;
    }
    emit(out, w, s, n);
}

void fmt_x10(char *out, uint8_t w, int16_t v, uint8_t plus) {
    char s[9];
    uint8_t n = 0;
    uint8_t r;
    uint16_t a = mag(v);
    if (v < 0) {
        s[n++] = '-';
    } else if (plus) {
        s[n++] = '+';
    }
    n += digits(s + n, div10(a, &r));
    s[n++] = '.';
    s[n++] = (char)('0' + r);
    emit(out, w, s, n);
}

void fmt_time(char *out, uint16_t sec) {
    uint16_t m = sec / 60u;
    uint8_t ss = (uint8_t)(sec % 60u);
    if (m > 99u) {
        m = 99u;
        ss = 59u;
    }
    out[0] = (char)('0' + m / 10u);
    out[1] = (char)('0' + m % 10u);
    out[2] = ':';
    out[3] = (char)('0' + ss / 10u);
    out[4] = (char)('0' + ss % 10u);
}

uint8_t fmt_bar_px(int16_t value, uint16_t max, uint8_t w) {
    uint16_t v;
    if (value <= 0 || max == 0u) {
        return 0;
    }
    v = (uint16_t)value > max ? max : (uint16_t)value;
    while (max > 65535u / 140u) {
        max >>= 1;
        v >>= 1;
    }
    return (uint8_t)((uint16_t)(v * (uint16_t)(w * 7u)) / max);
}
