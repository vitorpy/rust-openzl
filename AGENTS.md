# AGENTS.md — rust-openzl (vendored OpenZL)

Audience: agentic coding assistants and maintainers working in this repo.
Scope: the entire `rust-openzl` workspace and its sub-crates.

Goal: deliver robust, safe, and ergonomic Rust bindings for OpenZL using a vendored source tree at `vendor/openzl` (git submodule), with a clear roadmap from MVP to advanced features.

## Ground Rules
- Always build against the vendored OpenZL in `vendor/openzl`. Do not depend on system installations.
- Do not modify upstream OpenZL sources unless absolutely required. Prefer thin shims in `openzl-sys/src/` or upstream PRs.
- Keep `openzl-sys` unsafe and minimal; put ergonomic, safe APIs in `openzl`.
- Preserve reproducibility: pin the submodule commit; update intentionally.
- Avoid adding unrelated dependencies. Prefer std + well-known crates where necessary.

## Repository Layout
- `vendor/openzl/` — submodule with the OpenZL C/C++ sources and CMake build (contains its own submodules e.g., zstd).
- `openzl-sys/` — unsafe FFI crate: CMake build, C shim helpers, bindgen-generated Rust declarations.
- `openzl/` — safe wrapper crate: RAII, Result-based error handling, type-safe and ergonomic API.
- `Cargo.toml` (workspace), `.gitmodules`, `.gitignore`, `README.md`, `AGENTS.md`.

## Prerequisites (Local + CI)
- Build tools: `cmake >= 3.20`, C toolchain (clang or gcc), `make`/`ninja`.
- Bindgen: `clang` and `libclang` available on PATH (bindgen 0.69).
- Git submodules initialized recursively (zstd under vendor).

Quick bootstrap:
```bash
git submodule update --init --recursive
cargo build -p openzl
```

## OpenZL Architecture Understanding

**CRITICAL**: OpenZL is fundamentally a **graph-based typed compression** library, NOT a generic compressor like zlib/zstd.

- **Compression Graphs** are the CORE concept: they define HOW to compress specific data structures
- **Serial compression** (`compress_serial`/`decompress_serial`) is just a simple compatibility shim
- **TypedRef compression** REQUIRES compression graphs - you cannot compress typed data without specifying which algorithms to use
- Standard graphs available: `ZL_GRAPH_ZSTD`, `ZL_GRAPH_NUMERIC`, `ZL_GRAPH_FIELD_LZ`, `ZL_GRAPH_STORE`, etc.

See `vendor/openzl/examples/zs2_struct.c` for canonical usage pattern.

## Long, Detailed Sequence of Next Steps

The following steps are ordered for incremental value and easier review. Treat each numbered section as a checkpoint with a commit.

### 1) ✅ Stabilize the current build and baseline
- Verify `cargo build -p openzl` succeeds on Linux and macOS.
- Validate static linking: inspect the build dir (`target/debug/build/openzl-sys-*/out`) for `libopenzl.a` and link flags.
- Confirm that zstd is linked statically (presence of `libzstd.a`).
- Add minimal smoke tests in `openzl/tests/smoke.rs` to exercise `compress_serial`/`decompress_serial` round-trip.

### 2) ✅ Harden bindgen configuration
- Ensure bindgen allows only `ZL_*` symbols via allowlist.
- Use `--default-enum-style rust` for type-safe enums.
- Avoid Hash/Ord derives for structs with function pointers (causes warnings).
- Add layout tests for ABI verification.
- Document binding regeneration process in README.

### 3) ✅ Error handling and diagnostics
- C shim helpers for:
  - `openzl_error_code_to_string(ZL_ErrorCode)` using `ZL_ErrorCode_toString`.
  - Accessors for warnings arrays (`ZL_CCtx_getWarnings`, `ZL_DCtx_getWarnings`)
  - Error code/name extraction from `ZL_Error`
