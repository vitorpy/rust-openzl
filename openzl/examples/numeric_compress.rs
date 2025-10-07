//! Example of numeric array compression with specialized algorithms.
//!
//! OpenZL's NUMERIC graph provides optimized compression for numeric data
//! by exploiting patterns in numeric sequences (deltas, bitpacking, etc.).

use openzl::{compress_numeric, decompress_numeric};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("=== Numeric Compression Examples ===\n");

    // Example 1: Sequential u32 data (highly compressible)
    println!("Example 1: Sequential u32 data");
    let sequential: Vec<u32> = (0..10000).collect();
    let original_size = sequential.len() * std::mem::size_of::<u32>();

    let compressed = compress_numeric(&sequential)?;

    println!("  Original size: {} bytes ({} elements)", original_size, sequential.len());
    println!("  Compressed size: {} bytes", compressed.len());
    println!(
        "  Compression ratio: {:.2}%",
        (compressed.len() as f64 / original_size as f64) * 100.0
    );

    let decompressed: Vec<u32> = decompress_numeric(&compressed)?;
    assert_eq!(sequential, decompressed);
    println!("  ✓ Round-trip successful!\n");

    // Example 2: u64 data with large values
    println!("Example 2: u64 data with timestamps");
    let timestamps: Vec<u64> = (0..1000).map(|i| 1700000000000 + i * 1000).collect();
    let original_size = timestamps.len() * std::mem::size_of::<u64>();

    let compressed = compress_numeric(&timestamps)?;

    println!("  Original size: {} bytes ({} elements)", original_size, timestamps.len());
    println!("  Compressed size: {} bytes", compressed.len());
    println!(
        "  Compression ratio: {:.2}%",
        (compressed.len() as f64 / original_size as f64) * 100.0
    );

    let decompressed: Vec<u64> = decompress_numeric(&compressed)?;
    assert_eq!(timestamps, decompressed);
    println!("  ✓ Round-trip successful!\n");

    // Example 3: i32 data with mixed positive/negative values
    println!("Example 3: i32 sensor readings");
    let sensor_data: Vec<i32> = (0..5000)
        .map(|i| ((i as f64 * 0.1).sin() * 100.0) as i32)
        .collect();
    let original_size = sensor_data.len() * std::mem::size_of::<i32>();

    let compressed = compress_numeric(&sensor_data)?;

    println!("  Original size: {} bytes ({} elements)", original_size, sensor_data.len());
    println!("  Compressed size: {} bytes", compressed.len());
    println!(
        "  Compression ratio: {:.2}%",
        (compressed.len() as f64 / original_size as f64) * 100.0
    );

    let decompressed: Vec<i32> = decompress_numeric(&compressed)?;
    assert_eq!(sensor_data, decompressed);
    println!("  ✓ Round-trip successful!\n");

    // Example 4: f64 floating-point data
    println!("Example 4: f64 scientific measurements");
    let measurements: Vec<f64> = (0..1000)
        .map(|i| i as f64 * 2.718281828 + 0.001)
        .collect();
    let original_size = measurements.len() * std::mem::size_of::<f64>();

    let compressed = compress_numeric(&measurements)?;

    println!("  Original size: {} bytes ({} elements)", original_size, measurements.len());
    println!("  Compressed size: {} bytes", compressed.len());
    println!(
        "  Compression ratio: {:.2}%",
        (compressed.len() as f64 / original_size as f64) * 100.0
    );

    let decompressed: Vec<f64> = decompress_numeric(&compressed)?;
    assert_eq!(measurements, decompressed);
    println!("  ✓ Round-trip successful!\n");

    println!("All numeric compression examples completed successfully!");

    Ok(())
}
