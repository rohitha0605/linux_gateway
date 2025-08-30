use linux_gateway::{decode_calc_response, encode_calc_request};

const SYNC: u16 = 0xA55A;
const VER:  u8  = 0x01;

// sanity: request header + CRC bytes exist and look consistent
#[test]
fn encodes_header_and_crc() {
    let req = encode_calc_request(7, 35);
    assert!(req.len() >= 10);

    // [SYNC(2)][VER(1)][TYPE(1)][LEN(2)][CRC(4)][PAYLOAD…]
    let sync = u16::from_be_bytes([req[0], req[1]]);
    let ver  = req[2];
    let typ  = req[3];
    let len  = u16::from_be_bytes([req[4], req[5]]) as usize;
    let crc  = u32::from_be_bytes([req[6], req[7], req[8], req[9]]);
    let payload = &req[10..];

    assert_eq!(sync, SYNC);
    assert_eq!(ver, VER);
    assert_eq!(typ, 1u8); // CalcRequest
    assert_eq!(len, payload.len());

    let mut h = crc32fast::Hasher::new();
    h.update(payload);
    assert_eq!(crc, h.finalize());
}

// fabricate a response frame with zero-length payload -> decode should error
#[test]
fn empty_payload_is_error() {
    use crc32fast::Hasher;

    let payload: [u8;0] = [];
    let mut h = Hasher::new();
    h.update(&payload);
    let crc = h.finalize();

    let mut f = Vec::new();
    f.extend_from_slice(&SYNC.to_be_bytes()); // sync
    f.push(VER);                              // ver
    f.push(2u8);                              // type = CalcResponse
    f.extend_from_slice(&(0u16).to_be_bytes()); // len=0
    f.extend_from_slice(&crc.to_be_bytes());  // crc of empty
    // no payload

    assert!(decode_calc_response(&f).is_err());
}
