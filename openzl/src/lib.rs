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

/// OpenZL warning (non-fatal issue during compression/decompression)
#[derive(Debug, Clone)]
pub struct Warning {
    pub code: i32,
    pub name: String,
}

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

/// Compress a single TypedRef and return the compressed bytes
pub fn compress_typed_ref(input: &TypedRef) -> Result<Vec<u8>, Error> {
    let mut cctx = CCtx::new();
    // Set format version (required by OpenZL). Use latest version (21).
    cctx.set_parameter(sys::ZL_CParam::ZL_CParam_formatVersion, 21)?;

    // Estimate capacity based on input data (this is conservative)
    // For TypedRef we can't easily get the size, so use a large upper bound
    let cap = 1024 * 1024; // 1MB default, adjust as needed
    let mut dst = vec![0u8; cap];

    let n = cctx.compress_typed_ref(input, &mut dst)?;
    dst.truncate(n);
    Ok(dst)
}

/// Compress multiple TypedRefs into a single frame
pub fn compress_multi_typed_ref(inputs: &[&TypedRef]) -> Result<Vec<u8>, Error> {
    let mut cctx = CCtx::new();
    // Set format version (required by OpenZL). Use latest version (21).
    cctx.set_parameter(sys::ZL_CParam::ZL_CParam_formatVersion, 21)?;

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
