fn main() {
    let f = linux_gateway::encode_calc_request(5, 6);
    println!("wire::PROTO_VERSION={}", linux_gateway::wire::PROTO_VERSION);
    println!(
        "frame[0]=0x{:02X} len={} tail_crc_le={:02X?}",
        f[0],
        f.len(),
        &f[f.len() - 4..]
    );
    let n = f.len().min(16);
    println!("head={:02X?}", &f[..n]);
}
