# linux_gateway

## Overview
1) RPMsg + Protobuf + Fuzzed Parser (CI)
- Replace the ad-hoc text protocol with a versioned .proto (CalcRequest/CalcResponse).
- Framed wire format v1: [ver][type][varint len][protobuf][crc32].
- Defensive decoder with explicit errors and header guard before full parse.
- libFuzzer target for the frame/protobuf decoder; seed corpus and dictionaries included.
- CI builds and tests on stable, runs a nightly fuzz job, and publishes JUnit logs and coverage.
2) End-to-end tracing across Linux↔R5 (OpenTelemetry)
- Each message carries TraceCtx (trace_id, span_id, flags).
- Linux userspace exports spans to Jaeger; R5 echoes and propagates the context.
- Jaeger shows a parent span (linux_request) with a child span (r5_compute) for each call.
- A simple CLI round-trip proves the same trace_id on request and response.

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
- # Start Jaeger locally (UI on $JAEGER_PORT)
JAEGER_TAG=1.59
JAEGER_PORT=16686
OTLP_PORT=4317
docker run --rm -d --name jaeger \
  -p ${JAEGER_PORT}:16686 -p ${OTLP_PORT}:4317 \
  jaegertracing/all-in-one:${JAEGER_TAG}
# Point exporter to your local Jaeger OTLP endpoint
export OTEL_EXPORTER_OTLP_ENDPOINT=http://localhost:${OTLP_PORT}
# Open the UI (replace localhost if running remotely)
echo "Open Jaeger UI: http://localhost:${JAEGER_PORT}"
## Trace propagation
- Requests carry `TraceCtx { trace_id(16B), span_id(8B), flags }`
- R5 echoes `TraceCtx` back in responses
- Test: `tests/tracing_roundtrip.rs` verifies round-trip

## Protocol (rpmsg.calc.v1)
Protobuf messages
- `CalcRequest { a: int32, b: int32, op=Sum, trace?: TraceCtx }`
- `CalcResponse { result: int32, trace?: TraceCtx }`
- `TraceCtx { trace_id: bytes[16], span_id: bytes[8], flags: uint32 }`
  - `flags` bit 0 = sampled
Frame v1 layout
- `ver` : u8 = 1
- `type`: u8  (1 = request, 2 = response)
- `len` : varint length of `payload` in bytes
- `payload`: protobuf-encoded message
- `crc32_le`: CRC32 (IEEE) of `payload` bytes, written little-endian

## API highlights
Encode / decode
- `encode_calc_request(a: i32, b: i32, trace: Option<TraceCtx>) -> Vec<u8>`
- `encode_calc_response(resp: &CalcResponse) -> Vec<u8>`
- `decode_calc_request(frame: &[u8]) -> Result<CalcRequest, DecodeError>`
- `decode_calc_response(frame: &[u8]) -> Result<CalcResponse, DecodeError>`
Header peek (if enabled in the crate)
- `guard_header(frame: &[u8]) -> Result<(u8 /*ver*/, u8 /*typ*/), DecodeError>`
  - Parses just the header without validating CRC; useful for routing.

## Fuzzing
- Install: `rustup toolchain install nightly && cargo install cargo-fuzz`
- Seeds: `cargo +nightly run --example gen_seeds`
- Smoke run: `cargo +nightly fuzz run frame_decode -- -runs=20000 -dict=fuzz/dict/calc.dict give like this
  
## CI checks (required)
- build-test: stable toolchain; `cargo build --locked && cargo test --locked`.
- renode-smoke: boots a Renode scenario, runs an rpmsg round-trip, uploads JUnit logs.
- fuzz-smoke (nightly): `cargo +nightly fuzz run frame_decode -- -runs=20000 -rss_limit_mb=2048`; uploads corpus/crash artifacts.
- coverage: collects line/branch coverage (e.g., llvm-cov/grcov) and uploads an HTML report.
- sanitizers (nightly/Linux): Address/Undefined Sanitizer builds; fails on findings.

## Roadmap
- Re-enable parked tests (CRC mismatch; compat header guard).
- Add R5 timing log (`r5_compute_us=...`) and surface it in CI.
- Commit small fuzz seed corpus and dictionary; upload fuzz artifacts in CI.
- Add nanopb codegen and sample R5 firmware smoke test.
- Add backward/forward compatibility tests for protobuf versions.
- Publish crate and docs (crates.io + docs.rs).


