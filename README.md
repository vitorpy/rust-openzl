# rust-openzl (vendored)

Rust bindings for OpenZL with a vendored C library (git submodule at `vendor/openzl`).

- Always builds against the vendored source; no system or network dependencies.
- `openzl-sys`: unsafe FFI + CMake build of the C library.
- `openzl`: safe, ergonomic API on top of `openzl-sys`.

## Bootstrap

```bash
git submodule update --init --recursive
cargo build -p openzl
```

## Testing

```bash
cargo test -p openzl
```

## Regenerating Bindings

Bindings are automatically generated during the build process. To force regeneration after updating the OpenZL submodule:

```bash
cargo clean -p openzl-sys
cargo build -p openzl-sys
```

The bindings configuration in `openzl-sys/build.rs` uses:
- Rust-style enums for type safety
- Allowlist for `ZL_*` symbols only
- Layout tests for ABI verification

