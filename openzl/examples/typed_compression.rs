//! Example of TypedRef compression for typed structured data.
//!
//! TypedRef allows you to compress data with type information, enabling
//! OpenZL to apply type-specific optimizations.

use rust_openzl::{compress_typed_ref, decompress_typed_buffer, TypedRef};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("=== TypedRef Compression Examples ===\n");

    // Example 1: Serial (untyped) data
    println!("Example 1: Serial TypedRef");
    let text = b"Hello from TypedRef! This is untyped byte data.";
    let tref_serial = TypedRef::serial(text);

    let compressed = compress_typed_ref(&tref_serial)?;
    let tbuf = decompress_typed_buffer(&compressed)?;

    println!("  Original size: {} bytes", text.len());
    println!("  Compressed size: {} bytes", compressed.len());
    println!("  Data type: {:?}", tbuf.data_type());

    assert_eq!(text.as_slice(), tbuf.as_bytes());
    println!("  ✓ Round-trip successful!\n");

    // Example 2: Numeric u32 data
    println!("Example 2: Numeric TypedRef (u32)");
    let numbers: Vec<u32> = (0..1000).collect();
    let tref_numeric = TypedRef::numeric(&numbers)?;

    let compressed = compress_typed_ref(&tref_numeric)?;
    let tbuf = decompress_typed_buffer(&compressed)?;

    let original_size = numbers.len() * std::mem::size_of::<u32>();
    println!("  Original size: {} bytes ({} elements)", original_size, numbers.len());
    println!("  Compressed size: {} bytes", compressed.len());
    println!(
        "  Compression ratio: {:.2}%",
        (compressed.len() as f64 / original_size as f64) * 100.0
    );
    println!("  Data type: {:?}", tbuf.data_type());
    println!("  Element width: {} bytes", tbuf.elt_width());
    println!("  Element count: {}", tbuf.num_elts());

    let decompressed = tbuf.as_numeric::<u32>().expect("Failed to extract u32 data");
    assert_eq!(numbers.as_slice(), decompressed);
    println!("  ✓ Round-trip successful!\n");

    // Example 3: Numeric u64 data
    println!("Example 3: Numeric TypedRef (u64)");
    let large_numbers: Vec<u64> = (0..500).map(|i| i * 1000000).collect();
    let tref_numeric = TypedRef::numeric(&large_numbers)?;

    let compressed = compress_typed_ref(&tref_numeric)?;
    let tbuf = decompress_typed_buffer(&compressed)?;

    let original_size = large_numbers.len() * std::mem::size_of::<u64>();
    println!("  Original size: {} bytes ({} elements)", original_size, large_numbers.len());
    println!("  Compressed size: {} bytes", compressed.len());
    println!(
        "  Compression ratio: {:.2}%",
        (compressed.len() as f64 / original_size as f64) * 100.0
    );

    let decompressed = tbuf.as_numeric::<u64>().expect("Failed to extract u64 data");
    assert_eq!(large_numbers.as_slice(), decompressed);
    println!("  ✓ Round-trip successful!\n");

    // Example 4: Struct data
    println!("Example 4: Struct TypedRef");

    // Simulate a simple struct as raw bytes: [id: u32, value: u32]
    let struct_width = 8; // 2 * sizeof(u32)
    let struct_count = 100;
    let mut struct_data = Vec::with_capacity(struct_width * struct_count);

    for i in 0..struct_count {
        // Write id (u32)
        struct_data.extend_from_slice(&(i as u32).to_le_bytes());
        // Write value (u32)
        struct_data.extend_from_slice(&((i * 10) as u32).to_le_bytes());
    }

    let tref_struct = TypedRef::structs(&struct_data, struct_width, struct_count)?;

    let compressed = compress_typed_ref(&tref_struct)?;
    let tbuf = decompress_typed_buffer(&compressed)?;

    println!("  Original size: {} bytes ({} structs of {} bytes)",
             struct_data.len(), struct_count, struct_width);
    println!("  Compressed size: {} bytes", compressed.len());
    println!(
        "  Compression ratio: {:.2}%",
        (compressed.len() as f64 / struct_data.len() as f64) * 100.0
    );

    assert_eq!(struct_data.as_slice(), tbuf.as_bytes());
    println!("  ✓ Round-trip successful!\n");

    println!("All TypedRef compression examples completed successfully!");
    println!("\nNote: TypedRef provides type information to OpenZL, enabling");
    println!("type-specific compression optimizations not available with serial compression.");

    Ok(())
}
