#include <stddef.h>
#include "openzl/openzl.h"

int openzl_report_is_error(ZL_Report r) { return ZL_isError(r); }
size_t openzl_report_value(ZL_Report r) { return ZL_validResult(r); }
int openzl_report_code(ZL_Report r) { return (int)ZL_errorCode(r); }

const char* openzl_error_code_to_string(int code) {
    return ZL_ErrorCode_toString((ZL_ErrorCode)code);
}

const char* openzl_cctx_error_context(const ZL_CCtx* cctx, ZL_Report r) {
    return ZL_CCtx_getErrorContextString(cctx, r);
}

const char* openzl_dctx_error_context(const ZL_DCtx* dctx, ZL_Report r) {
    return ZL_DCtx_getErrorContextString(dctx, r);
}

// Wrapper for inline functions
size_t openzl_compress_bound(size_t totalSrcSize) {
    return ZL_compressBound(totalSrcSize);
}

// Warning accessors
ZL_Error_Array openzl_cctx_get_warnings(const ZL_CCtx* cctx) {
    return ZL_CCtx_getWarnings(cctx);
}

ZL_Error_Array openzl_dctx_get_warnings(const ZL_DCtx* dctx) {
    return ZL_DCtx_getWarnings(dctx);
}

// Extract error code from ZL_Error
int openzl_error_get_code(const ZL_Error* err) {
    return (int)err->_code;
}

// Get error description string (combines code + message if available)
const char* openzl_error_get_name(const ZL_Error* err) {
    return ZL_ErrorCode_toString(err->_code);
}
