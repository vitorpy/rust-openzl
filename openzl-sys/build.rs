use std::{env, path::PathBuf};

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let vendor = manifest_dir.join("vendor").join("openzl");
    assert!(vendor.join("CMakeLists.txt").exists(), "vendor/openzl missing CMakeLists.txt (did you init submodules?)");

    // Configure and build OpenZL with CMake
    let mut cfg = cmake::Config::new(&vendor);
    cfg.profile("Release")
        .define("OPENZL_BUILD_SHARED_LIBS", "OFF")
        .define("OPENZL_BUILD_TESTS", "OFF")
        .define("OPENZL_BUILD_BENCHMARKS", "OFF")
        .define("OPENZL_BUILD_PYTHON_EXT", "OFF")
        .define("OPENZL_BUILD_PARQUET_TOOLS", "OFF")
        .define("OPENZL_BUILD_TOOLS", "OFF")
        .define("OPENZL_BUILD_CUSTOM_PARSERS", "OFF")
        .define("OPENZL_BUILD_CLI", "OFF")
        .define("OPENZL_BUILD_EXAMPLES", "OFF")
        .define("OPENZL_INSTALL", "OFF");

    let dst = cfg.build();

    // Include directories: public and generated config
    let include_pub = vendor.join("include");
    let include_gen = dst.join("include");

    // Build shim.c to safely expose helpers
    let mut cc_build = cc::Build::new();
    cc_build.file("src/shim.c");
    cc_build.include(&include_pub);
    cc_build.include(&include_gen);
    // Silence some warnings from headers
    cc_build.flag_if_supported("-Wno-unused-parameter");
    cc_build.flag_if_supported("-Wno-unused-function");
    cc_build.compile("openzl_sys_shim");

    // Generate bindings after CMake so generated headers exist
    let bindings = bindgen::Builder::default()
        .header(include_pub.join("openzl").join("openzl.h").to_string_lossy())
        .clang_arg(format!("-I{}", include_pub.display()))
        .clang_arg(format!("-I{}", include_gen.display()))
        // Only allow ZL_* symbols (types, functions, variables)
        .allowlist_type("^ZL_.*")
        .allowlist_function("^ZL_.*")
        .allowlist_var("^ZL_.*")
        // Stability: use Rust-style enums with type-safe variants
        .default_enum_style(bindgen::EnumVariation::Rust {
            non_exhaustive: false,
        })
        // Stability: derive common traits
        // Note: avoid Hash/Ord for structs with function pointers
        .derive_debug(true)
        .derive_default(true)
        .derive_eq(false)  // avoid PartialEq/Eq due to function pointers
        .derive_hash(false)  // avoid Hash due to function pointers
        .derive_ord(false)  // avoid PartialOrd/Ord due to function pointers
        // Layout tests for verification
        .layout_tests(true)
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("bindgen generate");

    let out = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out.join("bindings.rs"))
        .expect("write bindings");

    // Link search paths
    // CMake puts libraries in build/ subdirectory
    let build_dir = dst.join("build");
    println!("cargo:rustc-link-search=native={}", build_dir.display());

    // zstd is in build/zstd_build/lib
    let zstd_lib_dir = build_dir.join("zstd_build").join("lib");
    if zstd_lib_dir.exists() {
        println!("cargo:rustc-link-search=native={}", zstd_lib_dir.display());
    }

    // Also check lib64 (CMake may use this on some systems)
    let lib64_dir = dst.join("lib64");
    if lib64_dir.exists() {
        println!("cargo:rustc-link-search=native={}", lib64_dir.display());
    }

    // Link libraries (static)
    println!("cargo:rustc-link-lib=static=openzl");
    println!("cargo:rustc-link-lib=static=zstd");
    if cfg!(target_os = "linux") {
        println!("cargo:rustc-link-lib=pthread");
        println!("cargo:rustc-link-lib=m");
    }

    // Rerun hints
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed=src/shim.c");
    println!("cargo:rerun-if-changed={}", include_pub.join("openzl").join("openzl.h").display());
}
