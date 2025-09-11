# linux_gateway

## Overview
- 1) RPMsg + Protobuf + Fuzzed Parser (CI)
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
- Prereqs: Rust stable; optional Docker for Jaeger; optional jq and curl for trace helpers
- Build: `cargo build`
- Test: `cargo test`
Round-trip demo (request → bounce → response) with trace id check:
```bash
# Make a traced request for A=7, B=35 (prints one HEX line)
REQ_HEX=$(cargo run --quiet -- make-req-trace 7 35 | tail -n1)
echo "REQ_HEX=${REQ_HEX}"
# Decode the request (prints A, B, TRACE_ID)
cargo run --quiet --example decode_req -- "${REQ_HEX}"
# Bounce the request via the RPMsg simulator to get a response frame (prints one HEX line)
RESP_HEX=$(cargo run --quiet -- rpmsg-bounce "${REQ_HEX}" | tail -n1)
echo "RESP_HEX=${RESP_HEX}"
# Decode the response (prints RESULT, TRACE_ID)
cargo run --quiet --example decode_resp -- "${RESP_HEX}"
# Verify the trace id is preserved end-to-end
REQ_TID=$(cargo run --quiet --example decode_req  -- "${REQ_HEX}"  | awk -F'"' '/TRACE_ID=Some/ {print $2}')
RESP_TID=$(cargo run --quiet --example decode_resp -- "${RESP_HEX}" | awk -F'"' '/TRACE_ID=Some/ {print $2}')
echo "REQ_TID=${REQ_TID}"
echo "RESP_TID=${RESP_TID}"
test -n "${REQ_TID}" -a -n "${RESP_TID}" -a "${REQ_TID}" = "${RESP_TID}" && echo trace_id_match || echo trace_id_mismatch

## CLI
- `linux_gateway --decode HEX
Validates that HEX parses as bytes. Use the example decoders below to inspect fields.
	-	linux_gateway make-resp <SUM>
Emits a CalcResponse frame with result=SUM. Prints one uppercase HEX line.
	- linux_gateway make-req-trace <A> <B>
Emits a CalcRequest frame carrying TraceCtx. Prints one uppercase HEX line.
	-	linux_gateway rpmsg-bounce <HEX>
Accepts a CalcRequest frame (HEX), returns a CalcResponse frame (HEX). Prints one uppercase HEX line.
	-	linux_gateway --version
Prints the crate version.

Decoders (examples):
	-	cargo run --quiet --example decode_req -- HEX
Prints A, B, TRACE_ID for a request frame.
	-	cargo run --quiet --example decode_resp -- HEX
Prints RESULT, TRACE_ID for a response frame.

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
