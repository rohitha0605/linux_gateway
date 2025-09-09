use afl::fuzz;

fn main() {
    fuzz!(|data: &[u8]| {
        let _ = linux_gateway::decode_calc_request(data);
        let _ = linux_gateway::decode_calc_response(data);
    });
}
