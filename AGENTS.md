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

Quick bootstrap
- `git submodule update --init --recursive`
- `cargo build -p openzl`


## Long, Detailed Sequence of Next Steps
The following steps are ordered for incremental value and easier review. Treat each numbered section as a checkpoint with a commit.

1) Stabilize the current build and baseline
- Verify `cargo build -p openzl` succeeds on Linux and macOS.
- Validate static linking: inspect the build dir (`target/debug/build/openzl-sys-*/out`) for `libopenzl.a` and link flags.
- Confirm that zstd is linked statically (presence of `libzstd.a`).
- Add a minimal smoke test in `openzl/tests/smoke.rs` to exercise `compress_serial`/`decompress_serial` round-trip for small buffers.

2) Harden bindgen configuration
- Ensure bindgen allows only `ZL_*` symbols. Add blocklists if any problematic macros/types appear.
- Freeze bindgen output stability: prefer `--default-enum-style rust` (only if it doesn’t break APIs); otherwise keep C-like enums and map in safe crate.
- Add a `regen-bindings` instruction in README and a short `xtask` (optional) to regenerate after submodule bumps.

3) Error handling and diagnostics
- Expand the C shim (`openzl-sys/src/shim.c`) with:
  - `openzl_error_code_name(ZL_ErrorCode)` returning a stable string (use `ZL_ErrorCode_toString`).
  - Accessors for CCtx/DCtx warnings arrays (`ZL_CCtx_getWarnings`, `ZL_DCtx_getWarnings`) plus helpers to render them as strings for logs.
- In `openzl`, add `OpenZLWarning` type and return warnings alongside results where appropriate or expose a `warnings()` query on contexts.
- Ensure all `ZL_Report` values are checked via shims; never inspect unions directly in Rust.

4) Safe types for inputs (TypedRef) and one-shot compression APIs
- Wrap `ZL_TypedRef_*` creation and `ZL_TypedRef_free` with Rust smart constructors:
  - `TypedRef::serial(&[u8])`
  - `TypedRef::structs(bytes: &[u8], width: usize, count: usize)`
  - `TypedRef::numeric<T: Pod>(&[T])` (validate `size_of::<T>() in {1,2,4,8}`)
  - `TypedRef::strings(flat: &[u8], lens: &[u32])`
- Add `compress_typed_ref(&TypedRef)` and `compress_multi_typed_ref(&[TypedRef])` using `ZL_CCtx_compressTypedRef`/`ZL_CCtx_compressMultiTypedRef`.
- Document lifetimes: `TypedRef` borrows input slices; horizon ends after the compression call returns.

5) Safe types for outputs (TypedBuffer) and typed decompression APIs
- Wrap `ZL_TypedBuffer` lifecycle:
  - `TypedBuffer::new()` -> `ZL_TypedBuffer_create`
  - `Drop` -> `ZL_TypedBuffer_free`
  - Accessors: `type()`, `byte_size()`, `num_elts()`, `elt_width()`, `as_bytes()`, `as_numeric<T>()` (alignment-safe), `string_lens()`.
- Implement decompression:
  - Single typed output into pre-allocated buffer: `ZL_DCtx_decompressTyped` (unsafe pointers managed internally).
  - Auto-sized typed buffer path: `ZL_DCtx_decompressTBuffer` -> convert to Rust-owned slices.
  - Multi-output variant: `ZL_DCtx_decompressMultiTBuffer` with vector of `TypedBuffer`.
- Enforce alignment for numeric views; provide fallbacks (byte view + safe copying) when misaligned.

6) Frame inspection utilities
- Wrap `ZL_FrameInfo_*` to provide an ergonomic `FrameInfo`:
  - `format_version()`, `num_outputs()`, `output_type(i)`, `output_content_size(i)`, `output_num_elts(i)`.
- Validate both single-output helpers (`ZL_getDecompressedSize`, `ZL_getOutputType`) and multi-output via `FrameInfo` against the same frame for consistency.

7) Parameters and configuration
- Add typed enums mapping to `ZL_CParam` and `ZL_DParam` in `openzl`, with methods on `CCtx` and `DCtx`:
  - `set_parameter`, `get_parameter`, `reset_parameters`.
- Add optional advanced arenas: `ZL_CCtx_setDataArena`, `ZL_DCtx_setStreamArena` behind a feature or as advanced API.

8) Public Rust API ergonomics (MVP completion)
- High-level helpers:
  - `compress_serial`/`decompress_serial` (already present).
  - `compress_numeric<T: Pod>(&[T])`, `decompress_numeric<T: Pod>(...)` with conversions when endianness/width differs (document current semantics: host endianness assumed by C API).
  - `compress_strings(flat, lens)`, `decompress_strings` returning `(Vec<u8>, Vec<u32>)`.
- Provide builder-like `Compressor` and `Decompressor` wrappers around CCtx/DCtx to encapsulate parameters and reuse contexts.

9) Tests and property-based checks
- Unit tests for each input/output type round-trip with random sizes.
- Proptests: numeric arrays with varying widths and alignment; string lens edge cases (0-length, large counts).
- Tests for multi-output frames using small synthesized corpora (if available).
- Validate error paths by intentionally misconfiguring (e.g., wrong widths) and asserting specific error codes and messages.

