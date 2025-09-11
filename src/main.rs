use tracing_opentelemetry::OpenTelemetrySpanExt;
mod telemetry;
use std::env;

const HELP: &str = "Usage:
  linux_gateway make-resp <SUM>
  linux_gateway make-req-trace <A> <B>
  linux_gateway rpmsg-bounce <HEX>
  linux_gateway --version
";

#[tokio::main]
async fn main() {
    let _ = telemetry::init("linux_gateway");

    let cli_span = tracing::info_span!("cli");
    let _cli_enter = cli_span.enter();

    let argv: Vec<String> = env::args().collect();

    // rpmsg-bounce <HEX>  -> decode CalcRequest, echo trace, encode CalcResponse
    if argv.len() == 3 && argv[1] == "rpmsg-bounce" {
        let hex_arg = &argv[2];
        let data = match hex::decode(hex_arg) {
            Ok(d) => d,
            Err(e) => {
                eprintln!("HEX_DECODE_ERR={e}");
                std::process::exit(2)
            }
        };
        match linux_gateway::decode_calc_request(&data) {
            Ok(req) => {
                let r5_span = tracing::info_span!("r5_compute", a = req.a, b = req.b);
                if let Some(cx) = linux_gateway::otel_trace::from_proto(req.trace.as_ref()) {
                    r5_span.set_parent(cx);
                }
                let _enter_r5 = r5_span.enter();
                // child span for the compute step
                let r5_span = tracing::info_span!("r5_compute", a = req.a, b = req.b);
                if let Some(cx) = linux_gateway::otel_trace::from_proto(req.trace.as_ref()) {
                    // Parent the span to the incoming context
                    r5_span.set_parent(cx);
                }
                let _r5_enter = r5_span.enter();

                let sum = (req.a as i64 + req.b as i64) as i32;
                let _resp = linux_gateway::proto::CalcResponse {
                    result: sum,
                    trace: req.trace.clone(), // echo incoming trace
                };
                let _resp = linux_gateway::proto::CalcResponse {
                    result: sum,
                    trace: req.trace.clone(),
                };
                let frame = linux_gateway::encode_calc_response(&_resp);
                println!("{}", hex::encode_upper(frame));
            }
            Err(e) => {
                eprintln!("DECODE_REQ_ERR={e}");
                std::process::exit(1)
            }
        }
        return;
    }

    if argv.len() == 1 || argv[1] == "-h" || argv[1] == "--help" {
        println!("{HELP}");
        return;
    }

    if argv[1] == "--version" || argv[1] == "-V" {
        println!("{}", env!("CARGO_PKG_VERSION"));
        return;
    }

    match argv[1].as_str() {
        "make-resp" | "make_resp" => {
            if argv.len() < 3 {
                eprintln!("{HELP}");
                std::process::exit(2);
            }
            let sum: u32 = argv[2].parse().unwrap_or_else(|_| {
                eprintln!("make-resp: invalid number");
                std::process::exit(2);
            });
            let _resp = linux_gateway::proto::CalcResponse {
                result: sum as i32,
                trace: None,
            };
            let frame = linux_gateway::encode_calc_response(&_resp);
            println!("{}", hex::encode_upper(frame));
        }
        "make-req-trace" | "make_req_trace" => {
            let make_span = tracing::info_span!("linux_request");
            let _enter = make_span.enter();
            if argv.len() < 4 {
                eprintln!("{HELP}");
                std::process::exit(2);
            }
            let a: i32 = argv[2].parse().unwrap_or_else(|_| {
                eprintln!("invalid A");
                std::process::exit(2);
            });
            let b: i32 = argv[3].parse().unwrap_or_else(|_| {
                eprintln!("invalid B");
                std::process::exit(2);
            });

            // Root span for the request we’re generating
            let make_span = tracing::info_span!("linux_request");
            let _enter = make_span.enter();

            let frame =
                linux_gateway::encode_calc_request(a, b, linux_gateway::otel_trace::current());
            println!("{}", hex::encode_upper(frame));
        }
        _ => {
            println!("{HELP}");
            std::process::exit(2);
        }
    }
}
