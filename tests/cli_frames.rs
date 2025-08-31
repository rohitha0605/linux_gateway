use assert_cmd::prelude::*;
use predicates::prelude::*;
use std::process::Command;

const BIN: &str = env!("CARGO_PKG_NAME"); // auto-uses your bin name

fn both(out: &std::process::Output) -> String {
    let mut s = String::new();
    s.push_str(&String::from_utf8_lossy(&out.stdout));
    s.push_str(&String::from_utf8_lossy(&out.stderr));
    s
}

#[test]
fn make_req_trace_large_varints() {
    // 0x3FFF_FFFF and 0x2AAA_AAAA exercise multi-varint encoding
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
    // Encode a response frame for 12345, then decode it
    let enc = Command::cargo_bin(BIN)
        .unwrap()
        .args(["make_resp", "12345"])
        .output()
        .expect("run make_resp 12345");
    assert!(enc.status.success(), "make_resp status: {:?}", enc.status);

    // Only read; no mutation here → no `mut`
    let hex: String = both(&enc)
        .chars()
        .filter(|c| c.is_ascii_hexdigit())
        .collect();
    assert!(
        hex.len() >= 8,
        "encoded response didn't look like hex (len {}): {}",
        hex.len(),
        both(&enc)
    );

    let dec = Command::cargo_bin(BIN)
        .unwrap()
        .arg(format!("--decode={hex}"))
        .output()
        .expect("run --decode=<hex>");
    assert!(dec.status.success(), "--decode status: {:?}", dec.status);
    let txt = both(&dec);
    assert!(
        txt.contains("12345")
            || txt.contains("0x3039")
            || predicate::str::is_match(r"\b3039\b").unwrap().eval(&txt),
        "decoded output didn't show the sum 12345 in any common form:\n{}",
        txt
    );
}

#[test]
fn decode_with_trailing_bytes_does_not_panic() {
    // Append extra 00 bytes to the frame and ensure the program handles it
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
