use linux_gateway::{decode_calc_response, encode_calc_request, encode_calc_response};
use std::{
    env,
    fs::OpenOptions,
    io::{Read, Write},
};

fn main() {
    let args: Vec<String> = env::args().collect();

    // HTTP server
    if args.get(1).map(|s| s.as_str()) == Some("serve") {
        let rt = tokio::runtime::Runtime::new().expect("rt");
        rt.block_on(run_server()).unwrap();
        return;
    }

    // make_resp <sum>  -> hex frame for CalcResponse
    if args.len() == 3 && args[1] == "make_resp" {
        let sum: u32 = args[2].parse().expect("sum");
        let frame = encode_calc_response(sum);
        println!("{}", hex::encode_upper(frame));
        return;
    }

    // rpmsg-bounce <HEX> [DEV=/dev/rpmsg0]
    if (args.len() == 3 || args.len() == 4) && args[1] == "rpmsg-bounce" {
        let hex_in = &args[2];
        let dev = args.get(3).map(|s| s.as_str()).unwrap_or("/dev/rpmsg0");
        let to_write = hex::decode(hex_in).expect("hex");

        let mut f = OpenOptions::new()
            .read(true)
            .write(true)
            .open(dev)
            .expect("open rpmsg chardev");
        f.write_all(&to_write).expect("write");

        // Read header (10B) then payload
        let mut hdr = [0u8; 10];
        f.read_exact(&mut hdr).expect("read header");
        let len = u16::from_be_bytes([hdr[4], hdr[5]]) as usize;
        let mut rest = vec![0u8; len];
        f.read_exact(&mut rest).expect("read payload");

        let mut full = hdr.to_vec();
        full.extend_from_slice(&rest);
        match decode_calc_response(&full) {
            Ok(resp) => println!("BOUNCE_OK SUM={}", resp.sum),
            Err(e) => println!("BOUNCE_DECODE_ERROR={e}"),
        }
        return;
    }

    // <a> <b> -> request frame
    if args.len() == 3 {
        let a: u32 = args[1].parse().expect("a");
        let b: u32 = args[2].parse().expect("b");
        let frame = encode_calc_request(a, b);
        println!("FRAME_HEX={}", hex::encode_upper(frame));
        return;
    }

    // --decode=HEX -> response decode
    if args.len() == 2 && args[1].starts_with("--decode=") {
        let hex = &args[1]["--decode=".len()..];
        let data = hex::decode(hex).expect("hex");
        match decode_calc_response(&data) {
            Ok(resp) => println!("SUM={}", resp.sum),
            Err(e) => println!("DECODE_ERROR={e}"),
        }
        return;
    }

    eprintln!("Usage:");
    eprintln!("  {} serve                 # start HTTP API", args[0]);
    eprintln!(
        "  {} <a> <b>               # prints CalcRequest frame HEX",
        args[0]
    );
    eprintln!(
        "  {} --decode=HEX          # decodes a CalcResponse frame",
        args[0]
    );
    eprintln!(
        "  {} make_resp <sum>       # prints CalcResponse frame HEX",
        args[0]
    );
    eprintln!(
        "  {} rpmsg-bounce HEX [DEV]# write HEX to rpmsg, read & decode",
        args[0]
    );
}

//
// -------- HTTP server (axum) --------
//
use axum::{routing::post, Json, Router};
use serde::{Deserialize, Serialize};

#[derive(Deserialize)]
struct EncodeReq {
    a: u32,
    b: u32,
}
#[derive(Serialize)]
struct EncodeResp {
    a: u32,
    b: u32,
    frame_hex: String,
}

#[derive(Deserialize)]
struct DecodeReq {
    frame_hex: String,
}
#[derive(Serialize)]
struct DecodeResp {
    sum: Option<u32>,
    error: Option<String>,
    trace_id_hex: Option<String>,
    ts_ns: Option<u64>,
}

async fn run_server() -> anyhow::Result<()> {
    init_tracing()?;

    let app = Router::new()
        .route("/encode_calc", post(encode_calc_http))
        .route("/decode_calc", post(decode_calc_http));

    let listener = tokio::net::TcpListener::bind("127.0.0.1:8080").await?;
    axum::serve(listener, app).await?;
    Ok(())
}

async fn encode_calc_http(Json(req): Json<EncodeReq>) -> Json<EncodeResp> {
    let frame = encode_calc_request(req.a, req.b);
    Json(EncodeResp {
        a: req.a,
        b: req.b,
        frame_hex: hex::encode_upper(frame),
    })
}

async fn decode_calc_http(Json(req): Json<DecodeReq>) -> Json<DecodeResp> {
    match hex::decode(&req.frame_hex)
        .ok()
        .and_then(|d| linux_gateway::decode_calc_response(&d).ok())
    {
        Some(resp) => {
            let (trace_id_hex, ts_ns) = match resp.trace {
                Some(t) => (Some(hex::encode_upper(t.id)), Some(t.ts_ns)),
                None => (None, None),
            };
            Json(DecodeResp {
                sum: Some(resp.sum),
                error: None,
                trace_id_hex,
                ts_ns,
            })
        }
        None => Json(DecodeResp {
            sum: None,
            error: Some("decode error".into()),
            trace_id_hex: None,
            ts_ns: None,
        }),
    }
}

// ---- m2: tracing + jaeger ----
fn init_tracing() -> anyhow::Result<()> {
    tracing_subscriber::fmt::init();
    Ok(())
}
