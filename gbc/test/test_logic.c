#include <stdio.h>
#include <string.h>

#include "fmt.h"

static int fails;

#define CHECK(c)                                                    \
    do {                                                            \
        if (!(c)) {                                                 \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); \
            fails++;                                                \
        }                                                           \
    } while (0)

static int eq(const char *buf, const char *want) {
    return memcmp(buf, want, strlen(want)) == 0;
}

static void test_fmt(void) {
    char b[8];
    fmt_int(b, 4, 6450, 0);
    CHECK(eq(b, "6450"));
    fmt_int(b, 4, 850, 0);
    CHECK(eq(b, " 850"));
    fmt_int(b, 4, 88, 'C');
    CHECK(eq(b, " 88C"));
    fmt_int(b, 4, -18, 'C');
    CHECK(eq(b, "-18C"));
    fmt_int(b, 4, 100, '%');
    CHECK(eq(b, "100%"));
    fmt_int(b, 3, 12345, 0);
    CHECK(eq(b, "***"));
    fmt_x10(b, 5, 142, 1);
    CHECK(eq(b, "+14.2"));
    fmt_x10(b, 5, -85, 1);
    CHECK(eq(b, " -8.5"));
    fmt_x10(b, 5, -5, 1);
    CHECK(eq(b, " -0.5"));
    fmt_x10(b, 4, 118, 0);
    CHECK(eq(b, "11.8"));
    fmt_x10(b, 4, 0, 0);
    CHECK(eq(b, " 0.0"));
    fmt_x10(b, 5, -32768, 0);
    CHECK(eq(b, "*****"));
    CHECK(fmt_round_x10(885) == 89);
    CHECK(fmt_round_x10(884) == 88);
    CHECK(fmt_round_x10(-185) == -19);
    fmt_time(b, 192);
    CHECK(eq(b, "03:12"));
    fmt_time(b, 0);
    CHECK(eq(b, "00:00"));
    fmt_time(b, 65535);
    CHECK(eq(b, "99:59"));
}

static void ref_fit(char *out, int w, const char *s) {
    int n = (int)strlen(s);
    if (n > w) {
        memset(out, '*', (size_t)w);
        return;
    }
    memset(out, ' ', (size_t)(w - n));
    memcpy(out + w - n, s, (size_t)n);
}

static void test_fmt_all(void) {
    char got[8];
    char want[8];
    char s[16];
    int bad_int = 0;
    int bad_x10 = 0;
    int bad_round = 0;
    for (long i = -32768; i <= 32767; i++) {
        int16_t v = (int16_t)i;
        long a = i < 0 ? -i : i;
        long r = i >= 0 ? (i + 5) / 10 : (i - 5) / 10;

        fmt_int(got, 6, v, 'C');
        snprintf(s, sizeof s, "%ldC", i);
        ref_fit(want, 6, s);
        bad_int += memcmp(got, want, 6) != 0;

        fmt_x10(got, 7, v, 1);
        snprintf(s, sizeof s, "%s%ld.%ld", i < 0 ? "-" : "+", a / 10, a % 10);
        ref_fit(want, 7, s);
        bad_x10 += memcmp(got, want, 7) != 0;

        bad_round += fmt_round_x10(v) != r;
    }
    CHECK(bad_int == 0);
    CHECK(bad_x10 == 0);
    CHECK(bad_round == 0);
}

static void test_bar_px(void) {
    int worst = 0;
    for (long v = -100; v <= 9000; v++) {
        for (int k = 0; k < 2; k++) {
            long max = k ? 8000 : 250;
            long c = v < 0 ? 0 : (v > max ? max : v);
            int want = (int)(c * 126 / max);
            int got = fmt_bar_px((int16_t)v, (uint16_t)max, 18);
            int d = got > want ? got - want : want - got;
            if (d > worst) {
                worst = d;
            }
            if (c == max) {
                CHECK(got == 126);
            }
            if (c == 0) {
                CHECK(got == 0);
            }
        }
    }
    CHECK(worst <= 1);
}

int main(void) {
    test_bar_px();
    test_fmt();
    test_fmt_all();
    if (fails) {
        fprintf(stderr, "%d failures\n", fails);
        return 1;
    }
    puts("ok");
    return 0;
}
