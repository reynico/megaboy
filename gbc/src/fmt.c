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

static uint8_t digits(char *buf, uint16_t v) {
    char tmp[5];
    uint8_t n = 0;
    uint8_t i;
    do {
        tmp[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v != 0u);
    for (i = 0; i < n; i++) {
        buf[i] = tmp[n - 1u - i];
    }
    return n;
}

static uint16_t mag(int16_t v) {
    return v < 0 ? (uint16_t)(-(int32_t)v) : (uint16_t)v;
}

int16_t fmt_round_x10(int16_t v) {
    int32_t x = v;
    return (int16_t)(x >= 0 ? (x + 5) / 10 : (x - 5) / 10);
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
    uint16_t a = mag(v);
    if (v < 0) {
        s[n++] = '-';
    } else if (plus) {
        s[n++] = '+';
    }
    n += digits(s + n, a / 10u);
    s[n++] = '.';
    s[n++] = (char)('0' + a % 10u);
    emit(out, w, s, n);
}
