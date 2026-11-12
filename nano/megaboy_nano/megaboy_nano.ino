#include <SoftwareSerial.h>

extern "C" {
#include "ecu.h"
#include "link.h"
#include "ms1.h"
}

#define ECU_RX 10
#define ECU_TX 11
#define GB_SCK 2
#define GB_OUT 3
#define GB_IN 4
#define DEMO_PIN 5

#define GB_HALF_US 15
#define GB_GAP_US 150
#define POLL_MS 40
#define PRINT_MS 500
#define STATUS_MS 1000
#define RETRY_MS 1000
#define ECU_TIMEOUT_MS 400
#define MAX_ERRORS 10
#define LINK_F_DEMO 0x01

static SoftwareSerial ecu(ECU_RX, ECU_TX);
static ms1_cfg_t cfg;
static bool demo;
static bool ecu_ok;
static uint8_t errors;
static uint32_t last_poll;
static uint32_t last_status;
static uint32_t last_try;
static uint32_t last_print;

static uint8_t gb_xfer(uint8_t out) {
    uint8_t in = 0;
    for (uint8_t i = 0; i < 8; i++) {
        digitalWrite(GB_SCK, LOW);
        digitalWrite(GB_OUT, (out & 0x80) ? HIGH : LOW);
        out <<= 1;
        delayMicroseconds(GB_HALF_US);
        digitalWrite(GB_SCK, HIGH);
        in = (uint8_t)(in << 1) | (digitalRead(GB_IN) ? 1 : 0);
        delayMicroseconds(GB_HALF_US);
    }
    delayMicroseconds(GB_GAP_US);
    return in;
}

static void gb_send(uint8_t type, const uint8_t *payload, uint8_t len) {
    uint8_t fr[LINK_FRAME_MAX];
    uint8_t n = link_encode(fr, type, payload, len);
    for (uint8_t i = 0; i < n; i++) {
        gb_xfer(fr[i]);
    }
}

static void send_status(void) {
    link_status_t st;
    uint8_t pl[LINK_STATUS_LEN];
    st.state = (ecu_ok || demo) ? LINK_ST_OK : LINK_ST_WAIT;
    st.ecu = demo ? LINK_ECU_NONE : LINK_ECU_MS1;
    gb_send(LINK_T_STATUS, pl, link_put_status(pl, &st));
}

static void send_rt(const ecu_rt_t *r, uint8_t flags) {
    link_rt_t l;
    uint8_t pl[LINK_RT_LEN];
    l.rpm = r->rpm;
    l.adv_x10 = r->adv_x10;
    l.map_x10 = r->map_x10;
    l.iat_c_x10 = r->iat_c_x10;
    l.clt_c_x10 = r->clt_c_x10;
    l.tps_x10 = r->tps_x10;
    l.batt_x10 = r->batt_x10;
    l.afr_x10 = r->afr_x10;
    l.boost_psi_x10 = r->boost_psi_x10;
    l.engine = r->engine;
    l.flags = flags;
    gb_send(LINK_T_RT, pl, link_put_rt(pl, &l));
}

static bool ecu_cmd(const uint8_t *cmd, uint8_t clen, uint8_t *out, uint8_t n) {
    while (ecu.available()) {
        ecu.read();
    }
    ecu.write(cmd, clen);
    uint32_t t0 = millis();
    uint8_t got = 0;
    while (got < n && millis() - t0 < ECU_TIMEOUT_MS) {
        if (ecu.available()) {
            out[got++] = (uint8_t)ecu.read();
        }
    }
    if (got != n) {
        Serial.print(F("error: timeout, got "));
        Serial.print(got);
        Serial.print(F(" of "));
        Serial.println(n);
        return false;
    }
    return true;
}

