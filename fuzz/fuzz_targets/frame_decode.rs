#![no_main]
use libfuzzer_sys::fuzz_target;
use linux_gateway::decode_calc_response;

fuzz_target!(|data: &[u8]| {
    let _ = decode_calc_response(data); // We only care that it doesn't panic/UB
});
