use assert_cmd::prelude::*;
use std::process::Command;

const BIN: &str = env!("CARGO_PKG_NAME"); // uses your package/bin name

fn both_streams(out: &std::process::Output) -> String {
    let mut s = String::new();
    s.push_str(&String::from_utf8_lossy(&out.stdout));
    s.push_str(&String::from_utf8_lossy(&out.stderr));
    s
}

#[test]
fn prints_help() {
    let out = Command::cargo_bin(BIN).unwrap()
        .arg("--help")
        .output()
        .expect("run --help");
    assert!(out.status.success(), "--help exited with {:?}", out.status);
    let text = both_streams(&out);
    assert!(
        text.contains("Usage") || text.contains("--help") || text.contains("USAGE"),
        "unexpected help text:\n{}",
        text
    );
}

#[test]
fn prints_version() {
    let out = Command::cargo_bin(BIN).unwrap()
        .arg("--version")
        .output()
        .expect("run --version");
    assert!(out.status.success(), "--version exited with {:?}", out.status);
}

#[test]
fn invalid_flag_errors() {
    let st = Command::cargo_bin(BIN).unwrap()
        .arg("--definitely-not-a-real-flag")
        .output()
        .expect("run invalid flag").status;
    assert!(!st.success(), "invalid flag unexpectedly succeeded");
}
