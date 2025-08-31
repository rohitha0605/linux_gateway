use assert_cmd::prelude::*;
use std::process::Command;

const BIN: &str = env!("CARGO_PKG_NAME");

fn run_arg(a: &str) {
    let _ = Command::cargo_bin(BIN).unwrap().arg(a).output();
}

#[test]
fn decode_error_matrix() {
    for a in [
        "--decode=GG",
        "--decode=00",
        "--decode=FFFF",
        "--decode=DEADBEEF",
        "--decode=0xDEADBEEF",
    ] {
        run_arg(a);
    }
}

#[test]
fn more_make_req_variants() {
    let _ = Command::cargo_bin(BIN)
        .unwrap()
        .args(["make_req", "1", "2"])
        .output();
    let _ = Command::cargo_bin(BIN)
        .unwrap()
        .args(["make_req", "127", "128"])
        .output();
    let _ = Command::cargo_bin(BIN)
        .unwrap()
        .args(["make_req", "4096", "8192"])
        .output();
}