10) Examples and docs
- Add `examples/` covering:
  - Serial compress/decompress of a file (guarded on `std`).
  - Numeric round-trips with `u16`, `u32`, `u64`.
  - Strings flattening and regeneration.
- Rustdoc examples for each public API.

11) Reflection and graph discovery (read-only)
- Wrap a minimal subset from `zl_reflection.h` to enumerate graphs/nodes/params for inspection tools.
- Expose as optional module `openzl::reflection` to keep MVP small.

12) Introspection hooks (feature-gated)
- Gate under `features = ["introspection"]`.
- Provide safe registration that holds closures or trait objects with proper lifetime pinning; consider only observer semantics (no mutation) to avoid UB.
- Document that OpenZL must be compiled with `OPENZL_ALLOW_INTROSPECTION=ON` (default is ON in this repo; keep flag configurable in build.rs when we add features).

13) Advanced: function graphs and selectors (registration)
- Phase 1: bind and expose descriptors (`ZL_FunctionGraphDesc`, `ZL_SelectorDesc`) with safe builders.
- Phase 2: allow passing user opaque pointers. Define safe wrapper around `ZL_OpaquePtr` that ensures lifetime (Boxed state with leak on handoff, reclaimed via destructor callback if available; otherwise document process lifetime ownership by OpenZL).
- Phase 3: multi-input graph execution helpers `ZL_Graph_tryGraph`/`ZL_Graph_tryMultiInputGraph` beneath a `graphs` module.
- Add thorough tests to avoid use-after-free and ensure error propagation.

14) Performance validation
- Add micro-benchmarks (criterion) for serial and numeric paths.
- Measure impact of checksum parameters on decompression; record as doc comments.
- Confirm zero-copy for serial/numeric where possible; fallback to copy only on alignment/ownership constraints.

15) CI and toolchain hygiene
- GitHub Actions (or internal CI): linux + macOS; cache Cargo + CMake builds; run `git submodule update --init --recursive`.
- Pre-checks: rustfmt, clippy (deny on warnings for `openzl` only; keep `openzl-sys` permissive since it’s generated).
- Optional: docs.rs build for `openzl` with vendored build skipped (document how—docs.rs often disallows running cmake; consider `--cfg docsrs` to stub sys symbols or use `links` tricks and `cfg(doc)` mocks).

16) Versioning and release discipline
- Track OpenZL version via headers (`ZL_LIBRARY_VERSION_*`) and reflect it in crate metadata (e.g., in README and docs).
- Release plan:
  - Update submodule commit.
  - Re-run bindgen and reformat.
  - Bump `openzl-sys` patch/minor as needed; bump `openzl` in lockstep when public API changes.
- Tag and changelog entries describing upstream delta.

17) Windows backlog (explicitly deferred)
- Leave TODO to support MSVC/clang-cl. Keep Unix-first.
- Note upstream warning about C11/MSVC; prefer clang-cl toolset when implemented.

18) Safety and concurrency model
- `CCtx`/`DCtx` are not `Sync`. Consider `Send` if contexts are thread-confined and free of aliasing hazards.
- Typed views guarantee alignment only when produced by OpenZL; enforce checks when creating typed slices; otherwise expose bytes.
- Document lifetime coupling: borrowed inputs must outlive compression calls; owned outputs tie to `TypedBuffer` lifetime.

19) Developer utilities and scripts
- Optional `xtask` crate:
  - `xtask regen-bindings`
  - `xtask bump-openzl <rev>` to update submodule and verify build.
  - `xtask ci` to run full check locally.

20) Pitfalls and workarounds
- Bindgen may choke on complex macros; blocklist those or add small C wrappers.
- Ensure `ZL_getDecompressedSize` is trusted only for single-output frames.
- String type currently lacks wrapString in Python; for Rust we rely on `ZL_TypedBuffer` APIs for strings; avoid unimplemented paths in C++ helpers.
- Endianness for Numeric follows host; document cross-arch caveats.


## Coding Conventions
- `openzl-sys`: no `unsafe` abstractions beyond FFI; no public re-exports of bindgen internals except via `pub use` of generated items; helpers live in C shim.
- `openzl`: public APIs return `Result<T, Error>`; avoid `unsafe` in public surfaces; prefer `#[non_exhaustive]` on enums; small types `Copy` where possible.
- Tests: name includes behavior and type (e.g., `rt_serial_small`, `rt_numeric_u32_alignment`).


## Acceptance Criteria per Phase
- MVP complete when:
  - Serial and numeric round-trip APIs are stable and tested.
  - TypedRef and TypedBuffer wrappers implemented with Drop safety.
  - FrameInfo inspection implemented.
  - CI green on linux+mac.
- Advanced features gated behind features and covered by tests.


## Commands Reference
- Initialize submodules: `git submodule update --init --recursive`
- Build sys + safe crates: `cargo build -p openzl`
- Run tests: `cargo test -p openzl`
- Re-run with verbose CMake: `VERBOSE=1 cargo build -p openzl-sys`


## Maintenance Notes
- Submodule bumps may change headers; regenerate bindings and scan diffs for enum/ABI changes.
- If upstream adds/removes libs, update link search paths in `openzl-sys/build.rs` accordingly.
- Keep `AGENTS.md` updated when the plan changes.

