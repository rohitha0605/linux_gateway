#pragma once
#include <string.h>
#include "calc.pb.h"

typedef rpmsg_calc_v1_TraceHeader TraceHdr;

/* Echo whatever trace the decoder produced */
static inline void trace_copy(TraceHdr *dst, const TraceHdr *src) { *dst = *src; }

/* Stub to keep old callers harmless if any remain */
static inline void trace_set(TraceHdr *t,
                             unsigned long long id,
                             unsigned long long span_id,
                             unsigned long long ts_ns) {
    (void)t; (void)id; (void)span_id; (void)ts_ns;
}
