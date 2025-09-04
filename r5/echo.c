#include "calc.pb-c.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

static uint8_t *handle(const uint8_t *buf, size_t n, size_t *out_n) {
    Rpmsg__Calc__V1__CalcRequest *req =
        rpmsg__calc__v1__calc_request__unpack(NULL, n, buf);
    if (!req) return NULL;

    uint32_t result = req->a + req->b;
    rpmsg__calc__v1__calc_request__free_unpacked(req, NULL);

    Rpmsg__Calc__V1__CalcResponse resp = RPMSG__CALC__V1__CALC_RESPONSE__INIT;
    resp.result = result;

    size_t len = rpmsg__calc__v1__calc_response__get_packed_size(&resp);
    uint8_t *out = (uint8_t *)malloc(len);
    if (!out) return NULL;

    rpmsg__calc__v1__calc_response__pack(&resp, out);
    *out_n = len;
    return out;
}

#ifdef TEST_STANDALONE
int main(void) {
    Rpmsg__Calc__V1__CalcRequest req = RPMSG__CALC__V1__CALC_REQUEST__INIT;
    req.a = 7; req.b = 35;

    size_t in_len = rpmsg__calc__v1__calc_request__get_packed_size(&req);
    uint8_t *in_buf = (uint8_t *)malloc(in_len);
    rpmsg__calc__v1__calc_request__pack(&req, in_buf);

    size_t out_len = 0;
    uint8_t *out_buf = handle(in_buf, in_len, &out_len);
    free(in_buf);
    if (!out_buf) return 1;

    Rpmsg__Calc__V1__CalcResponse *resp =
        rpmsg__calc__v1__calc_response__unpack(NULL, out_len, out_buf);
    free(out_buf);
    if (!resp) return 2;

    printf("%u\n", resp->result);
    rpmsg__calc__v1__calc_response__free_unpacked(resp, NULL);
    return 0;
}
#endif
