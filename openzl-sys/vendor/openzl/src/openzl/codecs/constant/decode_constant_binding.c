// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "openzl/codecs/constant/decode_constant_binding.h"
#include "openzl/codecs/constant/decode_constant_kernel.h"

#include "openzl/common/assertion.h"
#include "openzl/shared/varint.h"
#include "openzl/zl_data.h"

ZL_Report DI_constant_typed(ZL_Decoder* dictx, const ZL_Input* ins[])
{
    ZL_ASSERT_NN(dictx);
    ZL_ASSERT_NN(ins);
    const ZL_Input* const in = ins[0];
    ZL_ASSERT_NN(in);
    ZL_ASSERT(
            ZL_Input_type(in) == ZL_Type_serial
            || ZL_Input_type(in) == ZL_Type_struct);

    const uint8_t* const src = ZL_Input_ptr(in);
    size_t const nbElts      = ZL_Input_numElts(in);
    size_t const eltWidth    = ZL_Input_eltWidth(in);
    ZL_ASSERT_NN(src);
    ZL_ASSERT_GE(eltWidth, 1);
    ZL_RET_R_IF_NE(corruption, nbElts, 1);

    ZL_RBuffer const header = ZL_Decoder_getCodecHeader(dictx);
    uint8_t const* hdrStart = (uint8_t const*)header.start;
    uint8_t const* hdrEnd   = hdrStart + header.size;
    ZL_TRY_LET_T(uint64_t, dstNbElts, ZL_varintDecode(&hdrStart, hdrEnd));
    ZL_RET_R_IF_NE(corruption, hdrStart, hdrEnd);
    ZL_RET_R_IF_LT(corruption, dstNbElts, 1);

    ZL_Output* const out =
            ZL_Decoder_create1OutStream(dictx, dstNbElts, eltWidth);
    ZL_RET_R_IF_NULL(allocation, out);
    uint8_t* const outPtr = (uint8_t*)ZL_Output_ptr(out);
    ZL_ASSERT_NN(outPtr);

    size_t const bufferSize = (eltWidth <= 32) ? 32 : eltWidth;
    void* const eltBuffer   = ZL_Decoder_getScratchSpace(dictx, bufferSize);
    ZL_RET_R_IF_NULL(allocation, eltBuffer);
    ZS_decodeConstant(outPtr, dstNbElts, src, eltWidth, eltBuffer);
    ZL_RET_R_IF_ERR(ZL_Output_commit(out, dstNbElts));
    return ZL_returnSuccess();
}
