// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "openzl/cpp/experimental/trace/StreamVisualizer.hpp"

#include "openzl/cpp/experimental/trace/CborHelpers.hpp"
#include "openzl/shared/a1cbor.h"
#include "openzl/zl_errors.h"

namespace openzl::visualizer {
const ZL_Report Stream::serializeStream(
        A1C_Arena* a1c_arena,
        A1C_Item* arrayItem)
{
    A1C_MapBuilder builder = A1C_Item_map_builder(arrayItem, 7, a1c_arena);
    ZL_RET_R_IF_NULL(allocation, builder.map);

    ZL_RET_R_IF_ERR(addIntValue(builder, "type", type));
    ZL_RET_R_IF_ERR(addIntValue(builder, "outputIdx", outputIdx));
    ZL_RET_R_IF_ERR(addIntValue(builder, "eltWidth", eltWidth));
    ZL_RET_R_IF_ERR(addIntValue(builder, "numElts", numElts));
    ZL_RET_R_IF_ERR(addIntValue(builder, "cSize", cSize));
    ZL_RET_R_IF_ERR(addFloatValue(builder, "share", share));
    ZL_RET_R_IF_ERR(addIntValue(builder, "contentSize", contentSize));

    return ZL_returnSuccess();
}
} // namespace openzl::visualizer
