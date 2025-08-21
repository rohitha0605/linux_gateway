fn main() {
    prost_build::Config::new()
        .out_dir(std::env::var("OUT_DIR").unwrap())
        .compile_protos(&["proto/calc.proto"], &["proto"])
        .expect("prost-build failed");
    println!("cargo:rerun-if-changed=proto/calc.proto");
}
