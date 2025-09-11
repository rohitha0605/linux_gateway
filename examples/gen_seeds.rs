use linux_gateway::proto::CalcResponse;

fn main() {
    // Request w/o trace
    let good_req = linux_gateway::encode_calc_request(7, 35, None);
    println!("{}", hex::encode_upper(good_req));

    // Response w/o trace
    let resp = CalcResponse { result: 42, trace: None };
    let good_resp = linux_gateway::encode_calc_response(&resp);
    println!("{}", hex::encode_upper(good_resp));
}
