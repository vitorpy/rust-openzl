# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2024-01-XX (Unreleased)

### Added

#### Core Functionality
- Initial implementation of safe Rust bindings for OpenZL
- Vendored OpenZL C library as git submodule (no system dependencies)
- CMake-based build system for OpenZL and zstd
- Bindgen-generated FFI bindings with Rust-style enums

#### Compression APIs
- `compress_serial()` / `decompress_serial()` - Generic byte compression
- `compress_numeric<T>()` / `decompress_numeric<T>()` - Type-optimized numeric compression
- `compress_with_graph()` - Graph-based compression with custom strategies
- `compress_typed_ref()` / `decompress_typed_buffer()` - Advanced typed compression

#### Compression Graphs (Core Feature)
- `Compressor` wrapper with RAII and Drop safety
- `GraphId` opaque type with validity checking
- Standard graph implementations: `ZstdGraph`, `NumericGraph`, `StoreGraph`, `FieldLzGraph`
- 9 standard graph constants: ZSTD, NUMERIC, STORE, FIELD_LZ, FSE, HUFFMAN, ENTROPY, BITPACK, CONSTANT
- `GraphFn` trait for custom compression strategies
- C callback trampolines for graph registration

#### Type System
- `TypedRef` - Borrowed references to typed input data with lifetime safety
- `TypedBuffer` - Owned decompression output buffers with type inspection
- Support for serial, numeric, struct, and string data types
- Type validation for numeric compression (1, 2, 4, 8 byte widths)

#### Error Handling
- `Error` type with detailed error context
- `Warning` type for non-fatal issues
- Context extraction from compression/decompression operations

#### Documentation
- Comprehensive module-level documentation with architecture diagrams
- Four complete examples: serial, numeric, graph-based, and typed compression
- README with quick start guide and usage examples
- Doc tests for all major APIs

#### Testing
- 16 unit tests covering serial, graph-based, and typed compression
- 6 tests for different numeric types (u16, u32, u64, i32, f32, f64)
- 2 tests for graph-based compression (ZSTD, STORE)
- 3 tests for TypedRef compression
- All tests passing on Linux

### Performance

Compression ratios on test data:
- Sequential u32 data: 0.30% (400:1 ratio)
- Timestamps (u64): 2.16% (46:1 ratio)
- Repetitive text: 1.96% (51:1 ratio)
- Sensor readings (i32): 5.91% (17:1 ratio)

### Architecture

OpenZL is a graph-based typed compression library (not a generic compressor):
- Compression graphs define HOW to compress specific data structures
- Type-specific optimizations (delta encoding, bitpacking, transpose)
- Serial compression is a compatibility shim for untyped data

### Safety Guarantees

- RAII wrappers with Drop for automatic resource cleanup
- Lifetime-checked TypedRef to prevent use-after-free
- Type validation for numeric compression
- All unsafe code isolated in openzl-sys FFI layer

## [Unreleased]

### Planned Features

- Frame inspection utilities (FrameInfo)
- Parameter configuration (compression levels, checksums)
- Custom graph registration API
- String compression support
- Multi-output frame support
- Reflection and graph discovery
- Property-based testing (proptest)
- CI/CD setup (GitHub Actions)
- Windows support (MSVC/clang-cl)

[0.1.0]: https://github.com/vitorpy/rust-openzl/releases/tag/v0.1.0
