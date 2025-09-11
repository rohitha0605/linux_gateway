use linux_gateway::{
    encode_calc_request, decode_calc_request,
    encode_calc_response, decode_calc_response,
};
use linux_gateway::proto::CalcResponse;

#[test]
fn request_roundtrips() {
    let frame = encode_calc_request(7, 35, None);
    let req = decode_calc_request(&frame).expect("decode req");
    assert_eq!(req.a, 7);
    assert_eq!(req.b, 35);
    assert!(req.trace.is_none());
}

#[test]
fn response_roundtrips() {
    let resp = CalcResponse { result: 42, trace: None };
    let frame = encode_calc_response(&resp);
    let out = decode_calc_response(&frame).expect("decode resp");
    assert_eq!(out.result, 42);
    assert!(out.trace.is_none());
}
