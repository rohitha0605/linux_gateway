use crate::proto::TraceCtx;
use opentelemetry::trace::{SpanContext, SpanId, TraceContextExt, TraceFlags, TraceId, TraceState};
use opentelemetry::Context;

/// Current span -> TraceCtx {trace_id, span_id, flags(1 = sampled)}
pub fn current() -> Option<TraceCtx> {
    let cx = Context::current();
    let span_ref = cx.span();
    let sc = span_ref.span_context().clone();

    if !sc.is_valid() {
        return None;
    }

    let trace_id = sc.trace_id().to_bytes().to_vec();
    let span_id = sc.span_id().to_bytes().to_vec();
    let flags = if sc.trace_flags().is_sampled() { 1 } else { 0 };

    Some(TraceCtx {
        trace_id,
        span_id,
        flags: flags as u32,
    })
}

/// TraceCtx -> OpenTelemetry Context (remote parent)
pub fn from_proto(t: Option<&TraceCtx>) -> Option<Context> {
    let t = t?;
    let mut tid = [0u8; 16];
    let mut sid = [0u8; 8];
    if t.trace_id.len() == 16 {
        tid.copy_from_slice(&t.trace_id[..]);
    } else {
        return None;
    }
    if t.span_id.len() == 8 {
        sid.copy_from_slice(&t.span_id[..]);
    } else {
        return None;
    }

    let flags = if (t.flags & 1) != 0 {
        TraceFlags::SAMPLED
    } else {
        TraceFlags::default()
    };
    let sc = SpanContext::new(
        TraceId::from_bytes(tid),
        SpanId::from_bytes(sid),
        flags,
        true, // is_remote
        TraceState::default(),
    );

    Some(Context::current().with_remote_span_context(sc))
}
