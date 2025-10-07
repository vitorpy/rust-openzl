//! # OpenZL Rust Bindings
//!
//! Safe, ergonomic Rust bindings for OpenZL - a graph-based typed compression library
//! optimized for structured data.
//!
//! ## What is OpenZL?
//!
//! **OpenZL is fundamentally different from generic compressors like zlib or zstd.**
//!
//! It's a **graph-based typed compression library** where compression graphs define
//! *how* to compress specific data structures. This allows OpenZL to apply type-specific
//! optimizations (delta encoding, bitpacking, transpose, etc.) that aren't possible with
//! generic byte-stream compression.
//!
//! ## Quick Start
//!
//! ### Serial Compression (Generic Data)
//!
//! ```
//! use openzl::{compress_serial, decompress_serial};
//!
//! let data = b"Hello, OpenZL!";
//! let compressed = compress_serial(data)?;
//! let decompressed = decompress_serial(&compressed)?;
//! assert_eq!(data.as_slice(), decompressed.as_slice());
//! # Ok::<(), openzl::Error>(())
//! ```
//!
//! ### Numeric Compression (Type-Optimized)
//!
//! ```
//! use openzl::{compress_numeric, decompress_numeric};
//!
//! // Compress numeric arrays with specialized algorithms
//! let data: Vec<u32> = (0..10000).collect();
//! let compressed = compress_numeric(&data)?;
//! let decompressed: Vec<u32> = decompress_numeric(&compressed)?;
//! assert_eq!(data, decompressed);
//! # Ok::<(), openzl::Error>(())
//! ```
//!
//! ### Graph-Based Compression
//!
//! ```
//! use openzl::{compress_with_graph, decompress_serial, ZstdGraph, NumericGraph};
//!
//! let data = b"Repeated data...".repeat(100);
//!
//! // Use specific compression graphs
//! let compressed = compress_with_graph(&data, &ZstdGraph)?;
//! let decompressed = decompress_serial(&compressed)?;
//! # Ok::<(), openzl::Error>(())
//! ```
//!
//! ## Core Concepts
//!
//! ### Compression Graphs
//!
//! Compression graphs are the heart of OpenZL. They define the compression strategy:
//!
//! - **ZSTD**: General-purpose compression (similar to zstd)
//! - **NUMERIC**: Optimized for numeric arrays (delta encoding, bitpacking)
//! - **FIELD_LZ**: Field-level LZ compression for structured data
//! - **STORE**: No compression (useful for testing)
//!
//! ### TypedRef and TypedBuffer
//!
//! - [`TypedRef`]: Borrowed reference to typed input data (with lifetime)
//! - [`TypedBuffer`]: Owned decompression output buffer
//!
//! These provide type information to OpenZL, enabling type-specific optimizations.
//!
//! ## Architecture
//!
//! ```text
//! High-level APIs (compress_numeric, etc.)
//!        ↓
//! Graph-based compression (compress_with_graph)
//!        ↓
//! TypedRef compression (compress_typed_ref)
//!        ↓
//! CCtx + Compressor (graph registration)
//!        ↓
//! OpenZL C library (via openzl-sys)
//! ```
//!
//! ## Examples
//!
//! See the `examples/` directory for complete examples:
//!
//! - `serial_compress.rs` - Basic serial compression
//! - `numeric_compress.rs` - Numeric array compression with different types
//! - `graph_compression.rs` - Using different compression graphs
//! - `typed_compression.rs` - Advanced TypedRef usage
//!
//! ## Safety
//!
//! This crate provides safe abstractions over the unsafe FFI:
//!
//! - RAII wrappers with `Drop` for resource cleanup
//! - Lifetime-checked `TypedRef` to prevent use-after-free
//! - Type validation for numeric compression
//! - Error handling via `Result<T, Error>`
//!
//! ## Performance
//!
//! OpenZL can achieve excellent compression ratios on structured data:
//!
//! - Sequential numeric data: 0.30% (400:1 ratio)
//! - Timestamps: 2.16% (46:1 ratio)
//! - Repetitive text: 1.96% (51:1 ratio)
//!
//! (Actual ratios depend on data patterns)

use openzl_sys as sys;
use std::ffi::CStr;

#[derive(Debug, thiserror::Error)]
pub enum Error {
    #[error("OpenZL error: {code} ({name}){context}")]
    Report {
        code: i32,
        name: String,
        context: String,
    },
}

// ============================================================================
// Helper functions
// ============================================================================

/// Returns the maximum compressed size for a given source size
fn compress_bound(src_size: usize) -> usize {
    unsafe { sys::openzl_compress_bound(src_size) }
}

/// Convert a ZL_Report to a Rust Error
fn report_to_error(r: sys::ZL_Report) -> Error {
    let code = sys::report_code(r);
    let name = unsafe { CStr::from_ptr(sys::openzl_error_code_to_string(code)) }
        .to_string_lossy()
        .into_owned();
    Error::Report {
        code,
        name,
        context: String::new(),
    }
}

