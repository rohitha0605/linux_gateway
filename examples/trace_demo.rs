use tracing::{info, instrument};
use tracing_subscriber::{layer::SubscriberExt, util::SubscriberInitExt, EnvFilter};

#[tokio::main]
#[allow(deprecated)]
async fn main() -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    use opentelemetry::global;
    use opentelemetry_sdk::propagation::TraceContextPropagator;

    global::set_text_map_propagator(TraceContextPropagator::new());

    let tracer = opentelemetry_jaeger::new_agent_pipeline()
        .with_service_name("linux_gateway")
        .install_batch(opentelemetry_sdk::runtime::Tokio)?;

    tracing_subscriber::registry()
        .with(EnvFilter::new("info"))
        .with(tracing_subscriber::fmt::layer())
        .with(tracing_opentelemetry::layer().with_tracer(tracer))
        .init();

    example_work().await;

    opentelemetry::global::shutdown_tracer_provider();
    Ok(())
}

#[instrument]
async fn example_work() {
    info!("doing work...");
    tokio::time::sleep(std::time::Duration::from_millis(120)).await;
    info!("done");
}
