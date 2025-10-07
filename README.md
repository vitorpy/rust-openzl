# rust-openzl (vendored)

Rust bindings for OpenZL with a vendored C library (git submodule at `vendor/openzl`).

**OpenZL** is a graph-based typed compression library optimized for structured data. Unlike generic compressors (zlib/zstd), OpenZL uses compression graphs to define *how* to compress specific data types.

- Always builds against the vendored source; no system or network dependencies.
- `openzl-sys`: unsafe FFI + CMake build of the C library.
- `openzl`: safe, ergonomic API on top of `openzl-sys`.

## Quick Start

```bash
git submodule update --init --recursive
cargo build -p openzl
cargo test -p openzl
```

## Usage Examples

### Serial Compression (Generic Data)

```rust
use openzl::{compress_serial, decompress_serial};

let data = b"Hello, OpenZL!";
let compressed = compress_serial(data)?;
let decompressed = decompress_serial(&compressed)?;
assert_eq!(data.as_slice(), decompressed.as_slice());
```

### Numeric Compression (Optimized for Arrays)

```rust
use openzl::{compress_numeric, decompress_numeric};

// Compress numeric arrays with specialized algorithms
let data: Vec<u32> = (0..10000).collect();
let compressed = compress_numeric(&data)?;
let decompressed: Vec<u32> = decompress_numeric(&compressed)?;
assert_eq!(data, decompressed);

// Works with all numeric types: u8, u16, u32, u64, i8, i16, i32, i64, f32, f64
```

### Graph-Based Compression

```rust
use openzl::{compress_with_graph, decompress_serial, ZstdGraph, NumericGraph, StoreGraph};

// Use specific compression graphs for different data types
let data = b"Repeated data...".repeat(100);

// ZSTD graph for general-purpose compression
let compressed = compress_with_graph(&data, &ZstdGraph)?;

// Store graph for no compression (testing/debugging)
let stored = compress_with_graph(&data, &StoreGraph)?;
```

## Architecture

OpenZL is fundamentally a **graph-based typed compression** library:

- **Compression Graphs** define HOW to compress specific data structures
- **Standard Graphs**: ZSTD, NUMERIC, FIELD_LZ, STORE, ENTROPY, etc.
- **TypedRef**: Borrowed references to typed input data
- **TypedBuffer**: Owned decompression output buffers

This is NOT a drop-in replacement for zlib/zstd. Serial compression is just a compatibility shim.

## Testing

```bash
cargo test -p openzl
# Run with output to see compression ratios
cargo test -p openzl -- --nocapture
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

