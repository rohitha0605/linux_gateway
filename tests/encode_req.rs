use linux_gateway::encode_calc_request;

const SYNC: u16 = 0xA55A;
const VER: u8 = 0x01;

#[test]
fn request_header_crc_ok() {
    let f = encode_calc_request(1, 2);
    assert!(f.len() >= 10);
    let sync = u16::from_be_bytes([f[0], f[1]]);
    let ver = f[2];
    let typ = f[3];
    let len = u16::from_be_bytes([f[4], f[5]]) as usize;
    let crc = u32::from_be_bytes([f[6], f[7], f[8], f[9]]);
    assert_eq!(sync, SYNC);
    assert_eq!(ver, VER);
    assert_eq!(typ, 1u8);
    assert_eq!(len, f.len() - 10);

    let mut h = crc32fast::Hasher::new();
    h.update(&f[10..]);
    assert_eq!(h.finalize(), crc);
}

#[test]
fn request_varints_are_multibyte_for_large_values() {
    let f = encode_calc_request(300, 400); // forces multibyte varints
    assert_eq!(u16::from_be_bytes([f[0], f[1]]), SYNC);
    assert_eq!(f[2], VER);
    assert_eq!(f[3], 1u8);
    assert!(u16::from_be_bytes([f[4], f[5]]) as usize > 0);
}
