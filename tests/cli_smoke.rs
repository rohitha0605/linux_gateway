use assert_cmd::prelude::*;
use predicates::prelude::*;
use std::process::Command;

#[test]
fn prints_help() {
    let mut cmd = Command::cargo_bin("linux_gateway").unwrap(); // <-- binary name
    cmd.arg("--help");
    cmd.assert()
        .success()
        .stdout(predicate::str::contains("--help").or(predicate::str::contains("Usage")));
}

#[test]
fn prints_version() {
    let mut cmd = Command::cargo_bin("linux_gateway").unwrap();
    cmd.arg("--version");
    cmd.assert().success();
}
