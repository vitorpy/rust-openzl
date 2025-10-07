// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "openzl/codecs/zigzag/decode_zigzag_binding.h"
#include "openzl/codecs/zigzag/decode_zigzag_kernel.h" // ZS_deltaEncodeXX
#include "openzl/common/assertion.h"

// ZL_TypedEncoderFn, NUMPIPE
ZL_Report DI_zigzag_num(ZL_Decoder* dictx, const ZL_Input* ins[])
{
    ZL_ASSERT_NN(dictx);
    ZL_ASSERT_NN(ins);
    const ZL_Input* const in = ins[0];
    ZL_ASSERT_NN(in);
    ZL_ASSERT_EQ(ZL_Input_type(in), ZL_Type_numeric);
    size_t const numWidth = ZL_Input_eltWidth(in);
    size_t const nbInts   = ZL_Input_numElts(in);
    ZL_Output* const out = ZL_Decoder_create1OutStream(dictx, nbInts, numWidth);
    ZL_RET_R_IF_NULL(allocation, out);
    ZL_ASSERT(numWidth == 1 || numWidth == 2 || numWidth == 4 || numWidth == 8);
    switch (numWidth) {
        case 1:
            ZL_zigzagDecode8(ZL_Output_ptr(out), ZL_Input_ptr(in), nbInts);
            break;
        case 2:
            ZL_zigzagDecode16(ZL_Output_ptr(out), ZL_Input_ptr(in), nbInts);
            break;
        case 4:
            ZL_zigzagDecode32(ZL_Output_ptr(out), ZL_Input_ptr(in), nbInts);
            break;
        case 8:
            ZL_zigzagDecode64(ZL_Output_ptr(out), ZL_Input_ptr(in), nbInts);
            break;
        default:
            ZL_ASSERT_FAIL("Unreachable");
    }
    ZL_RET_R_IF_ERR(ZL_Output_commit(out, nbInts));
    return ZL_returnValue(1);
}
