mod codec;
pub mod otel_trace;
pub mod proto;

pub use codec::{
    decode_calc_request, decode_calc_response, encode_calc_request, encode_calc_response,
};
