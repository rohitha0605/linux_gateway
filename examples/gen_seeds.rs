fn main() {
    std::fs::create_dir_all("fuzz/corpus/frame_decode").unwrap();
let good_req = linux_gateway::encode_calc_request(7, 35);
    let good_resp = linux_gateway::encode_calc_response(42);

    std::fs::write("fuzz/corpus/frame_decode/00-good-req", &good_req).unwrap();
    std::fs::write("fuzz/corpus/frame_decode/01-good-resp", &good_resp).unwrap();

    // bad crc (flip last byte)
    let mut bad_crc = good_resp.clone();
    *bad_crc.last_mut().unwrap() ^= 1;
    std::fs::write("fuzz/corpus/frame_decode/02-bad-crc", &bad_crc).unwrap();

    // unknown type
    let mut unk = good_req.clone();
    if unk.len() >= 2 {
        unk[1] = 0xFF;
    }
    std::fs::write("fuzz/corpus/frame_decode/03-unknown-type", &unk).unwrap();

    // short header
    std::fs::write("fuzz/corpus/frame_decode/04-too-short", [1u8]).unwrap();
}
