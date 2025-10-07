// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <gtest/gtest.h>

#include "openzl/common/scope_context.h"

using namespace ::testing;

TEST(ScopeContextTest, GetScopeContextCCtx)
{
    // Trailing underscore to break the magic cctx variable context
    ZL_CCtx* cctx_ = ZL_CCtx_create();
    EXPECT_EQ(ZL_GET_SCOPE_CONTEXT(), nullptr);
    {
        ZL_CCtx* cctx = cctx_;
        EXPECT_NE(ZL_GET_SCOPE_CONTEXT(), nullptr);
        EXPECT_NE(ZL_GET_SCOPE_CONTEXT()->opCtx, nullptr);
    }
    {
        ZL_SCOPE_CONTEXT(cctx_);
        EXPECT_NE(ZL_GET_SCOPE_CONTEXT(), nullptr);
        EXPECT_NE(ZL_GET_SCOPE_CONTEXT()->opCtx, nullptr);
    }
    EXPECT_EQ(ZL_GET_SCOPE_CONTEXT(), nullptr);
    {
        ZL_SCOPE_GRAPH_CONTEXT(cctx_, { .transformID = 5 });
        EXPECT_NE(ZL_GET_SCOPE_CONTEXT(), nullptr);
        EXPECT_NE(ZL_GET_SCOPE_CONTEXT()->opCtx, nullptr);
        EXPECT_EQ(ZL_GET_SCOPE_CONTEXT()->graphCtx.transformID, 5u);
    }
    EXPECT_EQ(ZL_GET_SCOPE_CONTEXT(), nullptr);
    ZL_CCtx_free(cctx_);
}

TEST(ScopeContextTest, GetScopeContextCGraph)
{
    // Trailing underscore to break the magic cgraph variable context
    ZL_Compressor* cgraph_ = ZL_Compressor_create();
    EXPECT_EQ(ZL_GET_SCOPE_CONTEXT(), nullptr);
    {
        ZL_Compressor* cgraph = cgraph_;
        EXPECT_NE(ZL_GET_SCOPE_CONTEXT(), nullptr);
        EXPECT_NE(ZL_GET_SCOPE_CONTEXT()->opCtx, nullptr);
    }
    {
        ZL_SCOPE_CONTEXT(cgraph_);
        EXPECT_NE(ZL_GET_SCOPE_CONTEXT(), nullptr);
        EXPECT_NE(ZL_GET_SCOPE_CONTEXT()->opCtx, nullptr);
    }
    ZL_Compressor_free(cgraph_);
}

TEST(ScopeContextTest, GetScopeContextDCtx)
{
    // Trailing underscore to break the magic dctx variable context
    ZL_DCtx* dctx_ = ZL_DCtx_create();
    EXPECT_EQ(ZL_GET_SCOPE_CONTEXT(), nullptr);
    {
        ZL_DCtx* dctx = dctx_;
        EXPECT_NE(ZL_GET_SCOPE_CONTEXT(), nullptr);
        EXPECT_NE(ZL_GET_SCOPE_CONTEXT()->opCtx, nullptr);
    }
    {
        ZL_SCOPE_CONTEXT(dctx_);
        EXPECT_NE(ZL_GET_SCOPE_CONTEXT(), nullptr);
        EXPECT_NE(ZL_GET_SCOPE_CONTEXT()->opCtx, nullptr);
    }
    ZL_DCtx_free(dctx_);
}
