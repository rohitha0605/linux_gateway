use linux_gateway::{
    encode_calc_request, decode_calc_request,
    encode_calc_response, decode_calc_response,
};
use linux_gateway::proto::CalcResponse;

#[test]
fn trace_is_echoed_when_present() {
    let req_frame = encode_calc_request(7, 35, None);
    let req = decode_calc_request(&req_frame).expect("decode req");
    let sum = req.a + req.b;

    // Echo/propagate whatever trace was present
    let resp = CalcResponse { result: sum, trace: req.trace.clone() };
    let rf = encode_calc_response(&resp);
    let out = decode_calc_response(&rf).expect("decode resp");

    assert_eq!(out.result, sum);
    assert_eq!(out.trace, req.trace);
}
