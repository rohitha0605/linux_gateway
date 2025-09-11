#![allow(deprecated)]
use opentelemetry::global;
use opentelemetry_jaeger::new_agent_pipeline; // (yes, deprecated; fine for Jaeger agent MVP)
use opentelemetry_sdk::{propagation::TraceContextPropagator, runtime};
use tracing_subscriber::{layer::SubscriberExt, util::SubscriberInitExt, EnvFilter};

pub fn init(service: &str) -> Result<(), Box<dyn std::error::Error>> {
    // W3C propagation so Jaeger can stitch spans
    global::set_text_map_propagator(TraceContextPropagator::new());

    // Jaeger tracer (UDP agent exporter)
    let tracer = new_agent_pipeline()
        .with_service_name(service)
        .install_batch(runtime::Tokio)?; // <-- runtime from opentelemetry_sdk

    let fmt_layer = tracing_subscriber::fmt::layer().with_target(false);
    let env_filter =
        EnvFilter::try_from_default_env().unwrap_or_else(|_| "linux_gateway=info".parse().unwrap());
    let otel_layer = tracing_opentelemetry::layer().with_tracer(tracer);

    tracing_subscriber::registry()
        .with(env_filter)
        .with(fmt_layer)
        .with(otel_layer)
        .try_init()
        .ok();

    tracing::info!("tracing initialized");
    Ok(())
}
