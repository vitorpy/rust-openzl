// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "openzl/cpp/experimental/trace/Codec.hpp"

#include "openzl/common/a1cbor_helpers.h"
#include "openzl/cpp/experimental/trace/CborHelpers.hpp"
#include "openzl/shared/a1cbor.h"
#include "openzl/zl_compress.h"
#include "openzl/zl_errors.h"

namespace openzl::visualizer {

static ZL_Report serializeCodecEdges(
        A1C_Arena* a1c_arena,
        A1C_Item* a1c_edgesParent,
        const std::vector<ZL_DataID>& edges)
{
    A1C_ArrayBuilder inEdgesBuilder =
            A1C_Item_array_builder(a1c_edgesParent, edges.size(), a1c_arena);
    ZL_RET_R_IF_NULL(allocation, inEdgesBuilder.array);
    for (const auto& streamID : edges) {
        A1C_ARRAY_TRY_ADD_R(a1c_streamID, inEdgesBuilder);
        A1C_Item_int64(a1c_streamID, streamID.sid);
    }

    return ZL_returnSuccess();
}

const ZL_Report Codec::serializeCodec(
        A1C_Arena* a1c_arena,
        A1C_Item* arrayItem,
        const ZL_CCtx* const cctx,
        const std::vector<ZL_DataID>& inEdges,
        const std::vector<ZL_DataID>& outEdges)
{
    A1C_MapBuilder builder = A1C_Item_map_builder(arrayItem, 8, a1c_arena);
    ZL_RET_R_IF_NULL(allocation, builder.map);

    ZL_RET_R_IF_ERR(addStringValue(builder, "name", name.c_str()));
    ZL_RET_R_IF_ERR(addBooleanValue(builder, "cType", cType));
    ZL_RET_R_IF_ERR(addIntValue(builder, "cID", cID));
    ZL_RET_R_IF_ERR(addIntValue(builder, "cHeaderSize", cHeaderSize));
    if (ZL_isError(cFailure)) {
        ZL_RET_R_IF_ERR(addStringValue(
                builder,
                "cFailureString",
                ZL_CCtx_getErrorContextString(cctx, cFailure)));
    }

    // local params
    A1C_MAP_TRY_ADD_R(a1c_cLocalParams, builder);
    A1C_Item_string_refCStr(&a1c_cLocalParams->key, "cLocalParams");
    ZL_RET_R_IF_ERR(serializeLocalParams(
            a1c_arena, &a1c_cLocalParams->val, cLocalParams));

    // codec in-edges
    A1C_MAP_TRY_ADD_R(a1c_inEdges, builder);
    A1C_Item_string_refCStr(&a1c_inEdges->key, "inputStreams");
    ZL_RET_R_IF_ERR(serializeCodecEdges(a1c_arena, &a1c_inEdges->val, inEdges));

    // codec out-edges
    A1C_MAP_TRY_ADD_R(a1c_outEdges, builder);
    A1C_Item_string_refCStr(&a1c_outEdges->key, "outputStreams");
    ZL_RET_R_IF_ERR(
            serializeCodecEdges(a1c_arena, &a1c_outEdges->val, outEdges));

    return ZL_returnSuccess();
}

} // namespace openzl::visualizer
