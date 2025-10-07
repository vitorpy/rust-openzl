//! Simple example of serial compression and decompression.
//!
//! This demonstrates the basic usage of OpenZL for generic byte data.

use rust_openzl::{compress_serial, decompress_serial};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Original data
    let original = b"Hello, OpenZL! This is a simple example of serial compression. \
                     Serial compression works on raw byte arrays without type information. \
                     It's the simplest way to use OpenZL, similar to zlib or zstd.";

    println!("Original size: {} bytes", original.len());
    println!("Original data: {}", String::from_utf8_lossy(original));

    // Compress
    let compressed = compress_serial(original)?;
    println!("\nCompressed size: {} bytes", compressed.len());
    println!(
        "Compression ratio: {:.2}%",
        (compressed.len() as f64 / original.len() as f64) * 100.0
    );

    // Decompress
    let decompressed = decompress_serial(&compressed)?;
    println!("\nDecompressed size: {} bytes", decompressed.len());

    // Verify round-trip
    assert_eq!(original.as_slice(), decompressed.as_slice());
    println!("✓ Round-trip successful!");

    Ok(())
}
