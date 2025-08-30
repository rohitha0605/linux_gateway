use linux_gateway::{guard_header, FrameError};

fn mk_hdr(ver: u8, typ: u8, len: u16) -> [u8; 10] {
    let sync = 0xA55Au16.to_le_bytes();
    let lenb = len.to_le_bytes();
    let crc = 0u32.to_le_bytes(); // CRC value irrelevant to guard test
    [
        sync[0], sync[1], ver, typ, lenb[0], lenb[1], crc[0], crc[1], crc[2], crc[3],
    ]
}

#[test]
fn rejects_unknown_version() {
    let hdr = mk_hdr(0x7F, 1, 0);
    let err = guard_header(&hdr).unwrap_err();
    match err {
        FrameError::UnknownVersion(0x7F) => {}
        _ => panic!("{err:?}"),
    }
}

#[test]
fn rejects_unknown_type() {
    let hdr = mk_hdr(0x01, 0xFF, 0);
    let err = guard_header(&hdr).unwrap_err();
    match err {
        FrameError::UnknownType(0xFF) => {}
        _ => panic!("{err:?}"),
    }
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
