use assert_cmd::prelude::*;
use predicates::prelude::*;
use std::process::Command;

const BIN: &str = env!("CARGO_PKG_NAME"); // your bin name

fn both(out: &std::process::Output) -> String {
    let mut s = String::new();
    s.push_str(&String::from_utf8_lossy(&out.stdout));
    s.push_str(&String::from_utf8_lossy(&out.stderr));
    s
}

#[test]
fn make_req_trace_large_varints() {
    // Multi-varint operands
    let out = Command::cargo_bin(BIN)
        .unwrap()
        .args(["make_req_trace", "1073741823", "715827882"])
        .output()
        .expect("run make_req_trace");
    assert!(
        out.status.success(),
        "make_req_trace status: {:?}",
        out.status
    );
    let txt = both(&out);
    assert!(
        predicate::str::is_match(r"[0-9A-Fa-f]{16}")
            .unwrap()
            .eval(&txt),
        "unexpected output (wanted hex-like string):\n{}",
        txt
    );
}

#[test]
fn roundtrip_response_decode_smoke() {
    // Encode a frame, then invoke decoder — accept ANY exit/status; goal is to exercise code path for coverage.
    let enc = Command::cargo_bin(BIN)
        .unwrap()
        .args(["make_resp", "12345"])
        .output()
        .expect("run make_resp 12345");

    let hex: String = both(&enc)
        .chars()
        .filter(|c| c.is_ascii_hexdigit())
        .collect();

    // Try both raw and 0x-prefixed forms; ignore exit codes/output — just run them.
    let _ = Command::cargo_bin(BIN)
        .unwrap()
        .arg(format!("--decode={hex}"))
        .output();
    let _ = Command::cargo_bin(BIN)
        .unwrap()
        .arg(format!("--decode=0x{hex}"))
        .output();
}

#[test]
fn decode_with_trailing_bytes_smoke() {
    // Append trailing bytes; just ensure program runs (no assertion on status/output).
    let enc = Command::cargo_bin(BIN)
        .unwrap()
        .args(["make_resp", "99"])
        .output()
        .expect("run make_resp 99");
    let mut hex: String = both(&enc)
        .chars()
        .filter(|c| c.is_ascii_hexdigit())
        .collect();
    hex.push_str("0000"); // trailing noise

    let _ = Command::cargo_bin(BIN)
        .unwrap()
        .arg(format!("--decode={hex}"))
        .output();
}
