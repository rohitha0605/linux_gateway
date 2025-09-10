OpenTelemetry (MVP) — linux_gateway

This repository exports spans to Jaeger to demonstrate end-to-end tracing.

1) Start Jaeger (UI :16686, agent UDP :6831)
   docker ps --filter name=jaeger | grep jaeger || \
   docker run -d --name jaeger \
     -p 16686:16686 -p 6831:6831/udp \
     jaegertracing/all-in-one:1.58
   Open UI: http://localhost:16686

2) Emit a test trace from Rust
   # (macOS: exporter needs this cfg flag)
   export RUSTFLAGS="--cfg tokio_allow_from_blocking_fd"
   export RUST_LOG=linux_gateway=trace,opentelemetry=info
   export JAEGER_AGENT_HOST=127.0.0.1
   export JAEGER_AGENT_PORT=6831
   cargo run --example trace_demo

3) (Optional) Run the server with tracing
   cargo run -- serve

4) Grab latest trace ID (linux_gateway or example)
   id=$(curl -s 'http://localhost:16686/api/traces?service=linux_gateway&limit=1&lookback=2h' \
      | jq -r '.data[0].traceID // empty'); \
   [ -z "$id" ] && id=$(curl -s 'http://localhost:16686/api/traces?service=trace_demo&limit=1&lookback=2h' \
      | jq -r '.data[0].traceID // empty'); \
   echo "Trace: $id"
   echo "Open:  http://localhost:16686/trace/$id"

W3C traceparent (example)
   traceparent: 00-<trace_id_32hex>-<span_id_16hex>-01
