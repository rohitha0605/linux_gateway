use std::env;
use std::process;

const HELP: &str = "Usage:
  linux_gateway --decode HEX
  linux_gateway make-resp <SUM>
  linux_gateway make-req-trace <A> <B>
  linux_gateway rpmsg-bounce <HEX>
  linux_gateway --version
";

fn parse_hex(s: &str) -> Result<Vec<u8>, ()> {
    if s.len() % 2 != 0 {
        return Err(());
    }
    let mut out = Vec::with_capacity(s.len() / 2);
    let bytes = s.as_bytes();
    let mut i = 0;
    while i < s.len() {
        let hi = (bytes[i] as char).to_digit(16).ok_or(())?;
        let lo = (bytes[i + 1] as char).to_digit(16).ok_or(())?;
        out.push(((hi << 4) | lo) as u8);
        i += 2;
    }
    Ok(out)
}

fn main() {
    let argv: Vec<String> = env::args().collect();
    if argv.len() == 1 || argv[1] == "-h" || argv[1] == "--help" {
        println!("{}", HELP);
        return;
    }

    if argv[1] == "--version" || argv[1] == "-V" {
        println!("{}", env!("CARGO_PKG_VERSION"));
        return;
    }

    match argv[1].as_str() {
        s if s.starts_with("--decode") => {
            let val = if let Some(eq) = s.strip_prefix("--decode=") {
                eq.to_string()
            } else if s == "--decode" && argv.len() >= 3 {
                argv[2].clone()
            } else {
                eprintln!("{}", HELP);
                process::exit(2);
            };
            if parse_hex(&val).is_err() {
                eprintln!("decode: invalid hex");
                process::exit(2);
            }
        }
        "make-resp" | "make_resp" => {
            if argv.len() < 3 {
                eprintln!("{}", HELP);
                process::exit(2);
            }
            let sum: u32 = match argv[2].parse() {
                Ok(v) => v,
                Err(_) => {
                    eprintln!("make-resp: invalid number");
                    process::exit(2);
                }
            };
            let frame = linux_gateway::encode_calc_response(sum);
            let mut s = String::with_capacity(frame.len() * 2);
            for b in frame {
                s.push_str(&format!("{:02X}", b));
            }
            println!("{}", s);
        }
        "make-req-trace" | "make_req_trace" => {
            if argv.len() < 4 {
                eprintln!("{}", HELP);
                process::exit(2);
            }
            let a: u32 = argv[2].parse().unwrap_or_else(|_| {
                eprintln!("invalid A");
                process::exit(2);
            });
            let b: u32 = argv[3].parse().unwrap_or_else(|_| {
                eprintln!("invalid B");
                process::exit(2);
            });
            let frame = linux_gateway::encode_calc_request(a, b);
            let mut s = String::with_capacity(frame.len() * 2);
            for b in frame {
                s.push_str(&format!("{:02X}", b));
            }
            println!("{}", s);
        }
        "rpmsg-bounce" | "rpmsg_bounce" => {
            if argv.len() < 3 {
                eprintln!("rpmsg-bounce: missing HEX argument");
                process::exit(2);
            }
            if parse_hex(&argv[2]).is_err() {
                eprintln!("rpmsg-bounce: invalid hex");
                process::exit(2);
            }
        }
        _ => {
            println!("{}", HELP);
            process::exit(2);
        }
    }
}
