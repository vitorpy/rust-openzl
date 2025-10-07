//! Smoke tests for basic compress/decompress round-trips

use rust_openzl::{
    compress_serial, decompress_serial,
    compress_typed_ref, decompress_typed_buffer,
    compress_with_graph, compress_numeric, decompress_numeric,
    TypedRef, ZstdGraph, NumericGraph, StoreGraph,
};

#[test]
fn rt_serial_empty() {
    let src = b"";
    let compressed = compress_serial(src).expect("compress empty");
    let decompressed = decompress_serial(&compressed).expect("decompress empty");
    assert_eq!(src.as_slice(), decompressed.as_slice());
}

#[test]
fn rt_serial_small() {
    let src = b"Hello, OpenZL! This is a small test buffer.";
    let compressed = compress_serial(src).expect("compress small");
    let decompressed = decompress_serial(&compressed).expect("decompress small");
    assert_eq!(src.as_slice(), decompressed.as_slice());
}

#[test]
fn rt_serial_medium() {
    let src = b"Lorem ipsum dolor sit amet, consectetur adipiscing elit. ".repeat(100);
    let compressed = compress_serial(&src).expect("compress medium");
    assert!(compressed.len() < src.len(), "compressed should be smaller");
    let decompressed = decompress_serial(&compressed).expect("decompress medium");
    assert_eq!(src.as_slice(), decompressed.as_slice());
}

#[test]
fn rt_serial_large() {
    // 1MB of repeated pattern
    let pattern = b"The quick brown fox jumps over the lazy dog. ";
    let src = pattern.repeat(1024 * 1024 / pattern.len());
    let compressed = compress_serial(&src).expect("compress large");
    assert!(compressed.len() < src.len(), "compressed should be much smaller for repeated data");
    let decompressed = decompress_serial(&compressed).expect("decompress large");
    assert_eq!(src.as_slice(), decompressed.as_slice());
}

#[test]
fn rt_serial_random_like() {
    // Less compressible data (sequence of numbers)
    let src: Vec<u8> = (0..=255).cycle().take(10000).collect();
    let compressed = compress_serial(&src).expect("compress random-like");
    let decompressed = decompress_serial(&compressed).expect("decompress random-like");
    assert_eq!(src.as_slice(), decompressed.as_slice());
}

// TypedRef compression tests now work with graph support (Step 4 Phase C complete)

#[test]
fn rt_typed_numeric_u32() {
    let data: Vec<u32> = (0..1000).collect();
    let tref = TypedRef::numeric(&data).expect("create numeric TypedRef");

    let compressed = compress_typed_ref(&tref).expect("compress numeric");
    let tbuf = decompress_typed_buffer(&compressed).expect("decompress numeric");

    // Verify type
    assert_eq!(tbuf.data_type(), rust_openzl_sys::ZL_Type::ZL_Type_numeric);
    assert_eq!(tbuf.elt_width(), 4);
    assert_eq!(tbuf.num_elts(), 1000);

    // Verify data
    let decompressed = tbuf.as_numeric::<u32>().expect("get numeric data");
    assert_eq!(decompressed, data.as_slice());
}

#[test]
fn rt_typed_numeric_u64() {
    let data: Vec<u64> = (0..500).map(|i| i * 1000).collect();
    let tref = TypedRef::numeric(&data).expect("create numeric TypedRef");

    let compressed = compress_typed_ref(&tref).expect("compress numeric");
    let tbuf = decompress_typed_buffer(&compressed).expect("decompress numeric");

    assert_eq!(tbuf.data_type(), rust_openzl_sys::ZL_Type::ZL_Type_numeric);
    assert_eq!(tbuf.elt_width(), 8);

    let decompressed = tbuf.as_numeric::<u64>().expect("get numeric data");
    assert_eq!(decompressed, data.as_slice());
}

#[test]
fn rt_typed_serial() {
    let data = b"Hello, TypedRef world!";
    let tref = TypedRef::serial(data);

    let compressed = compress_typed_ref(&tref).expect("compress serial TypedRef");
    let tbuf = decompress_typed_buffer(&compressed).expect("decompress serial");

    assert_eq!(tbuf.data_type(), rust_openzl_sys::ZL_Type::ZL_Type_serial);
    assert_eq!(tbuf.as_bytes(), data);
}

