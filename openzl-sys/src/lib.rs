#![allow(non_camel_case_types, non_snake_case, non_upper_case_globals, clippy::all)]

pub use libc;

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

extern "C" {
    pub fn openzl_report_is_error(r: ZL_Report) -> ::std::os::raw::c_int;
    pub fn openzl_report_value(r: ZL_Report) -> usize;
    pub fn openzl_report_code(r: ZL_Report) -> ::std::os::raw::c_int;
    pub fn openzl_error_code_to_string(code: ::std::os::raw::c_int) -> *const ::std::os::raw::c_char;
    pub fn openzl_cctx_error_context(cctx: *const ZL_CCtx, r: ZL_Report) -> *const ::std::os::raw::c_char;
    pub fn openzl_dctx_error_context(dctx: *const ZL_DCtx, r: ZL_Report) -> *const ::std::os::raw::c_char;
    pub fn openzl_compress_bound(totalSrcSize: usize) -> usize;
    pub fn openzl_cctx_get_warnings(cctx: *const ZL_CCtx) -> ZL_Error_Array;
    pub fn openzl_dctx_get_warnings(dctx: *const ZL_DCtx) -> ZL_Error_Array;
    pub fn openzl_error_get_code(err: *const ZL_Error) -> ::std::os::raw::c_int;
    pub fn openzl_error_get_name(err: *const ZL_Error) -> *const ::std::os::raw::c_char;
}

#[inline]
pub fn report_is_error(r: ZL_Report) -> bool { unsafe { openzl_report_is_error(r) != 0 } }
#[inline]
pub fn report_value(r: ZL_Report) -> usize { unsafe { openzl_report_value(r) } }
#[inline]
pub fn report_code(r: ZL_Report) -> i32 { unsafe { openzl_report_code(r) as i32 } }
