#include <stdbool.h>
#include <string.h>
#include "pb.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "calc.pb.h"
#include "trace_util.h"

// Decode CalcRequest from `in` and fill CalcResponse into `out`
bool calc_handle_request(const uint8_t *in, size_t in_len,
                         uint8_t *out, size_t out_cap, size_t *out_len)
{
    rpmsg_calc_v1_CalcRequest req = rpmsg_calc_v1_CalcRequest_init_zero;
    rpmsg_calc_v1_CalcResponse resp = rpmsg_calc_v1_CalcResponse_init_zero;

    pb_istream_t is = pb_istream_from_buffer(in, in_len);
    if(!pb_decode(&is, rpmsg_calc_v1_CalcRequest_fields, &req)) {
        return false;
    }

    resp.sum = req.a + req.b;

    if (req.has_trace) {
        resp.has_trace = true;
        trace_copy(&resp.trace, &req.trace); // echo back
    }

    pb_ostream_t os = pb_ostream_from_buffer(out, out_cap);
    if(!pb_encode(&os, rpmsg_calc_v1_CalcResponse_fields, &resp)) {
        return false;
    }
    *out_len = os.bytes_written;
    return true;
}