- In `openzl`, add `Warning` type with `warnings()` methods on contexts.
- Ensure all `ZL_Report` values are checked via shims; never inspect unions directly in Rust.

### 4) ⏭️ Compression graphs (CORE FUNCTIONALITY)

**This is the heart of OpenZL** - must come before TypedRef compression APIs.

Phase A: Basic graph infrastructure
- Wrap `ZL_Compressor`:
  - `Compressor::new()` -> `ZL_Compressor_create()`
  - `Drop` -> `ZL_Compressor_free()`
  - `set_parameter()` for global parameters
  - `warnings()` for graph validation errors
- Expose `ZL_GraphID` as opaque type with validity checking (`ZL_GraphID_isValid`).
- Expose standard graph constants as Rust constants:
  - `GRAPH_ZSTD` = `ZL_StandardGraphID_zstd`
  - `GRAPH_NUMERIC` = `ZL_StandardGraphID_select_numeric`
  - `GRAPH_STORE` = `ZL_StandardGraphID_store`
  - `GRAPH_FIELD_LZ` = `ZL_StandardGraphID_field_lz`
  - etc.

Phase B: Graph function API (stateless compression)
- Wrap `ZL_compress_usingGraphFn(dst, dstCap, src, srcSize, graphFn)`.
- Define `GraphFn` trait/callback for Rust:
  ```rust
  pub trait GraphFn {
      fn build_graph(&self, compressor: &mut Compressor) -> Result<GraphId, Error>;
  }
  ```
- Provide standard graph functions:
  - `ZstdGraph` - uses `ZL_GRAPH_ZSTD` for generic compression
  - `NumericGraph` - uses `ZL_GRAPH_NUMERIC` for numeric data
  - `StoreGraph` - no compression, useful for testing
- Implement `compress_with_graph(src, graph_fn)` convenience function.

Phase C: CCtx with Compressor integration
- Add `CCtx::set_compressor(&mut Compressor)` or `CCtx::from_compressor(Compressor)`.
- This enables `CCtx::compress_typed_ref()` to work (needs graph setup).
- Document that TypedRef compression requires a Compressor with registered graphs.

### 5) Safe TypedRef wrappers (requires Step 4)
- Wrap `ZL_TypedRef_*` creation and `ZL_TypedRef_free` with Rust smart constructors:
  - `TypedRef::serial(&[u8])`
  - `TypedRef::structs(bytes: &[u8], width: usize, count: usize)`
  - `TypedRef::numeric<T: Pod>(&[T])` (validate `size_of::<T>() in {1,2,4,8}`)
  - `TypedRef::strings(flat: &[u8], lens: &[u32])`
- Now that graphs are available, TypedRef compression works:
  - Use `compress_with_graph()` with TypedRef input
  - Or use `CCtx` with Compressor set
- Document lifetimes: `TypedRef` borrows input slices; horizon ends after the compression call returns.

### 6) Safe TypedBuffer wrappers (decompression output)
- Wrap `ZL_TypedBuffer` lifecycle:
  - `TypedBuffer::new()` -> `ZL_TypedBuffer_create`
  - `Drop` -> `ZL_TypedBuffer_free`
  - Accessors: `data_type()`, `byte_size()`, `num_elts()`, `elt_width()`, `as_bytes()`, `as_numeric<T>()` (alignment-safe), `string_lens()`.
- Implement decompression:
  - Auto-sized typed buffer path: `ZL_DCtx_decompressTBuffer` -> convert to Rust-owned slices.
  - Multi-output variant: `ZL_DCtx_decompressMultiTBuffer` with vector of `TypedBuffer`.
- Enforce alignment for numeric views; provide fallbacks (byte view + safe copying) when misaligned.

### 7) Frame inspection utilities
- Wrap `ZL_FrameInfo_*` to provide an ergonomic `FrameInfo`:
  - `format_version()`, `num_outputs()`, `output_type(i)`, `output_content_size(i)`, `output_num_elts(i)`.
