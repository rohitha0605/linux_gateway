use prost::Message;

#[test]
fn trace_roundtrips_linux_to_r5_to_linux() {
    // Linux: create request frame with trace
    let (req_frame, sent_trace) = linux_gateway::encode_calc_request_with_trace_ctx(7, 35);

    // --- R5 side emulation (decode -> compute -> encode, echoing TraceCtx) ---
    let req = linux_gateway::decode_calc_request(&req_frame).expect("decode req");
    let resp = linux_gateway::proto::CalcResponse {
        result: req.a + req.b,    // pretend R5 did the math
        trace: req.trace.clone(), // echo/propagate trace
    };
    let mut payload = Vec::new();
    resp.encode(&mut payload).unwrap();
    let resp_frame = linux_gateway::wire::wrap_v1_resp(&payload);
    // -------------------------------------------------------------------------

    // Linux: decode response and assert the trace matches
    let got_resp = linux_gateway::decode_calc_response(&resp_frame).expect("decode resp");
    let got = got_resp.trace.expect("response should carry trace");
    assert_eq!(
        got.trace_id, sent_trace.trace_id,
        "trace_id must round-trip"
    );
    assert_eq!(got.span_id, sent_trace.span_id, "span_id must round-trip");
}
