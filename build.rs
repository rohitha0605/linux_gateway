fn main() {
    // Rebuild when proto changes
    println!("cargo:rerun-if-changed=proto/rpmsg/calc/v1/calc.proto");

    // Use a vendored protoc so CI and dev machines don't need it installed
    let protoc = protoc_bin_vendored::protoc_bin_path()
        .expect("vendored protoc");
    std::env::set_var("PROTOC", protoc);

    prost_build::Config::new()
        .compile_protos(
            &["proto/rpmsg/calc/v1/calc.proto"],
            &["proto"],
        )
        .expect("generate prost code");
}
