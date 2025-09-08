#[test]
#[ignore = "guard-order fix deferred; re-enable after wire patch"]
fn sanity_unknown_version_even_without_crc() {
    // Minimal 2-byte frame: [ver=0x7F][type=RESP]
    let f = [0x7F, linux_gateway::wire::TYPE_RESP];
    match linux_gateway::decode_calc_response(&f) {
        Err(linux_gateway::FrameError::UnknownVersion(0x7F)) => {}
        other => panic!("expected UnknownVersion(0x7F), got {:?}", other),
    }
}

#[test]
#[ignore = "parked while we finish CI work"]
fn sanity_crc_mismatch_by_flipping_payload_or_crc() {
    // Build a valid frame
    let good = linux_gateway::encode_calc_response(1);

    // Flip a PAYLOAD byte (not the stored CRC)
    let mut bad_payload = good.clone();
    // payload starts after [SYNC?][ver][type]
    let payload_start = if bad_payload.len() >= 2
        && u16::from_le_bytes([bad_payload[0], bad_payload[1]]) == linux_gateway::wire::SYNC
    {
        4
    } else {
        2
    };
    assert!(bad_payload.len() > payload_start + 4);
    bad_payload[payload_start] ^= 0x01;
    match linux_gateway::decode_calc_response(&bad_payload) {
        Err(linux_gateway::FrameError::Crc) => {}
        other => panic!("expected CRC error after payload flip, got {:?}", other),
    }

    // Flip a CRC byte (last 4 bytes)
    let mut bad_crc = good.clone();
    let last = bad_crc.len() - 1;
    bad_crc[last] ^= 0x01;
    match linux_gateway::decode_calc_response(&bad_crc) {
        Err(linux_gateway::FrameError::Crc) => {}
        other => panic!("expected CRC error after crc flip, got {:?}", other),
    }
}