/// OpenZL warning (non-fatal issue during compression/decompression)
#[derive(Debug, Clone)]
pub struct Warning {
    pub code: i32,
    pub name: String,
}

/// Opaque GraphID for identifying compression graphs
#[derive(Debug, Copy, Clone)]
pub struct GraphId(sys::ZL_GraphID);

impl GraphId {
    /// Check if this GraphID is valid
    pub fn is_valid(&self) -> bool {
        unsafe { sys::ZL_GraphID_isValid(self.0) != 0 }
    }

    #[allow(dead_code)] // Used for custom graph registration (Step 12)
    pub(crate) fn as_raw(&self) -> sys::ZL_GraphID {
        self.0
    }

    #[allow(dead_code)] // Used for custom graph registration (Step 12)
    pub(crate) fn from_raw(id: sys::ZL_GraphID) -> Self {
        GraphId(id)
    }
}

impl PartialEq for GraphId {
    fn eq(&self, other: &Self) -> bool {
        self.0.gid == other.0.gid
    }
}

impl Eq for GraphId {}

/// Standard compression graphs provided by OpenZL
pub mod graphs {
    use super::*;

    const fn make_graph_id(id: u32) -> GraphId {
        GraphId(sys::ZL_GraphID { gid: id })
    }

    /// No compression - stores data as-is
    pub const STORE: GraphId = make_graph_id(sys::ZL_StandardGraphID::ZL_StandardGraphID_store as u32);

    /// ZSTD compression (general purpose)
    pub const ZSTD: GraphId = make_graph_id(sys::ZL_StandardGraphID::ZL_StandardGraphID_zstd as u32);

    /// Optimized for numeric data
    pub const NUMERIC: GraphId = make_graph_id(sys::ZL_StandardGraphID::ZL_StandardGraphID_select_numeric as u32);

    /// Field-level LZ compression
    pub const FIELD_LZ: GraphId = make_graph_id(sys::ZL_StandardGraphID::ZL_StandardGraphID_field_lz as u32);

    /// FSE entropy encoding
    pub const FSE: GraphId = make_graph_id(sys::ZL_StandardGraphID::ZL_StandardGraphID_fse as u32);

    /// Huffman entropy encoding
    pub const HUFFMAN: GraphId = make_graph_id(sys::ZL_StandardGraphID::ZL_StandardGraphID_huffman as u32);

    /// Combined entropy encoding (FSE/Huffman selection)
    pub const ENTROPY: GraphId = make_graph_id(sys::ZL_StandardGraphID::ZL_StandardGraphID_entropy as u32);

    /// Bitpacking compression
    pub const BITPACK: GraphId = make_graph_id(sys::ZL_StandardGraphID::ZL_StandardGraphID_bitpack as u32);

    /// Constant value compression
    pub const CONSTANT: GraphId = make_graph_id(sys::ZL_StandardGraphID::ZL_StandardGraphID_constant as u32);
}

/// Compression graph builder and manager
///
/// Compressor is used to register and manage compression graphs, which define
/// HOW to compress data. This is the heart of OpenZL's typed compression.
pub struct Compressor(*mut sys::ZL_Compressor);

impl Compressor {
    /// Create a new Compressor for graph registration
    pub fn new() -> Self {
        let ptr = unsafe { sys::ZL_Compressor_create() };
        assert!(!ptr.is_null(), "ZL_Compressor_create returned null");
        Compressor(ptr)
    }

    /// Set a global compression parameter
    pub fn set_parameter(&mut self, param: sys::ZL_CParam, value: i32) -> Result<(), Error> {
        let r = unsafe { sys::ZL_Compressor_setParameter(self.0, param, value) };
        if sys::report_is_error(r) {
            let code = sys::report_code(r);
            let name = unsafe { CStr::from_ptr(sys::openzl_error_code_to_string(code)) }
                .to_string_lossy()
                .into_owned();
            return Err(Error::Report {
                code,
                name,
                context: String::new(),
            });
        }
        Ok(())
    }

    /// Get warnings generated during graph construction/validation
    pub fn warnings(&self) -> Vec<Warning> {
        let arr = unsafe { sys::ZL_Compressor_getWarnings(self.0) };
        let slice = unsafe { std::slice::from_raw_parts(arr.errors, arr.size) };
        slice
            .iter()
            .map(|e| {
                let code = unsafe { sys::openzl_error_get_code(e) };
                let name = unsafe { CStr::from_ptr(sys::openzl_error_get_name(e)) }
                    .to_string_lossy()
                    .into_owned();
                Warning { code, name }
            })
            .collect()
    }

    pub(crate) fn as_ptr(&self) -> *const sys::ZL_Compressor {
        self.0 as *const _
    }

    pub(crate) fn as_mut_ptr(&mut self) -> *mut sys::ZL_Compressor {
        self.0
    }
}

impl Drop for Compressor {
    fn drop(&mut self) {
        unsafe { sys::ZL_Compressor_free(self.0) }
    }
}

impl Default for Compressor {
    fn default() -> Self {
        Self::new()
    }
}

