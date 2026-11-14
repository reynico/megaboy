#include "link.h"

enum { S_SYNC0, S_SYNC1, S_TYPE, S_LEN, S_DATA, S_SUM };

uint8_t link_encode(uint8_t *out, uint8_t type, const uint8_t *payload, uint8_t len) {
    uint8_t sum = (uint8_t)(type + len);
    uint8_t i;
    out[0] = LINK_SYNC0;
    out[1] = LINK_SYNC1;
    out[2] = type;
    out[3] = len;
    for (i = 0; i < len; i++) {
        out[4 + i] = payload[i];
        sum = (uint8_t)(sum + payload[i]);
    }
    out[4 + len] = (uint8_t)~sum;
    return (uint8_t)(len + 5u);
}

static uint8_t *put16(uint8_t *o, uint16_t v) {
    o[0] = (uint8_t)v;
    o[1] = (uint8_t)(v >> 8);
    return o + 2;
}

static uint16_t get16(const uint8_t *d) {
    return (uint16_t)(d[0] | (uint16_t)d[1] << 8);
}

uint8_t link_put_rt(uint8_t *out, const link_rt_t *rt) {
    uint8_t *o = out;
    o = put16(o, rt->rpm);
    o = put16(o, (uint16_t)rt->adv_x10);
    o = put16(o, (uint16_t)rt->map_x10);
    o = put16(o, (uint16_t)rt->iat_c_x10);
    o = put16(o, (uint16_t)rt->clt_c_x10);
    o = put16(o, (uint16_t)rt->tps_x10);
    o = put16(o, (uint16_t)rt->batt_x10);
    o = put16(o, (uint16_t)rt->afr_x10);
    o = put16(o, (uint16_t)rt->boost_psi_x10);
    o[0] = rt->engine;
    o[1] = rt->flags;
    return LINK_RT_LEN;
}

void link_get_rt(const uint8_t *in, link_rt_t *rt) {
    rt->rpm = get16(in);
    rt->adv_x10 = (int16_t)get16(in + 2);
    rt->map_x10 = (int16_t)get16(in + 4);
    rt->iat_c_x10 = (int16_t)get16(in + 6);
    rt->clt_c_x10 = (int16_t)get16(in + 8);
    rt->tps_x10 = (int16_t)get16(in + 10);
    rt->batt_x10 = (int16_t)get16(in + 12);
    rt->afr_x10 = (int16_t)get16(in + 14);
    rt->boost_psi_x10 = (int16_t)get16(in + 16);
    rt->engine = in[18];
    rt->flags = in[19];
}

uint8_t link_put_status(uint8_t *out, const link_status_t *st) {
    out[0] = st->state;
    out[1] = st->ecu;
    return LINK_STATUS_LEN;
}

void link_get_status(const uint8_t *in, link_status_t *st) {
    st->state = in[0];
    st->ecu = in[1];
}

uint8_t link_put_time(uint8_t *out, const rtc_time_t *t) {
    out[0] = t->yy;
    out[1] = t->mo;
    out[2] = t->dd;
    out[3] = t->hh;
    out[4] = t->mi;
    out[5] = t->ss;
    return LINK_TIME_SET_LEN;
}

void link_get_time(const uint8_t *in, rtc_time_t *t) {
    t->yy = in[0];
    t->mo = in[1];
    t->dd = in[2];
    t->hh = in[3];
    t->mi = in[4];
    t->ss = in[5];
}

uint8_t link_put_log(uint8_t *out, const link_log_t *l) {
    out[0] = l->state;
    put16(out + 1, l->num);
    put16(out + 3, l->secs);
    put16(out + 5, (uint16_t)l->bytes);
    put16(out + 7, (uint16_t)(l->bytes >> 16));
    out[9] = l->hz;
    out[10] = l->dated;
    link_put_time(out + 11, &l->start);
    return LINK_LOG_LEN;
}

void link_get_log(const uint8_t *in, link_log_t *l) {
    l->state = in[0];
    l->num = get16(in + 1);
    l->secs = get16(in + 3);
    l->bytes = (uint32_t)get16(in + 5) | (uint32_t)get16(in + 7) << 16;
    l->hz = in[9];
    l->dated = in[10];
    link_get_time(in + 11, &l->start);
}

void link_parser_reset(link_parser_t *p) {
    p->step = S_SYNC0;
}

uint8_t link_feed(link_parser_t *p, uint8_t b) {
    switch (p->step) {
    case S_SYNC0:
        if (b == LINK_SYNC0) {
            p->step = S_SYNC1;
        }
        return 0;
    case S_SYNC1:
        p->step = b == LINK_SYNC1 ? S_TYPE : (b == LINK_SYNC0 ? S_SYNC1 : S_SYNC0);
        return 0;
    case S_TYPE:
        p->type = b;
        p->sum = b;
        p->step = S_LEN;
        return 0;
    case S_LEN:
        if (b > LINK_MAX_PAYLOAD) {
            p->step = S_SYNC0;
            return 0;
        }
        p->len = b;
        p->n = 0;
        p->sum = (uint8_t)(p->sum + b);
        p->step = b ? S_DATA : S_SUM;
        return 0;
    case S_DATA:
        p->buf[p->n++] = b;
        p->sum = (uint8_t)(p->sum + b);
        if (p->n == p->len) {
            p->step = S_SUM;
        }
        return 0;
    default:
        p->step = S_SYNC0;
        return (uint8_t)~p->sum == b;
    }
}
