use linux_gateway::{decode_calc_request, decode_calc_response, FrameError};

#[test]
fn rejects_unknown_version_resp() {
    let sync = 0xA55Au16.to_le_bytes();
    let mut frame = Vec::new();
    frame.extend_from_slice(&sync);
    frame.push(0x7F); // ver (unknown)
    frame.push(1);    // type = CalcResp
    match decode_calc_response(&frame) {
        Err(FrameError::UnknownVersion(0x7F)) => {}
        other => panic!("expected UnknownVersion(0x7F), got {other:?}"),
    }
}

#[test]
fn rejects_unknown_type_req() {
    let sync = 0xA55Au16.to_le_bytes();
    let mut frame = Vec::new();
    frame.extend_from_slice(&sync);
    frame.push(1);    // ver = 1
    frame.push(0xFF); // type unknown
    match decode_calc_request(&frame) {
        Err(FrameError::UnknownType(0xFF)) => {}
        other => panic!("expected UnknownType(0xFF), got {other:?}"),
    }
}
