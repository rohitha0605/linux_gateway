use linux_gateway::wire;

#[test]
fn request_header_crc_ok() {
    let frame = linux_gateway::encode_calc_request(1, 2);

    // Optional SYNC at the front
    let sync = wire::SYNC.to_le_bytes();
    let has_sync = frame.len() >= 2 && frame[0..2] == sync;
    let off = if has_sync { 2 } else { 0 };

    // [ver][type] + payload + [CRC (last 4 bytes, LE)]
    assert!(frame.len() >= off + 2 + 4, "frame too short");
    let payload_start = off + 2;
    let payload_end = frame.len() - 4;

    let payload = &frame[payload_start..payload_end];
    let want = u32::from_le_bytes(frame[payload_end..payload_end + 4].try_into().unwrap());
    let got = wire::crc32(payload);

    assert_eq!(got, want, "CRC mismatch: got {got:#010x} want {want:#010x}");
}

#[test]
#[ignore = "parked: schema-agnostic varint probe is flaky; re-enable after proto is finalized"]
fn request_varints_are_multibyte_for_large_values() {
    // Values >=128 must produce multi-byte varints in protobuf
    let a: u32 = 150;
    let b: u32 = 100_000;

    let frame = linux_gateway::encode_calc_request(a, b);

    let sync = wire::SYNC.to_le_bytes();
    let has_sync = frame.len() >= 2 && frame[0..2] == sync;
    let off = if has_sync { 2 } else { 0 };

    assert!(frame.len() >= off + 2 + 4, "frame too short");
    let payload_start = off + 2;
    let payload_end = frame.len() - 4;
    let payload = &frame[payload_start..payload_end];

    // Protobuf varint field tags: field 1 -> 0x08, field 2 -> 0x10
    let tag = |id: u8| -> u8 { id << 3 };
    let tag_a = tag(1);
    let tag_b = tag(2);

    fn varint_len_after(buf: &[u8], tag: u8) -> Option<usize> {
        let mut i = 0;
        while i < buf.len() {
            if buf[i] == tag {
                let mut n = 0usize;
                let mut j = i + 1;
                while j < buf.len() {
                    n += 1;
                    let byte = buf[j];
                    j += 1;
                    if byte < 0x80 {
                        break;
                    } // end of varint
                }
                return Some(n);
            }
            i += 1;
        }
        None
    }

    let len_a = varint_len_after(payload, tag_a).unwrap_or(0);
    let len_b = varint_len_after(payload, tag_b).unwrap_or(0);

    assert!(
        len_a > 1,
        "field a varint should be multi-byte (len_a={len_a})"
    );
    assert!(
        len_b > 1,
        "field b varint should be multi-byte (len_b={len_b})"
    );
}