- Validate both single-output helpers (`ZL_getDecompressedSize`, `ZL_getOutputType`) and multi-output via `FrameInfo` against the same frame for consistency.

### 8) Parameters and configuration
- Add typed enums mapping to `ZL_CParam` and `ZL_DParam` in `openzl`, with methods on `CCtx`, `DCtx`, and `Compressor`:
  - `set_parameter`, `get_parameter`, `reset_parameters`.
- Expose key parameters as Rust enums:
  - `CompressionLevel(i32)`
  - `FormatVersion(u32)`
  - `ChecksumFlag(bool)`
- Add optional advanced arenas: `ZL_CCtx_setDataArena`, `ZL_DCtx_setStreamArena` behind a feature or as advanced API.

### 9) High-level ergonomic APIs (MVP completion)
- High-level helpers built on graph API:
  - `compress_numeric<T: Pod>(&[T])` - uses `GRAPH_NUMERIC`
  - `decompress_numeric<T: Pod>(...)`
  - `compress_strings(flat, lens)` - custom graph for strings
  - `decompress_strings` returning `(Vec<u8>, Vec<u32>)`
- Provide builder-like patterns:
  - `Compression::new().with_level(5).with_graph(GRAPH_ZSTD).compress(data)`
  - `Decompression::new().decompress_typed(compressed)`

### 10) Tests and property-based checks
- Unit tests for each input/output type round-trip with random sizes.
- Graph-specific tests:
  - Test each standard graph (ZSTD, NUMERIC, FIELD_LZ)
  - Test graph validation errors
  - Test custom graph registration
- Proptests: numeric arrays with varying widths and alignment; string lens edge cases (0-length, large counts).
- Tests for multi-output frames using small synthesized corpora (if available).
- Validate error paths by intentionally misconfiguring (e.g., wrong widths) and asserting specific error codes and messages.

### 11) Examples and docs
- Add `examples/` covering:
  - Serial compress/decompress of a file (guarded on `std`).
  - Numeric round-trips with `u16`, `u32`, `u64` using `GRAPH_NUMERIC`.
  - Custom graph for struct compression (similar to `zs2_struct.c`).
  - Strings flattening and regeneration.
- Rustdoc examples for each public API.
- Document the graph-based architecture prominently.

### 12) Advanced graph registration (custom graphs)
- Phase 1: bind node registration APIs:
  - `ZL_Compressor_registerStaticGraph_fromNode1o`
  - `ZL_Compressor_registerStaticGraph_fromPipelineNodes1o`
  - `ZL_Compressor_registerSplitByStructGraph`
  - `ZL_Compressor_registerTokenizeGraph`
- Phase 2: expose standard node IDs:
  - `NODE_TRANSPOSE_SPLIT`, `NODE_DELTA_INT`, etc.
- Phase 3: Safe builders for complex graphs:
  - `GraphBuilder` pattern for composing nodes
  - Lifetime safety for graph references

### 13) Reflection and graph discovery (read-only)
- Wrap a minimal subset from `zl_reflection.h` to enumerate graphs/nodes/params for inspection tools.
- Expose as optional module `openzl::reflection` to keep MVP small.

### 14) Introspection hooks (feature-gated)
- Gate under `features = ["introspection"]`.
- Provide safe registration that holds closures or trait objects with proper lifetime pinning; consider only observer semantics (no mutation) to avoid UB.
- Document that OpenZL must be compiled with `OPENZL_ALLOW_INTROSPECTION=ON` (default is ON in this repo; keep flag configurable in build.rs when we add features).

### 15) Performance validation
- Add micro-benchmarks (criterion) for:
  - Different graphs on same data
  - Serial vs numeric paths
  - Graph construction overhead
- Measure impact of checksum parameters on decompression; record as doc comments.
- Confirm zero-copy for serial/numeric where possible; fallback to copy only on alignment/ownership constraints.

