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
RUST_LOG=info cargo run -- serve &