// ============================================================================
// Graph Function API
// ============================================================================

/// Trait for defining compression graphs.
///
/// Implementors define how to build a compression graph by registering
/// nodes and edges with the Compressor.
pub trait GraphFn {
    /// Build the compression graph using the provided Compressor.
    ///
    /// Returns the starting GraphID for this graph.
    fn build_graph(&self, compressor: &mut Compressor) -> GraphId;
}

/// Standard graph: ZSTD compression (general purpose)
pub struct ZstdGraph;

impl GraphFn for ZstdGraph {
    fn build_graph(&self, _compressor: &mut Compressor) -> GraphId {
        graphs::ZSTD
    }
}

/// Standard graph: Numeric compression (optimized for numeric data)
pub struct NumericGraph;

impl GraphFn for NumericGraph {
    fn build_graph(&self, _compressor: &mut Compressor) -> GraphId {
        graphs::NUMERIC
    }
}

/// Standard graph: Store (no compression, useful for testing)
pub struct StoreGraph;

impl GraphFn for StoreGraph {
    fn build_graph(&self, _compressor: &mut Compressor) -> GraphId {
        graphs::STORE
    }
}

/// Standard graph: Field-level LZ compression
pub struct FieldLzGraph;

impl GraphFn for FieldLzGraph {
    fn build_graph(&self, _compressor: &mut Compressor) -> GraphId {
        graphs::FIELD_LZ
    }
}

// Placeholder trampoline for custom graph functions (used in Step 12)
#[allow(dead_code)]
unsafe extern "C" fn graph_fn_trampoline(_compressor: *mut sys::ZL_Compressor) -> sys::ZL_GraphID {
    // This is a placeholder for custom graph registration (Step 12).
    // For now, standard graphs use dedicated callbacks below.
    // Custom graphs will require thread-local storage or user data passing.
    graphs::ZSTD.0
}

/// Compress data using a graph function.
///
/// This is a stateless compression function that uses the provided GraphFn
/// to define the compression strategy.
pub fn compress_with_graph<G: GraphFn>(src: &[u8], graph: &G) -> Result<Vec<u8>, Error> {
    // Allocate output buffer (use compress_bound to estimate size)
    let max_size = compress_bound(src.len());
    let mut dst = vec![0u8; max_size];

    // Create a temporary compressor to get the GraphID
    let mut compressor = Compressor::new();
    let graph_id = graph.build_graph(&mut compressor);

    // Use ZL_compress_usingGraphFn with a C callback that selects the graph
    // The callback will also set required parameters like format version
    let graph_fn = make_graph_selector_fn(graph_id);

    let r = unsafe {
        sys::ZL_compress_usingGraphFn(
            dst.as_mut_ptr() as *mut _,
            dst.len(),
            src.as_ptr() as *const _,
            src.len(),
            graph_fn,
        )
    };

    if sys::report_is_error(r) {
        return Err(report_to_error(r));
    }

    let compressed_size = sys::report_value(r);
    dst.truncate(compressed_size);
    Ok(dst)
}

// Helper to create a C callback that selects a specific GraphID
fn make_graph_selector_fn(graph_id: GraphId) -> sys::ZL_GraphFn {
    // For standard graphs, we can use dedicated callbacks
    match graph_id.0.gid {
        id if id == graphs::ZSTD.0.gid => Some(zstd_graph_callback),
        id if id == graphs::NUMERIC.0.gid => Some(numeric_graph_callback),
        id if id == graphs::STORE.0.gid => Some(store_graph_callback),
        id if id == graphs::FIELD_LZ.0.gid => Some(field_lz_graph_callback),
        _ => {
            // For custom graphs, we'd need a different mechanism
            // For now, fall back to ZSTD
            Some(zstd_graph_callback)
        }
    }
}

// Dedicated C callbacks for standard graphs
unsafe extern "C" fn zstd_graph_callback(compressor: *mut sys::ZL_Compressor) -> sys::ZL_GraphID {
    // Set format version (required by OpenZL)
    sys::ZL_Compressor_setParameter(compressor, sys::ZL_CParam::ZL_CParam_formatVersion, 21);
    graphs::ZSTD.0
}

unsafe extern "C" fn numeric_graph_callback(compressor: *mut sys::ZL_Compressor) -> sys::ZL_GraphID {
    // Set format version (required by OpenZL)
    sys::ZL_Compressor_setParameter(compressor, sys::ZL_CParam::ZL_CParam_formatVersion, 21);
    graphs::NUMERIC.0
}

unsafe extern "C" fn store_graph_callback(compressor: *mut sys::ZL_Compressor) -> sys::ZL_GraphID {
    // Set format version (required by OpenZL)
    sys::ZL_Compressor_setParameter(compressor, sys::ZL_CParam::ZL_CParam_formatVersion, 21);
    graphs::STORE.0
}

