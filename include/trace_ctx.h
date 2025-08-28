#pragma once
#include <stdint.h>

/* 8 bytes appended by R5 in reply (big-endian) */
typedef struct __attribute__((packed)) {
    uint32_t t_start_us;
    uint32_t t_end_us;
} r5_timings_t;

static inline void be32_store(uint32_t v, uint8_t out[4]) {
    out[0] = (uint8_t)(v >> 24);
    out[1] = (uint8_t)(v >> 16);
    out[2] = (uint8_t)(v >> 8);
    out[3] = (uint8_t)(v >> 0);
}
uint32_t now_us(void);
