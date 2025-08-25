#include "r5/calc_service.h"

#define SYNC_HI 0xA5
#define SYNC_LO 0x5A
#define VER     0x01
#define TYPE_CALC_REQ 1

bool calc_handle_frame(const uint8_t *f, size_t flen,
                       uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (flen < 10) return false;
    if (f[0] != SYNC_HI || f[1] != SYNC_LO) return false;
    /* uint8_t ver = f[2]; */
    uint8_t typ = f[3];
    uint16_t len = (uint16_t)f[4] << 8 | (uint16_t)f[5];
    if (typ != TYPE_CALC_REQ) return false;
    if (10u + len > flen) return false;

    const uint8_t *payload = f + 10;
    return calc_handle_request(payload, len, out, out_cap, out_len);
}