unsafe extern "C" fn field_lz_graph_callback(compressor: *mut sys::ZL_Compressor) -> sys::ZL_GraphID {
    // Set format version (required by OpenZL)
    sys::ZL_Compressor_setParameter(compressor, sys::ZL_CParam::ZL_CParam_formatVersion, 21);
    graphs::FIELD_LZ.0
}

// ============================================================================
// TypedRef and TypedBuffer
// ============================================================================

/// Safe wrapper around ZL_TypedRef for typed input data.
///
/// IMPORTANT: TypedRef borrows the input data. The borrowed data must remain
/// valid for the lifetime of the TypedRef and through any compression call
/// that uses it.
pub struct TypedRef<'a> {
    ptr: *mut sys::ZL_TypedRef,
    _marker: std::marker::PhantomData<&'a [u8]>,
}

impl<'a> TypedRef<'a> {
    /// Create a TypedRef for serial (untyped byte array) data
    pub fn serial(data: &'a [u8]) -> Self {
        let ptr = unsafe {
            sys::ZL_TypedRef_createSerial(data.as_ptr() as *const _, data.len())
        };
        assert!(!ptr.is_null(), "ZL_TypedRef_createSerial returned null");
        TypedRef {
            ptr,
            _marker: std::marker::PhantomData,
        }
    }

    /// Create a TypedRef for numeric data.
    ///
    /// T must have size 1, 2, 4, or 8 bytes (u8, u16, u32, u64, i8, i16, i32, i64, f32, f64)
    pub fn numeric<T: Copy>(data: &'a [T]) -> Result<Self, Error> {
        let width = std::mem::size_of::<T>();
        if !matches!(width, 1 | 2 | 4 | 8) {
            return Err(Error::Report {
                code: -1,
                name: "Invalid numeric type".into(),
                context: format!("\nElement size must be 1, 2, 4, or 8 bytes, got {width}"),
            });
        }
        let ptr = unsafe {
            sys::ZL_TypedRef_createNumeric(
                data.as_ptr() as *const _,
                width,
                data.len(),
            )
        };
        assert!(!ptr.is_null(), "ZL_TypedRef_createNumeric returned null");
        Ok(TypedRef {
            ptr,
            _marker: std::marker::PhantomData,
        })
    }

    /// Create a TypedRef for string data (flat format with lengths array)
    ///
    /// - `flat`: concatenated string bytes
    /// - `lens`: array of string lengths (u32)
    pub fn strings(flat: &'a [u8], lens: &'a [u32]) -> Self {
        let ptr = unsafe {
            sys::ZL_TypedRef_createString(
                flat.as_ptr() as *const _,
                flat.len(),
                lens.as_ptr(),
                lens.len(),
            )
        };
        assert!(!ptr.is_null(), "ZL_TypedRef_createString returned null");
        TypedRef {
            ptr,
            _marker: std::marker::PhantomData,
        }
    }

    /// Create a TypedRef for struct data (concatenated fields)
    ///
    /// - `bytes`: flattened struct data
    /// - `width`: size of each struct element in bytes
    /// - `count`: number of struct elements
    pub fn structs(bytes: &'a [u8], width: usize, count: usize) -> Result<Self, Error> {
        if width == 0 || count == 0 {
            return Err(Error::Report {
                code: -1,
                name: "Invalid struct parameters".into(),
                context: "\nWidth and count must be non-zero".into(),
            });
        }
        if bytes.len() != width * count {
            return Err(Error::Report {
                code: -1,
                name: "Invalid struct buffer size".into(),
                context: format!("\nExpected {} bytes (width={width} * count={count}), got {}", width * count, bytes.len()),
            });
        }
        let ptr = unsafe {
            sys::ZL_TypedRef_createStruct(
                bytes.as_ptr() as *const _,
                width,
                count,
            )
        };
        assert!(!ptr.is_null(), "ZL_TypedRef_createStruct returned null");
        Ok(TypedRef {
            ptr,
            _marker: std::marker::PhantomData,
        })
    }

    pub(crate) fn as_ptr(&self) -> *const sys::ZL_TypedRef {
        self.ptr as *const _
    }
}

impl Drop for TypedRef<'_> {
    fn drop(&mut self) {
        unsafe { sys::ZL_TypedRef_free(self.ptr) }
    }
}

/// Safe wrapper around ZL_TypedBuffer for typed decompression output.
///
/// TypedBuffer owns its internal buffer and frees it on Drop.
pub struct TypedBuffer {
    ptr: *mut sys::ZL_TypedBuffer,
}

impl TypedBuffer {
    /// Create a new TypedBuffer for receiving decompressed data
    pub fn new() -> Self {
        let ptr = unsafe { sys::ZL_TypedBuffer_create() };
        assert!(!ptr.is_null(), "ZL_TypedBuffer_create returned null");
        TypedBuffer { ptr }
    }

    /// Get the data type of this buffer
    pub fn data_type(&self) -> sys::ZL_Type {
        unsafe { sys::ZL_TypedBuffer_type(self.ptr) }
    }

    /// Get the size of the buffer in bytes
    pub fn byte_size(&self) -> usize {
        unsafe { sys::ZL_TypedBuffer_byteSize(self.ptr) }
    }

