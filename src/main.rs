use linux_gateway::encode_calc_request_with_trace;
use opentelemetry::trace::TraceContextExt;
use opentelemetry::trace::{SpanBuilder, Tracer};
use opentelemetry::{global, KeyValue};
use std::time::{Duration, SystemTime};
use tracing::Span;
use tracing_opentelemetry::OpenTelemetrySpanExt;
fn current_trace_ids() -> Option<([u8; 16], [u8; 8])> {
    let cx = tracing::Span::current().context();
    let sc = cx.span().span_context().clone();
    if sc.is_valid() {
        Some((sc.trace_id().to_bytes(), sc.span_id().to_bytes()))
    } else {
        None
    }
}
#[derive(Deserialize)]
struct EncodeReq {
    a: u32,
    b: u32,
}

#[derive(Serialize)]
struct EncodeResp {
    a: u32,
    b: u32,
    frame_hex: String,
}

#[derive(Deserialize)]
struct DecodeReq {
    frame_hex: String,
}

#[derive(Serialize)]
struct DecodeResp {
    sum: Option<u32>,
    error: Option<String>,
    trace_id_hex: Option<String>,
    ts_ns: Option<u64>,
}
use serde::{Deserialize, Serialize};

#[derive(serde::Deserialize)]
struct R5TimingReq {
    start_ns: u64,
    end_ns: u64,
}

fn record_r5_span(start_ns: u64, end_ns: u64) {
    let parent_cx = tracing::Span::current().context();
    let tracer = global::tracer("linux_gateway");
    let start = SystemTime::UNIX_EPOCH + Duration::from_nanos(start_ns);
    let end = SystemTime::UNIX_EPOCH + Duration::from_nanos(end_ns);
    let dur = end_ns.saturating_sub(start_ns) as i64;

    let mut b = SpanBuilder::from_name("r5_compute");
    b.start_time = Some(start);
    b.end_time = Some(end);
    b.attributes = Some(vec![KeyValue::new("r5.duration.ns", dur)]);
    let _span = tracer.build_with_context(b, &parent_cx);
    // dropped here → exported with the provided times
}

async fn r5_timing_http(axum::Json(req): axum::Json<R5TimingReq>) -> axum::Json<serde_json::Value> {
    record_r5_span(req.start_ns, req.end_ns);
    axum::Json(serde_json::json!({"ok": true}))
}
// ---- exporter (HTTP → Jaeger OTLP at 4318) ----
async fn run_server() -> anyhow::Result<()> {
    use axum::{
        routing::{get, post},
        Router,
    };
    use tower_http::trace::{DefaultMakeSpan, DefaultOnResponse, TraceLayer};
    use tracing::Level;

    // these handlers should already exist in your file
    //   async fn encode_calc_http(Json(req): Json<EncodeReq>) -> Json<EncodeResp> { ... }
    //   async fn decode_calc_http(Json(req): Json<DecodeReq>) -> Json<DecodeResp> { ... }

    let app = Router::new()
        .route("/r5_timing", post(r5_timing_http))
        .route("/healthz", get(|| async { "ok" }))
        .layer(tower_http::trace::TraceLayer::new_for_http())
        .layer(tower_http::trace::TraceLayer::new_for_http())
        .route("/encode_calc", post(encode_calc_http))
        .route("/decode_calc", post(decode_calc_http))
        .layer(
            TraceLayer::new_for_http()
                .make_span_with(DefaultMakeSpan::new().level(Level::INFO))
                .on_response(DefaultOnResponse::new().level(Level::INFO)),
        );

    let listener = tokio::net::TcpListener::bind("0.0.0.0:8080").await?;
    tracing::info!("listening on {}", listener.local_addr()?);
    axum::serve(listener, app).await?;
    Ok(())
}

