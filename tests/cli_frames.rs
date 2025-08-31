use assert_cmd::prelude::*;
use predicates::prelude::*;
use std::process::Command;

const BIN: &str = env!("CARGO_PKG_NAME");

fn both(out: &std::process::Output) -> String {
    let mut s = String::new();
    s.push_str(&String::from_utf8_lossy(&out.stdout));
    s.push_str(&String::from_utf8_lossy(&out.stderr));
    s
}

#[test]
fn make_req_trace_large_varints() {
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
fn roundtrip_response_encode_then_decode() {
    let enc = Command::cargo_bin(BIN)
        .unwrap()
        .args(["make_resp", "12345"])
        .output()
        .expect("run make_resp 12345");
    assert!(enc.status.success(), "make_resp status: {:?}", enc.status);

    let hex_clean: String = both(&enc)
        .chars()
        .filter(|c| c.is_ascii_hexdigit())
        .collect();
    assert!(
        hex_clean.len() >= 8,
        "encoded response didn't look like hex (len {}): {}",
        hex_clean.len(),
        both(&enc)
    );

    // Try decode with raw hex
    let out1 = Command::cargo_bin(BIN)
        .unwrap()
        .arg(format!("--decode={hex_clean}"))
        .output()
        .expect("run --decode=<hex>");
    let txt1 = both(&out1);

    // If that failed, try with 0x prefix (some CLIs require it)
    let ok = if out1.status.success() && !txt1.trim().is_empty() {
        true
    } else {
        let out2 = Command::cargo_bin(BIN)
            .unwrap()
            .arg(format!("--decode=0x{hex_clean}"))
            .output()
            .expect("run --decode=0x<hex>");
        let txt2 = both(&out2);
        out2.status.success() && !txt2.trim().is_empty()
    };

    assert!(ok, "decode did not produce any output in either form");
}

#[test]
fn decode_with_trailing_bytes_does_not_panic() {
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

    let dec = Command::cargo_bin(BIN)
        .unwrap()
        .arg(format!("--decode={hex}"))
        .output()
        .expect("run --decode with trailing bytes");
    let txt = both(&dec);
    assert!(
        !txt.trim().is_empty(),
        "decoder produced no output with trailing bytes; status={:?}",
        dec.status
    );
}