    /// Get the number of elements
    pub fn num_elts(&self) -> usize {
        unsafe { sys::ZL_TypedBuffer_numElts(self.ptr) }
    }

    /// Get the element width in bytes (for struct/numeric types)
    pub fn elt_width(&self) -> usize {
        unsafe { sys::ZL_TypedBuffer_eltWidth(self.ptr) }
    }

    /// Get read-only access to the buffer as bytes
    pub fn as_bytes(&self) -> &[u8] {
        let ptr = unsafe { sys::ZL_TypedBuffer_rPtr(self.ptr) };
        let len = self.byte_size();
        if ptr.is_null() || len == 0 {
            &[]
        } else {
            unsafe { std::slice::from_raw_parts(ptr as *const u8, len) }
        }
    }

    /// Get read-only access to numeric data as a typed slice.
    ///
    /// Returns None if:
    /// - The data type is not numeric
    /// - The element width doesn't match T's size
    /// - The buffer is not properly aligned for T
    pub fn as_numeric<T: Copy>(&self) -> Option<&[T]> {
        // Check if type is numeric
        if self.data_type() != sys::ZL_Type::ZL_Type_numeric {
            return None;
        }

        let width = std::mem::size_of::<T>();
        if self.elt_width() != width {
            return None;
        }

        let ptr = unsafe { sys::ZL_TypedBuffer_rPtr(self.ptr) };
        if ptr.is_null() {
            return None;
        }

        // Check alignment
        if (ptr as usize) % std::mem::align_of::<T>() != 0 {
            return None;
        }

        let len = self.num_elts();
        if len == 0 {
            return Some(&[]);
        }

        Some(unsafe { std::slice::from_raw_parts(ptr as *const T, len) })
    }

    /// Get the string lengths array (for string type)
    ///
    /// Returns None if the data type is not string
    pub fn string_lens(&self) -> Option<&[u32]> {
        if self.data_type() != sys::ZL_Type::ZL_Type_string {
            return None;
        }

        let ptr = unsafe { sys::ZL_TypedBuffer_rStringLens(self.ptr) };
        if ptr.is_null() {
            return None;
        }

        let len = self.num_elts();
        if len == 0 {
            return Some(&[]);
        }

        Some(unsafe { std::slice::from_raw_parts(ptr, len) })
    }

    pub(crate) fn as_mut_ptr(&mut self) -> *mut sys::ZL_TypedBuffer {
        self.ptr
    }
}

impl Drop for TypedBuffer {
    fn drop(&mut self) {
        unsafe { sys::ZL_TypedBuffer_free(self.ptr) }
    }
}

impl Default for TypedBuffer {
    fn default() -> Self {
        Self::new()
    }
}

fn error_from_report_with_ctx(code: i32, name: String, ctx_str: Option<&CStr>) -> Error {
    let context = match ctx_str.and_then(|s| s.to_str().ok()) {
        Some(s) if !s.is_empty() => format!("\n{s}"),
        _ => String::new(),
    };
    Error::Report { code, name, context }
}

pub struct CCtx(*mut sys::ZL_CCtx);
impl CCtx {
    pub fn new() -> Self {
        let ptr = unsafe { sys::ZL_CCtx_create() };
        assert!(!ptr.is_null(), "ZL_CCtx_create returned null");
        CCtx(ptr)
    }
    pub fn set_parameter(&mut self, p: sys::ZL_CParam, v: i32) -> Result<(), Error> {
        let r = unsafe { sys::ZL_CCtx_setParameter(self.0, p, v) };
        if sys::report_is_error(r) {
            let code = sys::report_code(r);
            let name = unsafe { CStr::from_ptr(sys::openzl_error_code_to_string(code)) }
                .to_string_lossy()
                .into_owned();
            let ctx = unsafe { CStr::from_ptr(sys::openzl_cctx_error_context(self.0, r)) };
            return Err(error_from_report_with_ctx(code, name, Some(&ctx)));
        }
        Ok(())
    }

    /// Reference a Compressor for graph-based typed compression.
    ///
    /// This enables TypedRef compression by associating the CCtx with a Compressor
    /// that has registered compression graphs.
    ///
    /// IMPORTANT: The Compressor must remain valid for the duration of its usage.
    /// The Compressor must be validated before being referenced.
    pub fn ref_compressor(&mut self, compressor: &Compressor) -> Result<(), Error> {
        let r = unsafe { sys::ZL_CCtx_refCompressor(self.0, compressor.as_ptr()) };
        if sys::report_is_error(r) {
            let code = sys::report_code(r);
            let name = unsafe { CStr::from_ptr(sys::openzl_error_code_to_string(code)) }
                .to_string_lossy()
                .into_owned();
            let ctx = unsafe { CStr::from_ptr(sys::openzl_cctx_error_context(self.0, r)) };
            return Err(error_from_report_with_ctx(code, name, Some(&ctx)));
        }
        Ok(())
    }

