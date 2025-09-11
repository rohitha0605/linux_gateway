pub mod gen {
    // This file name matches your package "rpmsg.calc.v1" from the .proto
    include!(concat!(env!("OUT_DIR"), "/rpmsg.calc.v1.rs"));
}

// Nice, stable paths for the rest of the crate:
pub use gen::rpmsg::calc::v1::{CalcRequest, CalcResponse, TraceCtx};
pub use gen::rpmsg::calc::v1::calc_request::Op;