async fn encode_calc_http(axum::Json(req): axum::Json<EncodeReq>) -> axum::Json<EncodeResp> {
    // read the active HTTP span's IDs
    if let Some((trace_id, span_id)) = current_trace_ids() {
        tracing::info!(
            trace_id_hex = %hex::encode_upper(trace_id),
            span_id_hex = %hex::encode_upper(span_id),
            "propagation: got current HTTP trace/span IDs"
        );
    } else {
        tracing::warn!("propagation: no active span context");
    }

    // existing behavior unchanged for now
    // Grab current OTel context from the tracing span
    let cx = tracing::Span::current().context();
    let sc = cx.span().span_context().clone();
    if sc.is_valid() {
        tracing::info!(target: "propagation",
            http_trace_id=%sc.trace_id(),
            http_span_id=%sc.span_id(),
            "propagating current HTTP span IDs"
        );
    } else {
        tracing::warn!(target: "propagation", "no valid HTTP span context");
    }

    // Build RPMsg frame with a trace header
    let (frame, _trace_id_bytes, _ts_ns) = encode_calc_request_with_trace(req.a, req.b);
    axum::Json(EncodeResp {
        a: req.a,
        b: req.b,
        frame_hex: hex::encode_upper(frame),
    })
}

#[tracing::instrument(name="decode_calc", skip(req), fields(len = req.frame_hex.len()))]
async fn decode_calc_http(axum::Json(req): axum::Json<DecodeReq>) -> axum::Json<DecodeResp> {
    match hex::decode(&req.frame_hex)
        .ok()
        .and_then(|d| linux_gateway::decode_calc_response(&d).ok())
    {
        Some(resp) => {
            let (trace_id_hex, ts_ns) = match resp.trace {
                Some(t) => (Some(hex::encode_upper(t.id)), Some(t.ts_ns)),
                None => (None, None),
            };
            axum::Json(DecodeResp {
                sum: Some(resp.sum),
                error: None,
                trace_id_hex,
                ts_ns,
            })
        }
        None => axum::Json(DecodeResp {
            sum: None,
            error: Some("decode error".into()),
            trace_id_hex: None,
            ts_ns: None,
        }),
    }
}
fn main() {
    let _ = init_tracing();

    let args: Vec<String> = std::env::args().collect();
    if args.get(1).map(|s| s == "serve").unwrap_or(false) {
        let rt = tokio::runtime::Runtime::new().expect("rt");
        if let Err(e) = rt.block_on(run_server()) {
            eprintln!("server error: {e:?}");
        }
    } else {
        eprintln!("usage: {} serve", args[0]);
    }
}
fn init_tracing() -> anyhow::Result<()> {
    use opentelemetry::{global, trace::TracerProvider as _}; // for .tracer(...)
    use opentelemetry_otlp::{SpanExporter, WithExportConfig}; // for .with_endpoint(...)
    use opentelemetry_sdk::{propagation::TraceContextPropagator, trace::SdkTracerProvider};
    use tracing_subscriber::{layer::SubscriberExt, util::SubscriberInitExt};

    // Default to Jaeger OTLP/HTTP on localhost:4318
    let endpoint = std::env::var("OTEL_EXPORTER_OTLP_TRACES_ENDPOINT")
        .unwrap_or_else(|_| "http://127.0.0.1:4318/v1/traces".to_string());

    // Build OTLP/HTTP exporter
    let exporter = SpanExporter::builder()
        .with_http()
        .with_endpoint(endpoint) // needs WithExportConfig in scope
        .build()?;

    // Provider with batch exporter
    let provider = SdkTracerProvider::builder()
        .with_batch_exporter(exporter)
        .build();

    // Make global + W3C propagation
    let _ = global::set_tracer_provider(provider.clone());
    global::set_text_map_propagator(TraceContextPropagator::new());

    // Bridge tracing -> OTel
    let tracer = provider.tracer("linux_gateway"); // needs TracerProvider in scope
    let otel_layer = tracing_opentelemetry::layer().with_tracer(tracer);

    let _ = tracing_subscriber::registry()
        .with(tracing_subscriber::fmt::layer())
        .with(otel_layer)
        .try_init(); // idempotent

    Ok(())
}
