# linux_gateway (RPMsg + Protobuf + Framed Codec)

## Protocol
- `.proto`: rpmsg.calc.v1 → CalcRequest{a,b}, CalcResponse{sum}
- Frame: [SYNC=0xA55A|2][VER=0x01|1][TYPE|1][LEN|2][CRC32(payload)|4][PAYLOAD]
  - TYPE: 1=CalcReq, 2=CalcResp

## Quick start
cargo run -- 7 35
cargo run -- --decode=A55A0102000252BB5121082A
cargo run -- serve
curl -s 'http://127.0.0.1:3000/encode_calc?a=7&b=35'
curl -s -X POST 'http://127.0.0.1:3000/decode_calc' -d 'A55A0102000252BB5121082A'

![CI](https://github.com/rohitha0605/linux_gateway/actions/workflows/ci.yml/badge.svg?branch=main)
