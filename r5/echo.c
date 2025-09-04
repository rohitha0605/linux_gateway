#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "calc.pb-c.h"

int main(void) {
    uint8_t buf[1024];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    if (n == 0) return 1;

    Calc__V1__CalcRequest *req = calc__v1__calc_request__unpack(NULL, n, buf);
    if (!req) return 2;

    uint32_t result = (req->op == CALC__V1__OP__OP_SUM) ? (req->a + req->b)
                                                        : (req->a * req->b);
    calc__v1__calc_request__free_unpacked(req, NULL);

    Calc__V1__CalcResponse resp = CALC__V1__CALC_RESPONSE__INIT;
    resp.result = result;

    size_t out_len = calc__v1__calc_response__get_packed_size(&resp);
    uint8_t *out = (uint8_t*)malloc(out_len);
    if (!out) return 3;
    calc__v1__calc_response__pack(&resp, out);
    fwrite(out, 1, out_len, stdout);
    free(out);
    return 0;
}
