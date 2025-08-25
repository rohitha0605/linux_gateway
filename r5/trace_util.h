#pragma once
#include <stdint.h>
#include <string.h>
#include "r5/gen/calc.pb.h"

// Copy a 16-byte ID + set timestamp into TraceHeader
static inline void trace_set(rpmsg_calc_v1_TraceHeader *th,
                             const pb_byte_t id16[16],
                             uint64_t ts_ns)
{
    memcpy(th->id, id16, 16);
    th->ts_ns = ts_ns;
}
