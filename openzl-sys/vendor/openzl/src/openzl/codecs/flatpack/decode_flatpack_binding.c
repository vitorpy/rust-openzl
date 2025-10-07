// Copyright (c) Meta Platforms, Inc. and affiliates.
#include "openzl/codecs/flatpack/decode_flatpack_binding.h"

#include "openzl/codecs/flatpack/decode_flatpack_kernel.h"
#include "openzl/zl_dtransform.h"

ZL_Report DI_flatpack(ZL_Decoder* dictx, const ZL_Input* ins[])
{
    ZL_Input const* const alphabet = ins[0];
    ZL_Input const* const packed   = ins[1];

    ZL_ASSERT_EQ(ZL_Input_type(alphabet), ZL_Type_serial);
    ZL_ASSERT_EQ(ZL_Input_type(packed), ZL_Type_serial);

    size_t const alphabetSize = ZL_Input_numElts(alphabet);
    size_t const packedSize   = ZL_Input_numElts(packed);

    ZL_RET_R_IF_GT(corruption, alphabetSize, 256, "Alphabet too large!");

    size_t const nbElts = ZS_FlatPack_nbElts(
            alphabetSize, (uint8_t const*)ZL_Input_ptr(packed), packedSize);

    ZL_Output* const out = ZL_Decoder_create1OutStream(dictx, nbElts, 1);
    ZL_RET_R_IF_NULL(allocation, out);

    ZS_FlatPackSize const size = ZS_flatpackDecode(
            (uint8_t*)ZL_Output_ptr(out),
            nbElts,
            (uint8_t const*)ZL_Input_ptr(alphabet),
            alphabetSize,
            (uint8_t const*)ZL_Input_ptr(packed),
            packedSize);
    ZL_RET_R_IF(
            corruption, ZS_FlatPack_isError(size), "Flatpack decoding failed!");

    ZL_RET_R_IF_ERR(ZL_Output_commit(out, nbElts));

    return ZL_returnValue(1);
}
