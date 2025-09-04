fn main() {
    println!("cargo:rerun-if-changed=proto/calc.proto");
    let mut cfg = prost_build::Config::new();
    cfg.include_file("proto.rs");
    cfg.compile_protos(&["proto/calc.proto"], &["proto"])
        .unwrap();
}
