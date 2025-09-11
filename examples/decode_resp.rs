use std::env;
fn main() {
    let hex = env::args().nth(1).expect("HEX arg");
    let bytes = hex::decode(&hex).expect("hex");
    let resp = linux_gateway::decode_calc_response(&bytes).expect("decode");
    let tid = resp.trace.as_ref().map(|t| hex::encode_upper(&t.trace_id));
    println!("RESULT={}, TRACE_ID={:?}", resp.result, tid);
}
