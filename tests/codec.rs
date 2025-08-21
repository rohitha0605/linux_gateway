use linux_gateway::{encode_calc_request, decode_calc_response, FrameError};

const SYNC: u16 = 0xA55A;
const VER:  u8  = 0x01;

// Minimal protobuf varint encoder for u32.
fn varint_u32(mut v: u32) -> Vec<u8> {
    let mut out = Vec::new();
    while v >= 0x80 {
        out.push((v as u8) | 0x80);
        v >>= 7;
    }
    out.push(v as u8);
    out
}

// Build a CalcResponse frame for testing: SUM=<sum>
fn mk_resp_frame(sum: u32) -> Vec<u8> {
    // Protobuf payload: field 1 (varint) + value
    let mut payload = Vec::with_capacity(1 + 5);
    payload.push(0x08);               // field #1, varint wire type
    payload.extend(varint_u32(sum));  // value

    let crc = {
        let mut h = crc32fast::Hasher::new();
        h.update(&payload);
        h.finalize()
    };

    // [SYNC(2)][VER(1)][TYPE=2(1)][LEN(2)][CRC(4)][PAYLOAD]
    let mut frame = Vec::with_capacity(10 + payload.len());
    frame.extend_from_slice(&SYNC.to_be_bytes());
    frame.push(VER);
    frame.push(2u8); // CalcResp
    frame.extend_from_slice(&(payload.len() as u16).to_be_bytes());
    frame.extend_from_slice(&crc.to_be_bytes());
    frame.extend_from_slice(&payload);
    frame
}

#[test]
fn roundtrip_calc() {
    // sanity: request encodes
    let req = encode_calc_request(7, 35);
    assert!(req.len() >= 10);

    // decode a fabricated good response frame
    let frame = mk_resp_frame(42);
    let resp = decode_calc_response(&frame).expect("decode");
    assert_eq!(resp.sum, 42);
}

#[test]
fn crc_mismatch_is_error() {
    let mut frame = mk_resp_frame(42);
    // corrupt CRC
    frame[6] ^= 0xFF;
    assert!(matches!(decode_calc_response(&frame), Err(FrameError::Crc)));
}
