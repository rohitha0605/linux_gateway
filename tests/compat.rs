use linux_gateway::{decode_calc_response, encode_calc_request, FrameError};

const SYNC: u16 = 0xA55A;
const VER: u8 = 0x01;

fn varint_u32(mut v: u32) -> Vec<u8> {
    let mut out = Vec::new();
    while v >= 0x80 {
        out.push((v as u8) | 0x80);
        v >>= 7;
    }
    out.push(v as u8);
    out
}

fn mk_resp_frame(sum: u32) -> Vec<u8> {
    let mut payload = Vec::with_capacity(1 + 5);
    payload.push(0x08);
    payload.extend(varint_u32(sum));

    let crc = {
        let mut h = crc32fast::Hasher::new();
        h.update(&payload);
        h.finalize()
    };

    let mut frame = Vec::with_capacity(10 + payload.len());
    frame.extend_from_slice(&SYNC.to_be_bytes());
    frame.push(VER);
    frame.push(2u8); // CalcResponse
    frame.extend_from_slice(&(payload.len() as u16).to_be_bytes());
    frame.extend_from_slice(&crc.to_be_bytes());
    frame.extend_from_slice(&payload);
    frame
}

#[test]
fn compat_roundtrip() {
    let req = encode_calc_request(7, 35);
    assert!(req.len() >= 10);

    let frame = mk_resp_frame(42);
    let resp = decode_calc_response(&frame).expect("decode");
    assert_eq!(resp.sum, 42);
}

#[test]
fn compat_crc_mismatch_is_error() {
    let mut frame = mk_resp_frame(42);
    frame[6] ^= 0xFF; // flip one CRC byte
    assert!(matches!(decode_calc_response(&frame), Err(FrameError::Crc)));
}

#[test]
fn rejects_unknown_version() {
    let mut frame = mk_resp_frame(1);
    frame[2] ^= 0xFF; // version byte
    assert!(decode_calc_response(&frame).is_err());
}

#[test]
fn rejects_unknown_type() {
    let mut frame = mk_resp_frame(1);
    frame[3] = 0xFF; // type byte
    assert!(decode_calc_response(&frame).is_err());
}
#[test]
fn rejects_bad_sync() {
    let mut f = mk_resp_frame(1);
    f[0] = 0;
    f[1] = 0; // break SYNC
    assert!(linux_gateway::decode_calc_response(&f).is_err());
}

#[test]
fn rejects_truncated_payload() {
    let mut f = mk_resp_frame(1);
    let len = u16::from_be_bytes([f[4], f[5]]) as usize;
    f.truncate(10 + len - 1); // drop a byte from payload
    assert!(linux_gateway::decode_calc_response(&f).is_err());
}
