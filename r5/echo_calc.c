#include "calc.pb-c.h"
#include <stdlib.h>
#include <stdint.h>

int process_calc_request(const uint8_t *in, size_t in_len, uint8_t **out, size_t *out_len) {
  Calc__CalcRequest *req = calc__calc_request__unpack(NULL, in_len, in);
  if (!req) return -1;

  long long sum = 0;
  if (req->op == CALC__CALC_REQUEST__OP__SUM) {
    for (size_t i = 0; i < req->n_nums; i++) sum += req->nums[i];
  }
  calc__calc_request__free_unpacked(req, NULL);

  Calc__CalcResponse resp = CALC__CALC_RESPONSE__INIT;
  resp.result = sum;

  *out_len = calc__calc_response__get_packed_size(&resp);
  *out = (uint8_t*)malloc(*out_len);
  if (!*out) return -2;
  calc__calc_response__pack(&resp, *out);
  return 0;
}
