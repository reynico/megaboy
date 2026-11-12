#include <stdio.h>
#include <string.h>

#include "link.h"

static int fails;

#define CHECK(c)                                                  \
    do {                                                          \
        if (!(c)) {                                               \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); \
            fails++;                                              \
        }                                                         \
    } while (0)

static int feed_all(link_parser_t *p, const uint8_t *d, size_t n) {
    int frames = 0;
    for (size_t i = 0; i < n; i++) {
        frames += link_feed(p, d[i]);
    }
    return frames;
}

static void test_rt_roundtrip(void) {
    link_rt_t a = {1534, -25, 980, 472, 556, 50, 107, 109, -12, 1, 0x80};
    link_rt_t b;
    uint8_t pl[LINK_MAX_PAYLOAD];
    uint8_t fr[LINK_FRAME_MAX];
    link_parser_t p;
    uint8_t n = link_put_rt(pl, &a);
    uint8_t fl = link_encode(fr, LINK_T_RT, pl, n);
    CHECK(n == LINK_RT_LEN);
    CHECK(fl == LINK_RT_LEN + 5);
    CHECK(fr[0] == 0x4d && fr[1] == 0x42 && fr[2] == LINK_T_RT && fr[3] == LINK_RT_LEN);
    CHECK(fr[4] == (1534 & 0xff) && fr[5] == (1534 >> 8));
    link_parser_reset(&p);
    CHECK(feed_all(&p, fr, fl) == 1);
    CHECK(p.type == LINK_T_RT && p.len == LINK_RT_LEN);
    link_get_rt(p.buf, &b);
    CHECK(memcmp(&a, &b, sizeof a) == 0);
}

static void test_status(void) {
    link_status_t a = {LINK_ST_OK, LINK_ECU_MS1};
    link_status_t b;
    uint8_t pl[LINK_STATUS_LEN];
    uint8_t fr[LINK_FRAME_MAX];
    link_parser_t p;
    uint8_t fl = link_encode(fr, LINK_T_STATUS, pl, link_put_status(pl, &a));
    link_parser_reset(&p);
    CHECK(feed_all(&p, fr, fl) == 1);
    link_get_status(p.buf, &b);
    CHECK(b.state == LINK_ST_OK && b.ecu == LINK_ECU_MS1);
}

static void test_noise_and_errors(void) {
    link_rt_t a = {3000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t pl[LINK_MAX_PAYLOAD];
    uint8_t fr[LINK_FRAME_MAX];
    uint8_t buf[3 * LINK_FRAME_MAX];
    link_parser_t p;
    uint8_t fl = link_encode(fr, LINK_T_RT, pl, link_put_rt(pl, &a));
    const uint8_t noise[] = {0xff, 0x4d, 0x4d, 0x00, 0xff};

    link_parser_reset(&p);
    memcpy(buf, noise, sizeof noise);
    memcpy(buf + 1, fr, fl);
    CHECK(feed_all(&p, buf, fl + 1) == 1);

    link_parser_reset(&p);
    CHECK(feed_all(&p, noise, sizeof noise) == 0);
    CHECK(feed_all(&p, fr, fl) == 1);

    fr[10] ^= 0x01;
    link_parser_reset(&p);
    CHECK(feed_all(&p, fr, fl) == 0);
    fr[10] ^= 0x01;
    CHECK(feed_all(&p, fr, fl) == 1);

    const uint8_t big[] = {0x4d, 0x42, 0x01, LINK_MAX_PAYLOAD + 1};
    link_parser_reset(&p);
    CHECK(feed_all(&p, big, sizeof big) == 0);
    CHECK(feed_all(&p, fr, fl) == 1);

    memcpy(buf, fr, fl);
    memcpy(buf + fl, fr, fl);
    link_parser_reset(&p);
    CHECK(feed_all(&p, buf, 2u * fl) == 2);
}

int main(void) {
    test_rt_roundtrip();
    test_status();
    test_noise_and_errors();
    if (fails) {
        fprintf(stderr, "%d failures\n", fails);
        return 1;
    }
    puts("ok");
    return 0;
}
