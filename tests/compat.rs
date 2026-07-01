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

#[test]
fn skips_stray_sync_bytes() {
    let hdr = [0x5A, 0xA5, 0x5A, 0x01, 0x01];
    let (ver, typ) = guard_header(&hdr).expect("should decode after skipping SYNC");
    assert_eq!(ver, 1);
    assert_eq!(typ, 1);
}
