#ifndef TRACE_UTIL_H
#define TRACE_UTIL_H
#include <stdint.h>

static inline void trace_set(void *trace, uint64_t id, uint64_t span_id, uint64_t ts_ns) {
    struct { uint64_t id, span_id, ts_ns; } *t = (struct { uint64_t id, span_id, ts_ns; } *)trace;
    if (!t) return;
    t->id = id;
    t->span_id = span_id;
    t->ts_ns = ts_ns;
}

static inline void trace_echo(void *dst, const void *src) {
    const struct { uint64_t id, span_id, ts_ns; } *s = (const struct { uint64_t id, span_id, ts_ns; } *)src;
    struct { uint64_t id, span_id, ts_ns; } *d = (struct { uint64_t id, span_id, ts_ns; } *)dst;
    if (!d || !s) return;
    d->id = s->id;
    d->span_id = s->span_id;
    d->ts_ns = s->ts_ns;
}
#endif
