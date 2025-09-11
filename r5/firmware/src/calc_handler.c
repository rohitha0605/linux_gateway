#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "pb.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "generated/calc.pb.h"

/* Decode CalcRequest from `in`, compute, echo trace, encode CalcResponse into `out`.
   Returns true on success; `*out_len` gets the encoded size. */
bool handle_calc_request(const uint8_t *in, size_t in_len, uint8_t *out, size_t *out_len) {
    CalcRequest req = CalcRequest_init_zero;
    pb_istream_t istream = pb_istream_from_buffer(in, in_len);
    if (!pb_decode(&istream, CalcRequest_fields, &req)) {
        return false;
    }

    CalcResponse resp = CalcResponse_init_zero;
    resp.result = req.a + req.b;
    if (req.has_trace) {
        resp.has_trace = true;
        resp.trace = req.trace;   // echo entire TraceCtx (trace_id, span_id, flags)
    }
    pb_ostream_t ostream = pb_ostream_from_buffer(out, *out_len);
    if (!pb_encode(&ostream, CalcResponse_fields, &resp)) {
        return false;
    }
    *out_len = ostream.bytes_written;
    return true;
}