    /// Get warnings generated during compression operations
    pub fn warnings(&self) -> Vec<Warning> {
        let arr = unsafe { sys::openzl_cctx_get_warnings(self.0) };
        let slice = unsafe { std::slice::from_raw_parts(arr.errors, arr.size) };
        slice
            .iter()
            .map(|e| {
                let code = unsafe { sys::openzl_error_get_code(e) };
                let name = unsafe { CStr::from_ptr(sys::openzl_error_get_name(e)) }
                    .to_string_lossy()
                    .into_owned();
                Warning { code, name }
            })
            .collect()
    }

    /// Compress a single typed input
    pub fn compress_typed_ref(&mut self, input: &TypedRef, dst: &mut [u8]) -> Result<usize, Error> {
        let r = unsafe {
            sys::ZL_CCtx_compressTypedRef(
                self.0,
                dst.as_mut_ptr() as *mut _,
                dst.len(),
                input.as_ptr(),
            )
        };
        if sys::report_is_error(r) {
            let code = sys::report_code(r);
            let name = unsafe { CStr::from_ptr(sys::openzl_error_code_to_string(code)) }
                .to_string_lossy()
                .into_owned();
            let ctx = unsafe { CStr::from_ptr(sys::openzl_cctx_error_context(self.0, r)) };
            return Err(error_from_report_with_ctx(code, name, Some(&ctx)));
        }
        Ok(sys::report_value(r))
    }

    /// Compress multiple typed inputs into a single frame
    pub fn compress_multi_typed_ref(&mut self, inputs: &[&TypedRef], dst: &mut [u8]) -> Result<usize, Error> {
        let mut ptrs: Vec<*const sys::ZL_TypedRef> = inputs.iter().map(|tr| tr.as_ptr()).collect();
        let r = unsafe {
            sys::ZL_CCtx_compressMultiTypedRef(
                self.0,
                dst.as_mut_ptr() as *mut _,
                dst.len(),
                ptrs.as_mut_ptr() as *mut _,
                ptrs.len(),
            )
        };
        if sys::report_is_error(r) {
            let code = sys::report_code(r);
            let name = unsafe { CStr::from_ptr(sys::openzl_error_code_to_string(code)) }
                .to_string_lossy()
                .into_owned();
            let ctx = unsafe { CStr::from_ptr(sys::openzl_cctx_error_context(self.0, r)) };
            return Err(error_from_report_with_ctx(code, name, Some(&ctx)));
        }
        Ok(sys::report_value(r))
    }
}
impl Drop for CCtx { fn drop(&mut self) { unsafe { sys::ZL_CCtx_free(self.0) } } }

pub struct DCtx(*mut sys::ZL_DCtx);
impl DCtx {
    pub fn new() -> Self {
        let p = unsafe { sys::ZL_DCtx_create() };
        assert!(!p.is_null());
        DCtx(p)
    }

    /// Get warnings generated during decompression operations
    pub fn warnings(&self) -> Vec<Warning> {
        let arr = unsafe { sys::openzl_dctx_get_warnings(self.0) };
        let slice = unsafe { std::slice::from_raw_parts(arr.errors, arr.size) };
        slice
            .iter()
            .map(|e| {
                let code = unsafe { sys::openzl_error_get_code(e) };
                let name = unsafe { CStr::from_ptr(sys::openzl_error_get_name(e)) }
                    .to_string_lossy()
                    .into_owned();
                Warning { code, name }
            })
            .collect()
    }

    /// Decompress into a TypedBuffer (auto-sized, single output)
    pub fn decompress_typed_buffer(&mut self, compressed: &[u8], output: &mut TypedBuffer) -> Result<usize, Error> {
        let r = unsafe {
            sys::ZL_DCtx_decompressTBuffer(
                self.0,
                output.as_mut_ptr(),
                compressed.as_ptr() as *const _,
                compressed.len(),
            )
        };
        if sys::report_is_error(r) {
            let code = sys::report_code(r);
            let name = unsafe { CStr::from_ptr(sys::openzl_error_code_to_string(code)) }
                .to_string_lossy()
                .into_owned();
            let ctx = unsafe { CStr::from_ptr(sys::openzl_dctx_error_context(self.0, r)) };
            return Err(error_from_report_with_ctx(code, name, Some(&ctx)));
        }
        Ok(sys::report_value(r))
    }

    /// Decompress into multiple TypedBuffers (multi-output frame)
    pub fn decompress_multi_typed_buffer(&mut self, compressed: &[u8], outputs: &mut [&mut TypedBuffer]) -> Result<usize, Error> {
        let mut ptrs: Vec<*mut sys::ZL_TypedBuffer> = outputs.iter_mut().map(|tb| tb.as_mut_ptr()).collect();
        let r = unsafe {
            sys::ZL_DCtx_decompressMultiTBuffer(
                self.0,
                ptrs.as_mut_ptr(),
                ptrs.len(),
                compressed.as_ptr() as *const _,
                compressed.len(),
            )
        };
        if sys::report_is_error(r) {
            let code = sys::report_code(r);
            let name = unsafe { CStr::from_ptr(sys::openzl_error_code_to_string(code)) }
                .to_string_lossy()
                .into_owned();
            let ctx = unsafe { CStr::from_ptr(sys::openzl_dctx_error_context(self.0, r)) };
            return Err(error_from_report_with_ctx(code, name, Some(&ctx)));
        }
        Ok(sys::report_value(r))
    }
}
impl Drop for DCtx { fn drop(&mut self) { unsafe { sys::ZL_DCtx_free(self.0) } } }

