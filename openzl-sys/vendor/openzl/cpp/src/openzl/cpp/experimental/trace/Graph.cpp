// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "openzl/cpp/experimental/trace/Graph.hpp"

#include "openzl/common/a1cbor_helpers.h"
#include "openzl/cpp/experimental/trace/CborHelpers.hpp"
#include "openzl/shared/a1cbor.h"
#include "openzl/zl_errors.h"

namespace openzl::visualizer {

const ZL_Report Graph::serializeGraph(
        A1C_Arena* a1c_arena,
        A1C_Item* arrayItem,
        const ZL_CCtx* const cctx,
        const std::vector<size_t>& graphCodecs)
{
    A1C_MapBuilder builder = A1C_Item_map_builder(arrayItem, 5, a1c_arena);
    ZL_RET_R_IF_NULL(allocation, builder.map);

    ZL_RET_R_IF_ERR(addIntValue(builder, "gType", gType));
    ZL_RET_R_IF_ERR(addStringValue(builder, "gName", gName));
    if (ZL_isError(gFailure)) {
        ZL_RET_R_IF_ERR(addStringValue(
                builder,
                "gFailureString",
                ZL_CCtx_getErrorContextString(cctx, gFailure)));
    }

    // local params
    A1C_MAP_TRY_ADD_R(a1c_gLocalParams, builder);
    A1C_Item_string_refCStr(&a1c_gLocalParams->key, "gLocalParams");

    ZL_Report serializeLocalParamsReport = serializeLocalParams(
            a1c_arena, &a1c_gLocalParams->val, gLocalParams);
    ZL_RET_R_IF_ERR(serializeLocalParamsReport);

    // codec in graph
    A1C_MAP_TRY_ADD_R(a1c_codecIDs, builder);
    A1C_Item_string_refCStr(&a1c_codecIDs->key, "codecIDs");
    A1C_ArrayBuilder codecIDsBuilder = A1C_Item_array_builder(
            &a1c_codecIDs->val, graphCodecs.size(), a1c_arena);
    ZL_RET_R_IF_NULL(allocation, codecIDsBuilder.array);
    for (const auto& codecID : graphCodecs) {
        A1C_ARRAY_TRY_ADD_R(a1c_codecID, codecIDsBuilder);
        A1C_Item_int64(a1c_codecID, codecID);
    }

    return ZL_returnSuccess();
}

} // namespace openzl::visualizer
