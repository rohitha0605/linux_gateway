// Re-exports of Prost-generated types
pub mod gen {
    // Your build output shows: target/.../out/rpmsg.calc.v1.rs
    include!(concat!(env!("OUT_DIR"), "/rpmsg.calc.v1.rs"));
}
// In this layout, CalcRequest/CalcResponse/TraceCtx are at the top level
pub use gen::{CalcRequest, CalcResponse, TraceCtx};
// If you ever reintroduce the Op enum inside CalcRequest, also:
// pub use gen::calc_request::Op;
