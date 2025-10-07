//! Example of graph-based compression using different compression graphs.
//!
//! OpenZL's core concept is compression graphs - they define HOW to compress data.
//! Different graphs are optimized for different data patterns.

use openzl::{compress_with_graph, decompress_serial, ZstdGraph, StoreGraph};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("=== Graph-Based Compression Examples ===\n");

    // Test data: repetitive text (compresses well)
    let text = b"The quick brown fox jumps over the lazy dog. ".repeat(100);
    let original_size = text.len();

    println!("Original data: {} bytes\n", original_size);

    // Example 1: ZSTD Graph (general-purpose compression)
    println!("Example 1: ZSTD Graph");
    println!("  Description: General-purpose compression using ZSTD algorithm");

    let compressed_zstd = compress_with_graph(&text, &ZstdGraph)?;
    let decompressed_zstd = decompress_serial(&compressed_zstd)?;

    println!("  Compressed size: {} bytes", compressed_zstd.len());
    println!(
        "  Compression ratio: {:.2}%",
        (compressed_zstd.len() as f64 / original_size as f64) * 100.0
    );
    println!(
        "  Space savings: {:.2}%",
        ((1.0 - compressed_zstd.len() as f64 / original_size as f64) * 100.0)
    );

    assert_eq!(text.as_slice(), decompressed_zstd.as_slice());
    println!("  ✓ Round-trip successful!\n");

    // Example 2: STORE Graph (no compression)
    println!("Example 2: STORE Graph");
    println!("  Description: No compression - useful for testing and already-compressed data");

    let compressed_store = compress_with_graph(&text, &StoreGraph)?;
    let decompressed_store = decompress_serial(&compressed_store)?;

    println!("  Compressed size: {} bytes", compressed_store.len());
    println!(
        "  Overhead: {} bytes (frame header)",
        compressed_store.len() - original_size
    );

    assert_eq!(text.as_slice(), decompressed_store.as_slice());
    println!("  ✓ Round-trip successful!\n");

    // Comparison
    println!("=== Compression Comparison ===");
    println!("  ZSTD:  {} bytes ({:.2}%)",
             compressed_zstd.len(),
             (compressed_zstd.len() as f64 / original_size as f64) * 100.0);
    println!("  STORE: {} bytes ({:.2}%)",
             compressed_store.len(),
             (compressed_store.len() as f64 / original_size as f64) * 100.0);

    println!("\nKey takeaway: Different graphs provide different trade-offs.");
    println!("Choose the graph that matches your data type and performance requirements.");

    Ok(())
}
