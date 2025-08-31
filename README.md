# linux_gateway (RPMsg + Protobuf + Framed Codec)

![CI](https://github.com/rohitha0605/linux_gateway/actions/workflows/ci.yml/badge.svg?branch=main)

## Protocol
- `.proto`: `rpmsg.calc.v1` → `CalcRequest{a,b}`, `CalcResponse{sum, trace?}`
- Frame: `[SYNC=0xA55A|2][VER=0x01|1][TYPE|1][LEN|2][CRC32(payload)|4][PAYLOAD]`
  - TYPE: `1=CalcReq`, `2=CalcResp`

## Quick start

### Build & run the HTTP server
```bash
cargo build
RUST_LOG=info cargo run -- serve &# touch

## CI and coverage

CI status:
https://github.com/rohitha0605/linux_gateway/actions/workflows/ci.yml

Summary status:
https://github.com/rohitha0605/linux_gateway/actions/workflows/ci-summary.yml

## Frame layout

SYNC | VER | TYPE | LEN | CRC | PAYLOAD
1 byte | 1 | 1 | varint | u32 little endian | protobuf bytes

LEN is the length of the protobuf payload encoded as a varint.
CRC is CRC32 of VER, TYPE, LEN, and PAYLOAD.

## Quickstart

cargo build
cargo run -- --help

# encode request with large varints
cargo run -- make_req_trace 1073741823 715827882

# encode a response then decode it
HEX=$(cargo run --quiet -- make_resp 12345 | tr -d ' \n\r\t')
cargo run -- --decode="$HEX"
cargo run -- --decode="0x$HEX"

