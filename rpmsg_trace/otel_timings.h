#pragma once
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint32_t t_start_us;   /* big-endian on the wire */
    uint32_t t_end_us;
} r5_timings_t;

static inline void be32_store(uint32_t v, uint8_t out[4]) {
    out[0] = (uint8_t)(v >> 24);
    out[1] = (uint8_t)(v >> 16);
    out[2] = (uint8_t)(v >> 8);
    out[3] = (uint8_t)(v >> 0);
}

/* Firmware must provide now_us(). */
uint32_t now_us(void);

static inline void otel_append_timings(uint8_t *out, size_t *out_len, size_t out_cap,
                                       uint32_t t0_us, uint32_t t1_us) {
    if (*out_len + sizeof(r5_timings_t) <= out_cap) {
        r5_timings_t *tim = (r5_timings_t *)(out + *out_len);
        be32_store(t0_us, (uint8_t *)&tim->t_start_us);
        be32_store(t1_us, (uint8_t *)&tim->t_end_us);
        *out_len += sizeof(r5_timings_t);
    }
}
