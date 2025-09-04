use linux_gateway::{
    decode_calc_request, decode_calc_response, encode_calc_request, encode_calc_response,
    FrameError,
};

fn crc32(bytes: &[u8]) -> u32 {
    let mut crc: u32 = 0xFFFF_FFFF;
    for &b in bytes {
        crc ^= b as u32;
        for _ in 0..8 {
            let mask = (crc & 1).wrapping_mul(0xEDB88320);
            crc = (crc >> 1) ^ mask;
        }
    }
    !crc
}

#[test]
fn compat_roundtrip() {
    let frame = encode_calc_request(1234, 5678);
    let req = decode_calc_request(&frame).expect("decode");
    assert_eq!(req.a, 1234);
    assert_eq!(req.b, 5678);
}

#[test]
fn compat_crc_mismatch_is_error() {
    let mut frame = encode_calc_response(99);
    if frame.len() > 10 {
        frame[10] ^= 0xAA;
    }
    match decode_calc_response(&frame) {
        Err(FrameError::Crc) => {}
        other => panic!("expected CRC error, got {:?}", other),
    }
}

#[test]
fn decodes_max_varint_sum() {
    let frame = encode_calc_response(u32::MAX);
    let resp = decode_calc_response(&frame).expect("decode");
    assert_eq!(resp.result, u32::MAX);
}

#[test]
fn request_header_and_crc_are_consistent() {
    let frame = encode_calc_request(7, 35);
    assert!(frame.len() >= 10);
    let len = u16::from_be_bytes([frame[4], frame[5]]) as usize;
    let payload = &frame[10..];
    assert_eq!(len, payload.len(), "length field must match payload size");
    let got_crc = u32::from_be_bytes([frame[6], frame[7], frame[8], frame[9]]);
    let want_crc = crc32(payload);
    assert_eq!(got_crc, want_crc, "CRC32 must match payload");
}