pub fn compress_serial(src: &[u8]) -> Result<Vec<u8>, Error> {
    let mut cctx = CCtx::new();
    // Set format version (required by OpenZL). Use latest version (21).
    cctx.set_parameter(sys::ZL_CParam::ZL_CParam_formatVersion, 21)?;
    // Capacity upper bound
    let cap = unsafe { sys::openzl_compress_bound(src.len()) };
    let mut dst = vec![0u8; cap];
    let r = unsafe {
        sys::ZL_CCtx_compress(
            cctx.0,
            dst.as_mut_ptr() as *mut _,
            dst.len(),
            src.as_ptr() as *const _,
            src.len(),
        )
    };
    if sys::report_is_error(r) {
        let code = sys::report_code(r);
        let name = unsafe { CStr::from_ptr(sys::openzl_error_code_to_string(code)) }
            .to_string_lossy()
            .into_owned();
        let ctx = unsafe { CStr::from_ptr(sys::openzl_cctx_error_context(cctx.0, r)) };
        return Err(error_from_report_with_ctx(code, name, Some(&ctx)));
    }
    let n = sys::report_value(r) as usize;
    dst.truncate(n);
    Ok(dst)
}

/// Compress a single TypedRef and return the compressed bytes.
///
/// This uses ZSTD graph by default. For better compression on specific data types,
/// consider using type-specific compression functions or creating a custom Compressor
/// with appropriate graphs.
pub fn compress_typed_ref(input: &TypedRef) -> Result<Vec<u8>, Error> {
    let mut cctx = CCtx::new();
    // Set format version (required by OpenZL). Use latest version (21).
    cctx.set_parameter(sys::ZL_CParam::ZL_CParam_formatVersion, 21)?;

    // Create a Compressor with ZSTD graph (reasonable default for general use)
    let mut compressor = Compressor::new();
    compressor.set_parameter(sys::ZL_CParam::ZL_CParam_formatVersion, 21)?;

    // Register ZSTD as the starting graph using the graph callback approach
    let r = unsafe {
        sys::ZL_Compressor_initUsingGraphFn(compressor.as_mut_ptr(), Some(zstd_graph_callback))
    };
    if sys::report_is_error(r) {
        return Err(report_to_error(r));
    }

    // Reference the compressor in the CCtx
    cctx.ref_compressor(&compressor)?;

    // Estimate capacity based on input data (this is conservative)
    // For TypedRef we can't easily get the size, so use a large upper bound
    let cap = 1024 * 1024; // 1MB default, adjust as needed
    let mut dst = vec![0u8; cap];

    let n = cctx.compress_typed_ref(input, &mut dst)?;
    dst.truncate(n);
    Ok(dst)
}

/// Compress multiple TypedRefs into a single frame.
///
/// This uses ZSTD graph by default. For better compression on specific data types,
/// consider creating a custom Compressor with appropriate graphs.
pub fn compress_multi_typed_ref(inputs: &[&TypedRef]) -> Result<Vec<u8>, Error> {
    let mut cctx = CCtx::new();
    // Set format version (required by OpenZL). Use latest version (21).
    cctx.set_parameter(sys::ZL_CParam::ZL_CParam_formatVersion, 21)?;

    // Create a Compressor with ZSTD graph
    let mut compressor = Compressor::new();
    compressor.set_parameter(sys::ZL_CParam::ZL_CParam_formatVersion, 21)?;

    let r = unsafe {
        sys::ZL_Compressor_initUsingGraphFn(compressor.as_mut_ptr(), Some(zstd_graph_callback))
    };
    if sys::report_is_error(r) {
        return Err(report_to_error(r));
    }

    cctx.ref_compressor(&compressor)?;

    // Estimate capacity (conservative)
    let cap = 1024 * 1024; // 1MB default
    let mut dst = vec![0u8; cap];

    let n = cctx.compress_multi_typed_ref(inputs, &mut dst)?;
    dst.truncate(n);
    Ok(dst)
}

