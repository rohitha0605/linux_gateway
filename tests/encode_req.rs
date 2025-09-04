use linux_gateway::{encode_calc_request, wire};

fn find_after_tag(payload: &[u8], tag: u8) -> Option<usize> {
    // Scan for a single-byte varint tag; for our fields it’s always <= 0x1F
    payload.iter().position(|&b| b == tag).map(|i| i + 1)
}

#[test]
fn request_header_crc_ok() {
    // Small smoke to keep existing coverage expectation intact.
    let frame = encode_calc_request(7, 35);
    assert!(frame.len() >= 10, "frame too short");
    let len = u16::from_be_bytes([frame[4], frame[5]]) as usize;
    let payload = &frame[10..10 + len];
    let want_crc = wire::crc32(payload);
    let got_crc = u32::from_be_bytes([frame[6], frame[7], frame[8], frame[9]]);
    assert_eq!(want_crc, got_crc, "CRC mismatch");
}

#[test]
fn request_varints_are_multibyte_for_large_values() {
    // Use values > 127 so varints must span multiple bytes (MSB set on first byte)
    let frame = encode_calc_request(300, 70000);

    // Split payload out of our framing
    let len = u16::from_be_bytes([frame[4], frame[5]]) as usize;
    let payload = &frame[10..10 + len];

    // Tags for proto3 varint fields: a=2 -> 0x10, b=3 -> 0x18
    let a_idx = find_after_tag(payload, 0x10).expect("no tag for field a");
    let b_idx = find_after_tag(payload, 0x18).expect("no tag for field b");

    let a_first = payload[a_idx];
    let b_first = payload[b_idx];

    // Continuation bit must be set for multibyte varints
    assert!(
        a_first & 0x80 != 0,
        "field 'a' should be multibyte varint; first={:#04x}",
        a_first
    );
    assert!(
        b_first & 0x80 != 0,
        "field 'b' should be multibyte varint; first={:#04x}",
        b_first
    );
}
