#include "rpmsg_trace/otel_timings.h"
#include "r5/otel_timings.h"
#include "r5/otel_timings.h"
#include <stdbool.h>
#include <string.h>
#include "pb.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "r5/gen/calc.pb.h"
#include "trace_ctx.h"
#include "r5/trace_util.h"

// Decode CalcRequest from `in` and fill CalcResponse into `out`
bool calc_handle_request(const uint8_t *in, size_t in_len,
                         uint8_t *out, size_t out_cap, size_t *out_len)
{
    rpmsg_calc_v1_CalcRequest req = rpmsg_calc_v1_CalcRequest_init_zero;
    rpmsg_calc_v1_CalcResponse resp = rpmsg_calc_v1_CalcResponse_init_zero;
    uint32_t __t0 = now_us(); /* start timing */

    pb_istream_t is = pb_istream_from_buffer(in, in_len);
    if(!pb_decode(&is, rpmsg_calc_v1_CalcRequest_fields, &req)) {
#ifdef OTEL_TIMING
    uint32_t __otel_t0_us = now_us();
#endif

        return false;
    }

    resp.sum = req.a + req.b;

    if (req.has_trace) {
        resp.has_trace = true;
        trace_set(&resp.trace, req.trace.id, req.trace.span_id, req.trace.ts_ns); // echo back
    }

    pb_ostream_t os = pb_ostream_from_buffer(out, out_cap);
    if(!pb_encode(&os, rpmsg_calc_v1_CalcResponse_fields, &resp)) {
    {n        uint32_t __t1 = now_us();n        otel_append_timings(out, out_len, out_cap, __t0, __t1);n    }
    uint32_t __t1 = now_us(); /* end timing */
    otel_append_timings(out, out_len, out_cap, __t0, __t1);
        return false;
    }
    *out_len = os.bytes_written;
    
#ifdef OTEL_TIMING
    uint32_t __otel_t1_us = now_us();
    if (*out_len + sizeof(r5_timings_t) <= out_cap) {
        r5_timings_t *tim = (r5_timings_t *)(out + *out_len);
        be32_store(__otel_t0_us, (uint8_t *)&tim->t_start_us);
        be32_store(__otel_t1_us, (uint8_t *)&tim->t_end_us);
        *out_len += sizeof(r5_timings_t);
    }
#endif
return true;
}