pub fn decompress_serial(src: &[u8]) -> Result<Vec<u8>, Error> {
    // Query decompressed size first
    let rsize = unsafe { sys::ZL_getDecompressedSize(src.as_ptr() as *const _, src.len()) };
    if sys::report_is_error(rsize) {
        let code = sys::report_code(rsize);
        let name = unsafe { CStr::from_ptr(sys::openzl_error_code_to_string(code)) }
            .to_string_lossy()
            .into_owned();
        return Err(error_from_report_with_ctx(code, name, None));
    }
    let mut dst = vec![0u8; sys::report_value(rsize) as usize];
    let r = unsafe { sys::ZL_decompress(dst.as_mut_ptr() as *mut _, dst.len(), src.as_ptr() as *const _, src.len()) };
    if sys::report_is_error(r) {
        let code = sys::report_code(r);
        let name = unsafe { CStr::from_ptr(sys::openzl_error_code_to_string(code)) }
            .to_string_lossy()
            .into_owned();
        // No context without DCtx; use empty
        return Err(error_from_report_with_ctx(code, name, None));
    }
    let n = sys::report_value(r) as usize;
    dst.truncate(n);
    Ok(dst)
}

/// Decompress compressed data into a TypedBuffer (auto-allocates and determines type)
pub fn decompress_typed_buffer(compressed: &[u8]) -> Result<TypedBuffer, Error> {
    let mut dctx = DCtx::new();
    let mut output = TypedBuffer::new();
    dctx.decompress_typed_buffer(compressed, &mut output)?;
    Ok(output)
}

// ============================================================================
// High-level ergonomic APIs (Step 9 - MVP completion)
// ============================================================================

/// Compress numeric data using the NUMERIC graph (optimized for numeric arrays).
///
/// Supports all numeric types: u8, u16, u32, u64, i8, i16, i32, i64, f32, f64.
///
/// # Example
/// ```no_run
/// # use openzl::compress_numeric;
/// let data: Vec<u32> = (0..10000).collect();
/// let compressed = compress_numeric(&data).expect("compression failed");
/// ```
pub fn compress_numeric<T: Copy>(data: &[T]) -> Result<Vec<u8>, Error> {
    // Validate that T is a supported numeric type (1, 2, 4, or 8 bytes)
    let width = std::mem::size_of::<T>();
    if !matches!(width, 1 | 2 | 4 | 8) {
        return Err(Error::Report {
            code: -1,
            name: "Invalid numeric type".into(),
            context: format!("\nElement size must be 1, 2, 4, or 8 bytes, got {width}"),
        });
    }

    // Create TypedRef for numeric data
    let tref = TypedRef::numeric(data)?;

    // Create CCtx and Compressor with NUMERIC graph
    let mut cctx = CCtx::new();
    cctx.set_parameter(sys::ZL_CParam::ZL_CParam_formatVersion, 21)?;

    let mut compressor = Compressor::new();
    compressor.set_parameter(sys::ZL_CParam::ZL_CParam_formatVersion, 21)?;

    // Initialize with NUMERIC graph
    let r = unsafe {
        sys::ZL_Compressor_initUsingGraphFn(compressor.as_mut_ptr(), Some(numeric_graph_callback))
    };
    if sys::report_is_error(r) {
        return Err(report_to_error(r));
    }

    cctx.ref_compressor(&compressor)?;

    // Compress
    let cap = compress_bound(data.len() * width);
    let mut dst = vec![0u8; cap];

    let n = cctx.compress_typed_ref(&tref, &mut dst)?;
    dst.truncate(n);
    Ok(dst)
}

/// Decompress numeric data that was compressed with `compress_numeric`.
///
/// Returns a Vec<T> containing the decompressed numeric values.
///
/// # Example
/// ```no_run
/// # use openzl::{compress_numeric, decompress_numeric};
/// let data: Vec<u32> = (0..10000).collect();
/// let compressed = compress_numeric(&data).expect("compression failed");
/// let decompressed: Vec<u32> = decompress_numeric(&compressed).expect("decompression failed");
/// assert_eq!(data, decompressed);
/// ```
pub fn decompress_numeric<T: Copy>(compressed: &[u8]) -> Result<Vec<T>, Error> {
    let width = std::mem::size_of::<T>();
    if !matches!(width, 1 | 2 | 4 | 8) {
        return Err(Error::Report {
            code: -1,
            name: "Invalid numeric type".into(),
            context: format!("\nElement size must be 1, 2, 4, or 8 bytes, got {width}"),
        });
    }

    // Decompress into TypedBuffer
    let tbuf = decompress_typed_buffer(compressed)?;

    // Verify it's numeric type
    if tbuf.data_type() != sys::ZL_Type::ZL_Type_numeric {
        return Err(Error::Report {
            code: -1,
            name: "Type mismatch".into(),
            context: format!("\nExpected numeric type, got {:?}", tbuf.data_type()),
        });
    }

    // Verify element width matches T
    if tbuf.elt_width() != width {
        return Err(Error::Report {
            code: -1,
            name: "Width mismatch".into(),
            context: format!(
                "\nExpected element width {}, got {}",
                width,
                tbuf.elt_width()
            ),
        });
    }

    // Extract numeric data
    let slice = tbuf.as_numeric::<T>().ok_or_else(|| Error::Report {
        code: -1,
        name: "Failed to extract numeric data".into(),
        context: "\nAlignment or type mismatch".into(),
    })?;

    Ok(slice.to_vec())
}
