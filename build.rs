fn main() {
    println!("cargo:rerun-if-changed=proto/rpmsg/calc/v1/calc.proto");
    prost_build::Config::new()
        .compile_protos(&["proto/rpmsg/calc/v1/calc.proto"], &["proto"])
        .unwrap();
}