static bool ecu_connect(void) {
    static const uint8_t c_sig[] = {MS1_CMD_SIG};
    static const uint8_t c_page[] = {MS1_CMD_PAGE, MS1_CONFIG_PAGE, MS1_CMD_READ};
    uint8_t buf[MS1_PAGE_LEN];

    if (!ecu_cmd(c_sig, sizeof c_sig, buf, MS1_SIG_LEN)) {
        return false;
    }
    if (!ms1_check_sig(buf, MS1_SIG_LEN)) {
        Serial.println(F("error: unexpected signature"));
        return false;
    }
    Serial.print(F("signature: "));
    Serial.write(buf, MS1_SIG_LEN);
    Serial.println();

    if (!ecu_cmd(c_page, sizeof c_page, buf, MS1_PAGE_LEN) || !ms1_parse_config(buf, MS1_PAGE_LEN, &cfg)) {
        return false;
    }
    Serial.print(F("config: "));
    Serial.print(cfg.cyl);
    Serial.print(cfg.two_stroke ? F(" cyl, two-stroke, map ") : F(" cyl, four-stroke, map "));
    Serial.print(ms1_map_kpa(cfg.map));
    Serial.println(F(" kPa"));
    return true;
}

static void print_x10(const __FlashStringHelper *name, int16_t v) {
    Serial.print(name);
    if (v < 0) {
        Serial.print('-');
        v = (int16_t)-v;
    }
    Serial.print(v / 10);
    Serial.print('.');
    Serial.print(v % 10);
}

static void print_rt(const ecu_rt_t *r) {
    Serial.print(F("rpm "));
    Serial.print(r->rpm);
    print_x10(F("  boost "), r->boost_psi_x10);
    print_x10(F("  adv "), r->adv_x10);
    print_x10(F("  clt "), r->clt_c_x10);
    print_x10(F("  iat "), r->iat_c_x10);
    print_x10(F("  map "), r->map_x10);
    print_x10(F("  tps "), r->tps_x10);
    print_x10(F("  batt "), r->batt_x10);
    print_x10(F("  afr "), r->afr_x10);
    Serial.println();
}

static void demo_rt(ecu_rt_t *r, uint32_t now) {
    uint16_t phase = (uint16_t)((now / 100) % 60);
    uint16_t ramp = phase < 30 ? phase : (uint16_t)(60 - phase);
    r->rpm = (uint16_t)(1000 + ramp * 200);
    r->engine = 1;
    r->adv_x10 = (int16_t)(100 + ramp * 8);
    r->baro_x10 = 1000;
    r->map_x10 = (int16_t)(400 + ramp * 50);
    r->iat_c_x10 = 300;
    r->clt_c_x10 = 850;
    r->tps_x10 = (int16_t)(ramp * 33);
    r->batt_x10 = 139;
    r->afr_x10 = (int16_t)(147 - ramp);
    r->boost_psi_x10 = ecu_boost_psi_x10(r->map_x10, r->baro_x10);
}

void setup() {
    Serial.begin(115200);
    pinMode(GB_SCK, OUTPUT);
    digitalWrite(GB_SCK, HIGH);
    pinMode(GB_OUT, OUTPUT);
    digitalWrite(GB_OUT, HIGH);
    pinMode(GB_IN, INPUT_PULLUP);
    pinMode(DEMO_PIN, INPUT_PULLUP);
    delay(10);
    demo = digitalRead(DEMO_PIN) == LOW;
    ecu.begin(MS1_BAUD);
    Serial.println(demo ? F("megaboy nano: demo") : F("megaboy nano: ms1/extra"));
}

void loop() {
    uint32_t now = millis();
    ecu_rt_t rt;

    if (!demo && !ecu_ok && now - last_try >= RETRY_MS) {
        last_try = now;
        ecu_ok = ecu_connect();
        errors = 0;
    }

    if ((ecu_ok || demo) && now - last_poll >= POLL_MS) {
        last_poll = now;
        if (demo) {
            demo_rt(&rt, now);
            send_rt(&rt, LINK_F_DEMO);
        } else {
            static const uint8_t c_rt[] = {MS1_CMD_RT};
            uint8_t d[MS1_RT_LEN];
            if (ecu_cmd(c_rt, sizeof c_rt, d, MS1_RT_LEN) && ms1_decode_rt(d, MS1_RT_LEN, &cfg, &rt)) {
                errors = 0;
                send_rt(&rt, 0);
                if (now - last_print >= PRINT_MS) {
                    last_print = now;
                    print_rt(&rt);
                }
            } else if (++errors >= MAX_ERRORS) {
                ecu_ok = false;
            }
        }
    }

    if (now - last_status >= STATUS_MS) {
        last_status = now;
        send_status();
    }
}
