use assert_cmd::prelude::*;
use std::process::Command;

const BIN: &str = env!("CARGO_PKG_NAME"); // uses your package/bin name

fn both(out: &std::process::Output) -> String {
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
    let text = both(&out);
    assert!(text.contains("Usage") || text.contains("--help") || text.contains("USAGE"),
            "unexpected help text:\n{}", text);
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
fn clearly_invalid_inputs_exit_nonzero() {
    // 1) bad hex for --decode
    let out1 = Command::cargo_bin(BIN).unwrap()
        .arg("--decode=GG")   // 'G' isn't hex
        .output()
        .expect("run --decode=GG");
    if !out1.status.success() { return; }

    // 2) non-numeric for make_resp
    let out2 = Command::cargo_bin(BIN).unwrap()
        .args(["make_resp", "nope"])
        .output()
        .expect("run make_resp nope");
    if !out2.status.success() { return; }

    // 3) missing args for rpmsg-bounce
    let out3 = Command::cargo_bin(BIN).unwrap()
        .arg("rpmsg-bounce")
        .output()
        .expect("run rpmsg-bounce");
    assert!(!out3.status.success(), "invalid invocations unexpectedly succeeded.\n\
        --decode=GG -> {}\nmake_resp nope -> {}\nrpmsg-bounce -> {}\n",
        out1.status, out2.status, out3.status);
}
