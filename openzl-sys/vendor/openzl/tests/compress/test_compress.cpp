// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <string>

#include <gtest/gtest.h>

#include "openzl/common/assertion.h"
#include "openzl/common/errors_internal.h"
#include "openzl/zl_compress.h"
#include "openzl/zl_compressor.h"
#include "openzl/zl_public_nodes.h"

using namespace ::testing;

namespace {

class CompressTest : public Test {
   protected:
    size_t compress(
            void* dst,
            size_t dstCapacity,
            void const* src,
            size_t srcSize,
            ZL_GraphID graph)
    {
        auto cctx = ZL_CCtx_create();

        ZL_REQUIRE_SUCCESS(ZL_CCtx_setParameter(
                cctx, ZL_CParam_formatVersion, ZL_MAX_FORMAT_VERSION));
        ZL_REQUIRE_SUCCESS(
                ZL_CCtx_selectStartingGraphID(cctx, NULL, graph, NULL));

        auto const report =
                ZL_CCtx_compress(cctx, dst, dstCapacity, src, srcSize);
        ZL_REQUIRE_SUCCESS(report);

        ZL_CCtx_free(cctx);

        return ZL_validResult(report);
    }
};

TEST_F(CompressTest, CompressionSucceedsWithSmallDstBuffer)
{
    std::string data(1000, 'a');
    std::string dst(100, '\0');

    auto const cSize0 = compress(
            dst.data(),
            dst.size(),
            data.data(),
            data.size(),
            ZL_GRAPH_CONSTANT);
    auto const cSize1 = compress(
            dst.data(), cSize0, data.data(), data.size(), ZL_GRAPH_CONSTANT);
    ASSERT_EQ(cSize0, cSize1);
}
} // namespace