#[test]
#[ignore = "String compression requires custom graph (Step 12)"]
fn rt_typed_strings() {
    // NOTE: String type requires a custom graph registration.
    // The default ZSTD graph doesn't support string inputs.
    // This will be enabled in Step 12 (custom graph registration).
    let strings = vec!["hello", "world", "foo", "bar"];
    let flat: Vec<u8> = strings.iter().flat_map(|s| s.bytes()).collect();
    let lens: Vec<u32> = strings.iter().map(|s| s.len() as u32).collect();

    let tref = TypedRef::strings(&flat, &lens);

    let compressed = compress_typed_ref(&tref).expect("compress strings");
    let tbuf = decompress_typed_buffer(&compressed).expect("decompress strings");

    assert_eq!(tbuf.data_type(), rust_openzl_sys::ZL_Type::ZL_Type_string);
    assert_eq!(tbuf.as_bytes(), flat.as_slice());
    assert_eq!(tbuf.string_lens().expect("get string lens"), lens.as_slice());
}

// ============================================================================
// Graph-based compression tests (Step 4 Phase B)
// ============================================================================

#[test]
fn graph_zstd_roundtrip() {
    let src = b"Hello, OpenZL! This is a test of graph-based compression.".repeat(10);
    let compressed = compress_with_graph(&src, &ZstdGraph).expect("compress with ZSTD graph");
    assert!(compressed.len() < src.len(), "ZSTD should compress repeated data");

    let decompressed = decompress_serial(&compressed).expect("decompress");
    assert_eq!(src.as_slice(), decompressed.as_slice());
}

#[test]
#[ignore = "NUMERIC graph requires typed numeric input (TypedRef), will be enabled in Step 5"]
fn graph_numeric_roundtrip() {
    // NOTE: The NUMERIC graph is designed for typed numeric data, not serial bytes.
    // This test will be updated in Step 5 when we integrate TypedRef with graph compression.
    let src = b"0123456789".repeat(100);
    let compressed = compress_with_graph(&src, &NumericGraph).expect("compress with NUMERIC graph");

    let decompressed = decompress_serial(&compressed).expect("decompress");
    assert_eq!(src.as_slice(), decompressed.as_slice());
}

#[test]
fn graph_store_roundtrip() {
    let src = b"Hello, Store graph!";
    let compressed = compress_with_graph(src, &StoreGraph).expect("compress with STORE graph");
    // Store graph should not compress (may add overhead for frame header)

    let decompressed = decompress_serial(&compressed).expect("decompress");
    assert_eq!(src, decompressed.as_slice());
}

// ============================================================================
// High-level ergonomic APIs (Step 9)
// ============================================================================

#[test]
fn compress_numeric_u32_roundtrip() {
    let data: Vec<u32> = (0..1000).collect();
    let compressed = compress_numeric(&data).expect("compress numeric u32");
    let decompressed: Vec<u32> = decompress_numeric(&compressed).expect("decompress numeric u32");
    assert_eq!(data, decompressed);
}

#[test]
fn compress_numeric_u64_roundtrip() {
    let data: Vec<u64> = (0..500).map(|i| i * 1000).collect();
    let compressed = compress_numeric(&data).expect("compress numeric u64");

    // Should compress well due to numeric patterns
    println!("u64 data size: {} bytes, compressed: {} bytes",
             data.len() * 8, compressed.len());

    let decompressed: Vec<u64> = decompress_numeric(&compressed).expect("decompress numeric u64");
    assert_eq!(data, decompressed);
}

#[test]
fn compress_numeric_u16_roundtrip() {
    let data: Vec<u16> = (0..2000).map(|i| (i % 1000) as u16).collect();
    let compressed = compress_numeric(&data).expect("compress numeric u16");
    let decompressed: Vec<u16> = decompress_numeric(&compressed).expect("decompress numeric u16");
    assert_eq!(data, decompressed);
}

#[test]
fn compress_numeric_i32_roundtrip() {
    let data: Vec<i32> = (-500..500).collect();
    let compressed = compress_numeric(&data).expect("compress numeric i32");
    let decompressed: Vec<i32> = decompress_numeric(&compressed).expect("decompress numeric i32");
    assert_eq!(data, decompressed);
}

#[test]
fn compress_numeric_f32_roundtrip() {
    let data: Vec<f32> = (0..100).map(|i| i as f32 * 1.5).collect();
    let compressed = compress_numeric(&data).expect("compress numeric f32");
    let decompressed: Vec<f32> = decompress_numeric(&compressed).expect("decompress numeric f32");
    assert_eq!(data, decompressed);
}

#[test]
fn compress_numeric_f64_roundtrip() {
    let data: Vec<f64> = (0..100).map(|i| i as f64 * 2.718281828).collect();
    let compressed = compress_numeric(&data).expect("compress numeric f64");
    let decompressed: Vec<f64> = decompress_numeric(&compressed).expect("decompress numeric f64");
    assert_eq!(data, decompressed);
}
