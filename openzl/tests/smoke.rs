//! Smoke tests for basic compress/decompress round-trips

use openzl::{compress_serial, decompress_serial};

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
