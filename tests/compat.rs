use linux_gateway::{guard_header, FrameError};

#[test]
fn rejects_unknown_version() {
    let hdr = [0x7F, 1];
    let err = guard_header(&hdr).unwrap_err();
    match err {
        FrameError::UnknownVersion(0x7F) => {}
        _ => panic!("{err:?}"),
    }
}

#[test]
fn rejects_unknown_type() {
    let hdr = [0x01, 0xFF];
    let err = guard_header(&hdr).unwrap_err();
    match err {
        FrameError::UnknownType(0xFF) => {}
        _ => panic!("{err:?}"),
    }
}
