use std::env;
fn main() {
    let hex = env::args().nth(1).expect("HEX arg");
    let bytes = hex::decode(&hex).expect("hex");
    let req = linux_gateway::decode_calc_request(&bytes).expect("decode");
    let tid = req.trace.as_ref().map(|t| hex::encode_upper(&t.trace_id));
    println!("A={}, B={}, TRACE_ID={:?}", req.a, req.b, tid);
}
