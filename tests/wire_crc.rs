use linux_gateway::wire::{build_frame, parse_and_verify, WireError};

#[test]
fn good_crc_passes() {
    let p = b"\x08\x2A"; // example protobuf-ish tiny payload
    let f = build_frame(1, 1, p);
    let (got, ver, typ) = parse_and_verify(&f).unwrap();
    assert_eq!(got, p);
    assert_eq!((ver, typ), (1, 1));
}

#[test]
fn bad_crc_fails() {
    let p = b"hello";
    let mut f = build_frame(1, 2, p);
    // flip a bit in CRC
    let last = f.len() - 1;
    f[last] ^= 0xFF;
    let e = parse_and_verify(&f).unwrap_err();
    assert_eq!(e, WireError::BadCrc);
}

#[test]
fn trunc_fails() {
    let f = build_frame(1, 3, b"abc");
    let e = parse_and_verify(&f[..f.len() - 2]).unwrap_err();
    assert!(matches!(e, WireError::Short | WireError::BadLen));
}
