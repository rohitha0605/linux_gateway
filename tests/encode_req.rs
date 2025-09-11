use linux_gateway::encode_calc_request;

#[test]
fn encodes_basic_small() {
    let _frame = encode_calc_request(7, 35, None);
}

#[test]
fn encodes_large_values() {
    let _frame = encode_calc_request(300, 70000, None);
}