### 16) CI and toolchain hygiene
- GitHub Actions (or internal CI): linux + macOS; cache Cargo + CMake builds; run `git submodule update --init --recursive`.
- Pre-checks: rustfmt, clippy (deny on warnings for `openzl` only; keep `openzl-sys` permissive since it's generated).
- Optional: docs.rs build for `openzl` with vendored build skipped (document how—docs.rs often disallows running cmake; consider `--cfg docsrs` to stub sys symbols or use `links` tricks and `cfg(doc)` mocks).

### 17) Versioning and release discipline
- Track OpenZL version via headers (`ZL_LIBRARY_VERSION_*`) and reflect it in crate metadata (e.g., in README and docs).
- Release plan:
  - Update submodule commit.
  - Re-run bindgen and reformat.
  - Bump `openzl-sys` patch/minor as needed; bump `openzl` in lockstep when public API changes.
- Tag and changelog entries describing upstream delta.

### 18) Windows backlog (explicitly deferred)
- Leave TODO to support MSVC/clang-cl. Keep Unix-first.
- Note upstream warning about C11/MSVC; prefer clang-cl toolset when implemented.

### 19) Safety and concurrency model
- `CCtx`/`DCtx`/`Compressor` are not `Sync`. Consider `Send` if contexts are thread-confined and free of aliasing hazards.
- Typed views guarantee alignment only when produced by OpenZL; enforce checks when creating typed slices; otherwise expose bytes.
- Document lifetime coupling: borrowed inputs must outlive compression calls; owned outputs tie to `TypedBuffer` lifetime.
- Graph references must outlive compression operations.

### 20) Developer utilities and scripts
- Optional `xtask` crate:
  - `xtask regen-bindings`
  - `xtask bump-openzl <rev>` to update submodule and verify build.
  - `xtask ci` to run full check locally.

### 21) Pitfalls and workarounds
- Bindgen may choke on complex macros; blocklist those or add small C wrappers.
- Ensure `ZL_getDecompressedSize` is trusted only for single-output frames.
- String type currently lacks wrapString in Python; for Rust we rely on `ZL_TypedBuffer` APIs for strings; avoid unimplemented paths in C++ helpers.
- Endianness for Numeric follows host; document cross-arch caveats.
- **TypedRef compression REQUIRES compression graphs** - serial compression is the only graph-free path.

## Coding Conventions
- `openzl-sys`: no `unsafe` abstractions beyond FFI; no public re-exports of bindgen internals except via `pub use` of generated items; helpers live in C shim.
- `openzl`: public APIs return `Result<T, Error>`; avoid `unsafe` in public surfaces; prefer `#[non_exhaustive]` on enums; small types `Copy` where possible.
- Tests: name includes behavior and type (e.g., `rt_serial_small`, `rt_numeric_u32_alignment`, `graph_zstd_roundtrip`).

## Acceptance Criteria per Phase
- MVP complete when:
  - Compression graphs (Step 4) fully implemented with standard graphs (ZSTD, NUMERIC, etc.)
  - TypedRef and TypedBuffer wrappers implemented with Drop safety.
  - Graph-based typed compression working for numeric, serial, and string data.
  - FrameInfo inspection implemented.
  - CI green on linux+mac.
- Advanced features gated behind features and covered by tests.

## Commands Reference
- Initialize submodules: `git submodule update --init --recursive`
- Build sys + safe crates: `cargo build -p openzl`
- Run tests: `cargo test -p openzl`
- Run ignored tests (require graphs): `cargo test -p openzl -- --ignored`
- Re-run with verbose CMake: `VERBOSE=1 cargo build -p openzl-sys`

## Maintenance Notes
- Submodule bumps may change headers; regenerate bindings and scan diffs for enum/ABI changes.
- If upstream adds/removes libs, update link search paths in `openzl-sys/build.rs` accordingly.
- Keep `AGENTS.md` updated when the plan changes.
- Remember: OpenZL is graph-centric, not a drop-in replacement for zlib/zstd.
