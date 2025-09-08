fn main() {
    let frame = linux_gateway::encode_calc_request(2, 3);
    eprintln!(
        "len={} head={:02X?}",
        frame.len(),
        &frame[..frame.len().min(8)]
    );
    assert!(
        !frame.is_empty() && frame[0] == 1,
        "first byte must be 0x01"
    );
}
