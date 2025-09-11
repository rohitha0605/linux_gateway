# linux_gateway

## Overview
- Linux ↔ R5 gateway using RPMsg + Protobuf
- Framed wire format with version, type, CRC32
- Guarded parsing and decode errors
- Fuzzing (seed corpus + dict) and CI
- Minimal OpenTelemetry tracing (console + OTLP)

## Quick start
- Prereqs: Rust stable; (optional) Docker for Jaeger; nightly + cargo-fuzz for fuzzing
- Build: `cargo build`
- Test: `cargo test`

## CLI
- `linux_gateway --decode HEX`
- `linux_gateway make-resp <SUM>`
- `linux_gateway make-req-trace <A> <B>`
- `linux_gateway rpmsg-bounce <HEX>`
- `linux_gateway --version`

## Tracing
- Console logs/spans are always on
- Export to Jaeger:
  - `docker run --rm -it -p 16686:16686 -p 4317:4317 jaegertracing/all-in-one:1.57`
  - `export OTEL_EXPORTER_OTLP_ENDPOINT=http://localhost:4317`
  - `cargo run`
  - Open Jaeger: http://localhost:16686

## Trace propagation
- Requests carry `TraceCtx { trace_id(16B), span_id(8B), flags }`
- R5 echoes `TraceCtx` back in responses
- Test: `tests/tracing_roundtrip.rs` verifies round-trip

## Protocol (rpmsg.calc.v1)
- Protobuf messages:
  - `CalcRequest { a, b, op=Sum, trace? }`
  - `CalcResponse { result, trace? }`
  - `TraceCtx { trace_id(16), span_id(8), flags }`
- Wire (v1): `[ver=1][type][payload][crc32(payload, LE)]`
- Types: `1=request`, `2=response`
- CRC32: `crc32fast`

## API highlights
- Encode/decode:
  - `encode_calc_request(a, b) -> Vec<u8>`
  - `encode_calc_response(sum) -> Vec<u8>`
  - `decode_calc_request(frame) -> Result<CalcRequest, FrameError>`
  - `decode_calc_response(frame) -> Result<CalcResponse, FrameError>`
- Header guard (no CRC needed):
  - `guard_header(frame) -> Result<(ver, typ), FrameError>`
- With trace context:
  - `encode_calc_request_with_trace_ctx(a, b) -> (Vec<u8>, TraceCtx)`

## Fuzzing
- Install: `rustup toolchain install nightly && cargo install cargo-fuzz`
- Seeds: `cargo +nightly run --example gen_seeds`
- Smoke run: `cargo +nightly fuzz run frame_decode -- -runs=20000 -dict=fuzz/dict/calc.dict`

## CI checks (required)
- build-test
- renode-smoke
- fuzz-smoke
- coverage
- sanitizers

## Roadmap
- Re-enable parked tests (CRC mismatch; compat header guard)
- Optional R5 codegen script (nanopb) and firmware sample build
- Optional publish crate (crates.io/docs.rs)

## License
- TBD
