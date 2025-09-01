#include "calc.pb-c.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int process_calc_request(const uint8_t*, size_t, uint8_t**, size_t*);

int main(void) {
  Calc__CalcRequest req = CALC__CALC_REQUEST__INIT;
  req.op = CALC__CALC_REQUEST__OP__SUM;
  int32_t nums[3] = {7, 35, -2};
  req.n_nums = 3;
  req.nums   = nums;

  size_t in_len = calc__calc_request__get_packed_size(&req);
  uint8_t *in_buf = (uint8_t*)malloc(in_len);
  calc__calc_request__pack(&req, in_buf);

  uint8_t *out_buf = NULL; size_t out_len = 0;
  int rc = process_calc_request(in_buf, in_len, &out_buf, &out_len);
  free(in_buf);
  if (rc != 0) { fprintf(stderr, "process failed: %d\n", rc); return 1; }

  Calc__CalcResponse *resp = calc__calc_response__unpack(NULL, out_len, out_buf);
  free(out_buf);
  if (!resp) { fprintf(stderr, "unpack resp failed\n"); return 2; }

  long long expected = 7 + 35 - 2;
  if (resp->result != expected) {
    fprintf(stderr, "bad sum: got %lld expected %lld\n", (long long)resp->result, expected);
    calc__calc_response__free_unpacked(resp, NULL);
    return 3;
  }
  calc__calc_response__free_unpacked(resp, NULL);
  puts("R5 host-smoke OK");
  return 0;
}
