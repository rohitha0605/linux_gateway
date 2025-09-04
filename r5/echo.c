#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "calc.pb-c.h"

size_t handle(const uint8_t *buf, size_t n, uint8_t **out) {
    Rpmsg__Calc__V1__CalcRequest *req =
        rpmsg__calc__v1__calc_request__unpack(NULL, n, buf);
    if (!req) { *out = NULL; return 0; }

    uint32_t result = 0;
    switch (req->op) {
        case RPMSG__CALC__V1__OP__OP_SUM: result = req->a + req->b; break;
        case RPMSG__CALC__V1__OP__OP_SUB: result = req->a - req->b; break;
        case RPMSG__CALC__V1__OP__OP_MUL: result = req->a * req->b; break;
        case RPMSG__CALC__V1__OP__OP_DIV: result = (req->b ? req->a / req->b : 0); break;
        default: result = 0; break;
    }
    rpmsg__calc__v1__calc_request__free_unpacked(req, NULL);

    Rpmsg__Calc__V1__CalcResponse resp = RPMSG__CALC__V1__CALC_RESPONSE__INIT;
    resp.result = result;

    size_t out_len = rpmsg__calc__v1__calc_response__get_packed_size(&resp);
    *out = (uint8_t*)malloc(out_len);
    if (!*out) return 0;
    rpmsg__calc__v1__calc_response__pack(&resp, *out);
    return out_len;
}

#ifdef TEST_STANDALONE
int main(void) {
    Rpmsg__Calc__V1__CalcRequest req = RPMSG__CALC__V1__CALC_REQUEST__INIT;
    req.op = RPMSG__CALC__V1__OP__OP_SUM; req.a = 7; req.b = 35;

    size_t in_len = rpmsg__calc__v1__calc_request__get_packed_size(&req);
    uint8_t *in_buf = (uint8_t*)malloc(in_len);
    rpmsg__calc__v1__calc_request__pack(&req, in_buf);

    uint8_t *out_buf = NULL;
    size_t out_len = handle(in_buf, in_len, &out_buf);

    Rpmsg__Calc__V1__CalcResponse *resp =
        rpmsg__calc__v1__calc_response__unpack(NULL, out_len, out_buf);
    printf("%u\n", resp ? resp->result : 0u);

    free(in_buf); free(out_buf);
    if (resp) rpmsg__calc__v1__calc_response__free_unpacked(resp, NULL);
    return 0;
}
#endif
