// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <gtest/gtest.h>

#include "openzl/common/assertion.h"
#include "openzl/common/errors_internal.h"
#include "openzl/compress/private_nodes.h"
#include "openzl/zl_compress.h"
#include "openzl/zl_compressor.h"
#include "openzl/zl_graph_api.h"
#include "openzl/zl_reflection.h"
#include "openzl/zl_selector.h"

#include "tests/utils.h"

using namespace ::testing;

class CompressorTest : public ::testing::Test {
   public:
    void SetUp() override
    {
        compressor_ = ZL_Compressor_create();

        intParam_.paramId    = 1;
        intParam_.paramValue = 100;

        copyParam_.paramId   = 10;
        copyParam_.paramPtr  = "hello";
        copyParam_.paramSize = 6;

        refParam_.paramId  = 5;
        refParam_.paramRef = "world";

        localParams_.intParams.intParams   = &intParam_;
        localParams_.intParams.nbIntParams = 1;

        localParams_.copyParams.copyParams   = &copyParam_;
        localParams_.copyParams.nbCopyParams = 1;

        localParams_.refParams.refParams   = &refParam_;
        localParams_.refParams.nbRefParams = 1;
    }

    void TearDown() override
    {
        ZL_Compressor_free(compressor_);
        compressor_ = nullptr;
    }

    void setParameter(ZL_CParam param, int value)
    {
        ZL_REQUIRE_SUCCESS(
                ZL_Compressor_setParameter(compressor_, param, value));
    }

    int getParameter(ZL_CParam param)
    {
        return ZL_Compressor_getParameter(compressor_, param);
    }

    std::unordered_map<ZL_CParam, int> getParameters()
    {
        using Params = std::unordered_map<ZL_CParam, int>;
        Params params;
        ZL_REQUIRE_SUCCESS(ZL_Compressor_forEachParam(
                compressor_,
                [](void* opaque, ZL_CParam param, int value) noexcept {
                    Params* paramsPtr    = (Params*)opaque;
                    auto [it, inserteed] = paramsPtr->emplace(param, value);
                    ZL_REQUIRE(inserteed);
                    return ZL_returnSuccess();
                },
                &params));
        return params;
    }

    std::vector<ZL_GraphID> getGraphs()
    {
        std::vector<ZL_GraphID> graphs;
        ZL_REQUIRE_SUCCESS(ZL_Compressor_forEachGraph(
                compressor_,
                [](void* opaque,
                   const ZL_Compressor* compressor,
                   ZL_GraphID graph) noexcept {
                    auto* graphsPtr = (std::vector<ZL_GraphID>*)opaque;
                    graphsPtr->push_back(graph);
                    return ZL_returnSuccess();
                },
                &graphs));
        return graphs;
    }

    std::vector<ZL_NodeID> getNodes()
    {
        std::vector<ZL_NodeID> nodes;
        ZL_REQUIRE_SUCCESS(ZL_Compressor_forEachNode(
                compressor_,
                [](void* opaque,
                   const ZL_Compressor* compressor,
                   ZL_NodeID node) noexcept {
                    auto* nodesPtr = (std::vector<ZL_NodeID>*)opaque;
                    nodesPtr->push_back(node);
                    return ZL_returnSuccess();
                },
                &nodes));
        return nodes;
    }

    void expectParamsEmpty(const ZL_LocalParams& params)
    {
        ZL_LocalParams empty{};
        EXPECT_EQ(memcmp(&params, &empty, sizeof(empty)), 0);
    }

    void expectParamsEq(const ZL_LocalParams& lhs, const ZL_LocalParams& rhs)
    {
        ASSERT_EQ(lhs.intParams.nbIntParams, rhs.intParams.nbIntParams);
        for (size_t i = 0; i < lhs.intParams.nbIntParams; ++i) {
            ASSERT_EQ(
                    0,
                    memcmp(&lhs.intParams.intParams[i],
                           &rhs.intParams.intParams[i],
                           sizeof(ZL_IntParam)));
        }

        ASSERT_EQ(lhs.copyParams.nbCopyParams, rhs.copyParams.nbCopyParams);
        for (size_t i = 0; i < lhs.copyParams.nbCopyParams; ++i) {
            ASSERT_EQ(
                    lhs.copyParams.copyParams[i].paramId,
                    rhs.copyParams.copyParams[i].paramId);
            ASSERT_EQ(
                    lhs.copyParams.copyParams[i].paramSize,
                    rhs.copyParams.copyParams[i].paramSize);
            ASSERT_EQ(
                    0,
                    memcmp(lhs.copyParams.copyParams[i].paramPtr,
                           rhs.copyParams.copyParams[i].paramPtr,
                           lhs.copyParams.copyParams[i].paramSize));
        }

        ASSERT_EQ(lhs.refParams.nbRefParams, rhs.refParams.nbRefParams);
        for (size_t i = 0; i < lhs.refParams.nbRefParams; ++i) {
            ASSERT_EQ(
                    0,
                    memcmp(&lhs.refParams.refParams[i],
                           &rhs.refParams.refParams[i],
                           sizeof(ZL_RefParam)));
        }
    }

    ZL_GraphID makeStaticGraph(bool isAnchor = false)
    {
        std::array<ZL_GraphID, 2> successors = { ZL_GRAPH_FIELD_LZ,
                                                 ZL_GRAPH_ZSTD };
        ZL_StaticGraphDesc desc              = {
                         .name           = isAnchor ? "!static" : "static",
                         .headNodeid     = ZL_NODE_FLOAT16_DECONSTRUCT,
                         .successor_gids = successors.data(),
                         .nbGids         = successors.size(),
                         .localParams    = &localParams_,
        };
        return ZL_Compressor_registerStaticGraph(compressor_, &desc);
    }

    ZL_GraphID makeSelectorGraph(bool isAnchor = false)
    {
        std::array<ZL_GraphID, 2> graphs = { ZL_GRAPH_FIELD_LZ_LITERALS,
                                             ZL_GRAPH_STORE };
        ZL_SelectorDesc desc             = {
                        .selector_f = [](const ZL_Selector*,
                             const ZL_Input*,
                             const ZL_GraphID* customGraphs,
                             size_t) noexcept { return customGraphs[0]; },
            .inStreamType                = (ZL_Type)(ZL_Type_struct | ZL_Type_numeric),
            .customGraphs                = graphs.data(),
            .nbCustomGraphs              = graphs.size(),
            .localParams                 = localParams_,
            .name                        = isAnchor ? "!selector" : "selector",
        };
        return ZL_Compressor_registerSelectorGraph(compressor_, &desc);
    }

    ZL_GraphID makeDynamicGraph(bool isAnchor = false)
    {
        ZL_GraphID successor           = ZL_GRAPH_COMPRESS_GENERIC;
        std::array<ZL_NodeID, 2> nodes = { ZL_NODE_ZSTD, ZL_NODE_FIELD_LZ };
        ZL_Type inputType              = ZL_Type_serial;
        ZL_FunctionGraphDesc desc      = {
                 .name    = isAnchor ? "!dynamic" : "dynamic",
                 .graph_f = [](ZL_Graph* gctx,
                          ZL_Edge* inputs[],
                          size_t nbIns) noexcept { return ZL_returnSuccess(); },
            .inputTypeMasks            = &inputType,
            .nbInputs                  = 1,
            .lastInputIsVariable       = false,
            .customGraphs              = &successor,
            .nbCustomGraphs            = 1,
            .customNodes               = nodes.data(),
            .nbCustomNodes             = nodes.size(),
            .localParams               = localParams_,
        };
        auto graph = ZL_Compressor_registerFunctionGraph(compressor_, &desc);
        EXPECT_NE(graph.gid, ZL_GRAPH_ILLEGAL.gid);
        return graph;
    }

    ZL_GraphID makeMultiInputGraph(
            bool variableInput = true,
            bool isAnchor      = false)
    {
        std::array<ZL_Type, 2> inputs{ ZL_Type_serial, ZL_Type_numeric };
        ZL_GraphID successor      = ZL_GRAPH_COMPRESS_GENERIC;
        ZL_NodeID node            = ZL_NODE_ZSTD;
        ZL_FunctionGraphDesc desc = {
            .name = isAnchor ? "!multi_input" : "multi_input",
            .graph_f =
                    [](ZL_Graph* gctx,
                       ZL_Edge* input[],
                       size_t nbInputs) noexcept { return ZL_returnSuccess(); },
            .inputTypeMasks       = inputs.data(),
            .nbInputs             = inputs.size(),
            .lastInputIsVariable  = variableInput,
            .customGraphs         = &successor,
            .nbCustomGraphs       = 1,
            .customNodes          = &node,
            .nbCustomNodes        = 1,
            .localParams          = localParams_,
        };
        auto graph = ZL_Compressor_registerFunctionGraph(compressor_, &desc);
        EXPECT_NE(graph, ZL_GRAPH_ILLEGAL);
        return graph;
    }

    ZL_GraphID makeParameterizedGraph(
            bool hasName  = false,
            bool isAnchor = false)
    {
        const char* name = nullptr;
        if (hasName) {
            name = isAnchor ? "!parameterized" : "parameterized";
        }
        ZL_ParameterizedGraphDesc desc = {
            .name        = name,
            .graph       = ZL_GRAPH_FIELD_LZ,
            .localParams = &localParams_,
        };
        return ZL_Compressor_registerParameterizedGraph(compressor_, &desc);
    }

    ZL_NodeID makeCustomTransform(bool isAnchor = false)
    {
        const ZL_Type outType = ZL_Type_serial;
        ZL_TypedEncoderDesc desc = {
            .gd = {
                .CTid = isAnchor ? 0u : 1u,
                .inStreamType = ZL_Type_serial,
                .outStreamTypes = &outType,
                .nbOutStreams = 1,
            },
            .transform_f =
                    [](ZL_Encoder*, const ZL_Input*) noexcept {
                        return ZL_returnSuccess();
                    },
            .name = isAnchor ? "!custom_transform" : "custom_transform",
        };
        return ZL_Compressor_registerTypedEncoder(compressor_, &desc);
    }

   protected:
    ZL_Compressor* compressor_ = nullptr;
    ZL_LocalParams localParams_;
    ZL_IntParam intParam_;
    ZL_CopyParam copyParam_;
    ZL_RefParam refParam_;
};

TEST_F(CompressorTest, registerStaticGraph_registerWithSameName)
{
    // Illegal to register two graphs with the same anchor name
    auto graph = makeStaticGraph(true);
    ASSERT_NE(graph, ZL_GRAPH_ILLEGAL);
    auto graph2 = makeStaticGraph(true);
    ASSERT_EQ(graph2, ZL_GRAPH_ILLEGAL);
    // Allowed to register two non-anchors with same name
    auto graph3 = makeStaticGraph(false);
    ASSERT_NE(graph3, ZL_GRAPH_ILLEGAL);
    ASSERT_NE(graph3, graph);
    auto graph4 = makeStaticGraph(false);
    ASSERT_NE(graph4, ZL_GRAPH_ILLEGAL);
    ASSERT_NE(graph4, graph);
}

TEST_F(CompressorTest, registerStaticGraph_emptyName)
{
    auto graph = ZL_Compressor_registerStaticGraph_fromNode1o(
            compressor_, ZL_NODE_DELTA_INT, ZL_GRAPH_ZSTD);
    ASSERT_EQ(
            std::string("zl.delta_int#0"),
            ZL_Compressor_Graph_getName(compressor_, graph));
    ZL_StaticGraphDesc desc = {
        .name           = NULL,
        .headNodeid     = ZL_NODE_ZIGZAG,
        .successor_gids = &graph,
        .nbGids         = 1,
        .localParams    = &localParams_,
    };
    graph = ZL_Compressor_registerStaticGraph(compressor_, &desc);
    ASSERT_EQ(
            std::string("#1"), ZL_Compressor_Graph_getName(compressor_, graph));
    desc.name = "";
    graph     = ZL_Compressor_registerStaticGraph(compressor_, &desc);
    ASSERT_EQ(
            std::string("#2"), ZL_Compressor_Graph_getName(compressor_, graph));
    desc.name = "!";
    graph     = ZL_Compressor_registerStaticGraph(compressor_, &desc);
    ASSERT_EQ(std::string(""), ZL_Compressor_Graph_getName(compressor_, graph));

    graph = ZL_Compressor_getGraph(compressor_, "zl.delta_int#0");

    ZL_ParameterizedGraphDesc paramDesc = {
        .graph = graph,
    };
    auto paramGraph =
            ZL_Compressor_registerParameterizedGraph(compressor_, &paramDesc);
    ASSERT_NE(paramGraph, ZL_GRAPH_ILLEGAL);
    ASSERT_NE(paramGraph, graph);

    ASSERT_EQ(
            std::string("zl.delta_int#4"),
            ZL_Compressor_Graph_getName(compressor_, paramGraph));
}

TEST_F(CompressorTest, registerParameterizedGraph_name)
{
    auto graph                     = ZL_GRAPH_FIELD_LZ;
    ZL_ParameterizedGraphDesc desc = {
        .graph = graph,
    };
    graph = ZL_Compressor_registerParameterizedGraph(compressor_, &desc);
    ASSERT_EQ(
            std::string("zl.field_lz#0"),
            ZL_Compressor_Graph_getName(compressor_, graph));
    desc.graph = graph;
    graph      = ZL_Compressor_registerParameterizedGraph(compressor_, &desc);
    ASSERT_EQ(
            std::string("zl.field_lz#1"),
            ZL_Compressor_Graph_getName(compressor_, graph));

    desc.graph = graph;
    desc.name  = "parameterized";
    graph      = ZL_Compressor_registerParameterizedGraph(compressor_, &desc);
    ASSERT_EQ(
            std::string("parameterized#2"),
            ZL_Compressor_Graph_getName(compressor_, graph));

    desc.graph = graph;
    desc.name  = "!parameterized";
    graph      = ZL_Compressor_registerParameterizedGraph(compressor_, &desc);
    ASSERT_EQ(
            std::string("parameterized"),
            ZL_Compressor_Graph_getName(compressor_, graph));

    desc.graph = graph;
    desc.name  = "parameterized";
    graph      = ZL_Compressor_registerParameterizedGraph(compressor_, &desc);
    ASSERT_EQ(
            std::string("parameterized#4"),
            ZL_Compressor_Graph_getName(compressor_, graph));
}

TEST_F(CompressorTest, registerParameterizedGraph_localParams)
{
    auto graph = ZL_Compressor_registerStaticGraph_fromNode1o(
            compressor_, ZL_NODE_DELTA_INT, ZL_GRAPH_ZSTD);
    ZL_ParameterizedGraphDesc desc = {
        .graph = graph,
    };
    auto noParam = ZL_Compressor_registerParameterizedGraph(compressor_, &desc);
    desc.localParams = &localParams_;
    auto params = ZL_Compressor_registerParameterizedGraph(compressor_, &desc);
    expectParamsEq(
            ZL_Compressor_Graph_getLocalParams(compressor_, graph),
            ZL_Compressor_Graph_getLocalParams(compressor_, noParam));
    expectParamsEq(
            localParams_,
            ZL_Compressor_Graph_getLocalParams(compressor_, params));
}

TEST_F(CompressorTest, registerParameterizedGraph_customGraphs)
{
    auto graph = ZL_Compressor_registerStaticGraph_fromNode1o(
            compressor_, ZL_NODE_DELTA_INT, ZL_GRAPH_ZSTD);
    ZL_ParameterizedGraphDesc desc = {
        .graph          = graph,
        .customGraphs   = &graph,
        .nbCustomGraphs = 1,
    };
    auto graphs = ZL_Compressor_registerParameterizedGraph(compressor_, &desc);
    auto customGraphs =
            ZL_Compressor_Graph_getCustomGraphs(compressor_, graphs);
    ASSERT_EQ(customGraphs.nbGraphIDs, 1u);
    ASSERT_EQ(customGraphs.graphids[0], graph);
}

TEST_F(CompressorTest, registerParameterizedGraph_customNodes)
{
    auto graph = ZL_Compressor_registerStaticGraph_fromNode1o(
            compressor_, ZL_NODE_DELTA_INT, ZL_GRAPH_ZSTD);
    ZL_NodeID node                 = ZL_NODE_FIELD_LZ;
    ZL_ParameterizedGraphDesc desc = {
        .graph         = graph,
        .customNodes   = &node,
        .nbCustomNodes = 1,
    };
    auto nodes = ZL_Compressor_registerParameterizedGraph(compressor_, &desc);
    auto customNodes = ZL_Compressor_Graph_getCustomNodes(compressor_, nodes);
    ASSERT_EQ(customNodes.nbNodeIDs, 1u);
    ASSERT_EQ(customNodes.nodeids[0], node);
}

TEST_F(CompressorTest, setParameter)
{
    ASSERT_EQ(getParameter(ZL_CParam_formatVersion), 0);
    setParameter(ZL_CParam_formatVersion, ZL_MAX_FORMAT_VERSION);
    ASSERT_EQ(getParameter(ZL_CParam_formatVersion), ZL_MAX_FORMAT_VERSION);
    auto params = getParameters();
    ASSERT_EQ(params.size(), 1u);
    ASSERT_EQ(params.at(ZL_CParam_formatVersion), ZL_MAX_FORMAT_VERSION);
    setParameter(ZL_CParam_formatVersion, 0);
    ASSERT_EQ(getParameter(ZL_CParam_formatVersion), 0);
    ASSERT_EQ(getParameters().size(), 0u);

    setParameter(ZL_CParam_compressionLevel, 1);
    setParameter(ZL_CParam_decompressionLevel, 2);
    ASSERT_EQ(getParameters().size(), 2u);
}

TEST_F(CompressorTest, getNode)
{
    auto node = ZL_Compressor_getNode(compressor_, "zl.field_lz");
    ASSERT_NE(node, ZL_NODE_ILLEGAL);
    ASSERT_EQ(node, ZL_NODE_FIELD_LZ);

    node = ZL_Compressor_getNode(compressor_, "zl.field_lz#0");
    ASSERT_EQ(node, ZL_NODE_ILLEGAL);

    auto clone = ZL_Compressor_cloneNode(
            compressor_, ZL_NODE_FIELD_LZ, &localParams_);

    node = ZL_Compressor_getNode(compressor_, "zl.field_lz");
    ASSERT_NE(node, ZL_NODE_ILLEGAL);
    ASSERT_EQ(node, ZL_NODE_FIELD_LZ);

    ASSERT_EQ(
            std::string("zl.field_lz#0"),
            ZL_Compressor_Node_getName(compressor_, clone));

    node = ZL_Compressor_getNode(compressor_, "zl.field_lz#0");
    ASSERT_NE(node, ZL_NODE_ILLEGAL);
    ASSERT_EQ(node, clone);

    node = ZL_Compressor_getNode(compressor_, "zl.field_lz#1");
    ASSERT_EQ(node, ZL_NODE_ILLEGAL);

    auto clone2 = ZL_Compressor_cloneNode(
            compressor_, ZL_NODE_FIELD_LZ, &localParams_);
    ASSERT_EQ(
            std::string("zl.field_lz#1"),
            ZL_Compressor_Node_getName(compressor_, clone2));

    node = ZL_Compressor_getNode(compressor_, "zl.field_lz#1");
    ASSERT_NE(node, ZL_NODE_ILLEGAL);
    ASSERT_EQ(node, clone2);

    node = ZL_Compressor_getNode(compressor_, "zl.field_lz#0");
    ASSERT_NE(node, ZL_NODE_ILLEGAL);
    ASSERT_EQ(node, clone);

    node = ZL_Compressor_getNode(compressor_, "custom_transform");
    ASSERT_EQ(node, ZL_NODE_ILLEGAL);

    auto custom = makeCustomTransform(true);
    ASSERT_EQ(
            std::string("custom_transform"),
            ZL_Compressor_Node_getName(compressor_, custom));

    node = ZL_Compressor_getNode(compressor_, "custom_transform");
    ASSERT_NE(node, ZL_NODE_ILLEGAL);
    ASSERT_EQ(node, custom);

    node = ZL_Compressor_getNode(compressor_, "custom_transform#0");
    ASSERT_EQ(node, ZL_NODE_ILLEGAL);
    node = ZL_Compressor_getNode(compressor_, "custom_transform#1");
    ASSERT_EQ(node, ZL_NODE_ILLEGAL);
    node = ZL_Compressor_getNode(compressor_, "custom_transform#2");
    ASSERT_EQ(node, ZL_NODE_ILLEGAL);
    node = ZL_Compressor_getNode(compressor_, "custom_transform#3");
    ASSERT_EQ(node, ZL_NODE_ILLEGAL);

    auto custom2 = makeCustomTransform(false);
    ASSERT_EQ(
            std::string("custom_transform#3"),
            ZL_Compressor_Node_getName(compressor_, custom2));

    node = ZL_Compressor_getNode(compressor_, "custom_transform#3");
    ASSERT_NE(node, ZL_NODE_ILLEGAL);
    ASSERT_EQ(node, custom2);

    node = ZL_Compressor_getNode(compressor_, "custom_transform");
    ASSERT_NE(node, ZL_NODE_ILLEGAL);
    ASSERT_EQ(node, custom);

    node = ZL_Compressor_getNode(compressor_, "zl.field_lz#0");
    ASSERT_NE(node, ZL_NODE_ILLEGAL);
    ASSERT_EQ(node, clone);

    node = ZL_Compressor_getNode(compressor_, "zl.field_lz#1");
    ASSERT_NE(node, ZL_NODE_ILLEGAL);
    ASSERT_EQ(node, clone2);

    node = ZL_Compressor_getNode(compressor_, "zl.field_lz");
    ASSERT_NE(node, ZL_NODE_ILLEGAL);
    ASSERT_EQ(node, ZL_NODE_FIELD_LZ);
}

TEST_F(CompressorTest, registerParameterizedNode)
{
    auto node                     = ZL_NODE_FIELD_LZ;
    ZL_ParameterizedNodeDesc desc = {
        .name = "my_node",
        .node = node,
    };
    auto clone = ZL_Compressor_registerParameterizedNode(compressor_, &desc);
    ASSERT_EQ(
            std::string("my_node#0"),
            ZL_Compressor_Node_getName(compressor_, clone));
    ASSERT_NE(node, clone);
    ASSERT_EQ(node, ZL_Compressor_Node_getBaseNodeID(compressor_, clone));

    desc.name   = "!my_node";
    auto clone2 = ZL_Compressor_registerParameterizedNode(compressor_, &desc);
    ASSERT_EQ(
            std::string("my_node"),
            ZL_Compressor_Node_getName(compressor_, clone2));
    ASSERT_EQ(clone2, ZL_Compressor_getNode(compressor_, "my_node"));
    ASSERT_NE(clone, clone2);
}

TEST_F(CompressorTest, getGraph)
{
    // store is a special graph, make sure to test it directly
    auto graph = ZL_Compressor_getGraph(compressor_, "zl.store");
    ASSERT_NE(graph, ZL_GRAPH_ILLEGAL);
    ASSERT_EQ(graph, ZL_GRAPH_STORE);

    graph = ZL_Compressor_getGraph(compressor_, "zl.zstd");
    ASSERT_NE(graph, ZL_GRAPH_ILLEGAL);
    ASSERT_EQ(graph, ZL_GRAPH_ZSTD);

    auto clone = makeParameterizedGraph(false, false);
    ASSERT_NE(clone, ZL_GRAPH_FIELD_LZ);

    ASSERT_EQ(
            std::string("zl.field_lz#0"),
            ZL_Compressor_Graph_getName(compressor_, clone));

    graph = ZL_Compressor_getGraph(compressor_, "zl.field_lz#0");
    ASSERT_NE(graph, ZL_GRAPH_ILLEGAL);
    ASSERT_EQ(graph, clone);

    graph = ZL_Compressor_getGraph(compressor_, "zl.field_lz");
    ASSERT_NE(graph, ZL_GRAPH_ILLEGAL);
    ASSERT_EQ(graph, ZL_GRAPH_FIELD_LZ);

    auto clone2 = makeParameterizedGraph(true, false);
    ASSERT_NE(clone2, ZL_GRAPH_FIELD_LZ);

    ASSERT_EQ(
            std::string("parameterized#1"),
            ZL_Compressor_Graph_getName(compressor_, clone2));
    graph = ZL_Compressor_getGraph(compressor_, "parameterized#1");
    ASSERT_NE(graph, ZL_GRAPH_ILLEGAL);
    ASSERT_EQ(graph, clone2);

    graph = ZL_Compressor_getGraph(compressor_, "parameterized");
    ASSERT_EQ(graph, ZL_GRAPH_ILLEGAL);

    auto clone3 = makeParameterizedGraph(true, true);
    ASSERT_NE(clone3, ZL_GRAPH_FIELD_LZ);

    ASSERT_EQ(
            std::string("parameterized"),
            ZL_Compressor_Graph_getName(compressor_, clone3));
    graph = ZL_Compressor_getGraph(compressor_, "parameterized");
    ASSERT_NE(graph, ZL_GRAPH_ILLEGAL);
    ASSERT_EQ(graph, clone3);

    auto static0 = makeStaticGraph(false);
    auto static1 = makeStaticGraph(true);
    graph        = ZL_Compressor_getGraph(compressor_, "static#3");
    ASSERT_EQ(static0, graph);
    graph = ZL_Compressor_getGraph(compressor_, "static");
    ASSERT_EQ(static1, graph);

    auto selector1 = makeSelectorGraph(true);
    auto selector0 = makeSelectorGraph(false);
    graph          = ZL_Compressor_getGraph(compressor_, "selector#6");
    ASSERT_EQ(selector0, graph);
    graph = ZL_Compressor_getGraph(compressor_, "selector");
    ASSERT_EQ(selector1, graph);

    auto dynamic0 = makeDynamicGraph(false);
    auto dynamic1 = makeDynamicGraph(true);
    graph         = ZL_Compressor_getGraph(compressor_, "dynamic#7");
    ASSERT_EQ(dynamic0, graph);
    graph = ZL_Compressor_getGraph(compressor_, "dynamic");
    ASSERT_EQ(dynamic1, graph);

    auto multiInput1 = makeMultiInputGraph(true, true);
    auto multiInput0 = makeMultiInputGraph(false, false);
    graph            = ZL_Compressor_getGraph(compressor_, "multi_input");
    ASSERT_EQ(multiInput1, graph);
    graph = ZL_Compressor_getGraph(compressor_, "multi_input#10");
    ASSERT_EQ(multiInput0, graph);
}

TEST_F(CompressorTest, forEachGraph)
{
    ASSERT_EQ(getGraphs().size(), 0u);

    ZL_Compressor_cloneNode(compressor_, ZL_NODE_DELTA_INT, &localParams_);
    ASSERT_EQ(getGraphs().size(), 0u);

    auto graph0 = ZL_Compressor_registerStaticGraph_fromNode1o(
            compressor_, ZL_NODE_INTERPRET_AS_LE64, ZL_GRAPH_CONSTANT);
    ASSERT_EQ(getGraphs().size(), 1u);
    ASSERT_EQ(getGraphs()[0], graph0);

    auto graph1 = makeDynamicGraph();
    ASSERT_EQ(getGraphs().size(), 2u);
    ASSERT_EQ(getGraphs()[0], graph0);
    ASSERT_EQ(getGraphs()[1], graph1);
}

TEST_F(CompressorTest, forEachNode)
{
    ASSERT_EQ(getNodes().size(), 0u);

    ZL_Compressor_registerStaticGraph_fromNode1o(
            compressor_, ZL_NODE_INTERPRET_AS_LE64, ZL_GRAPH_CONSTANT);
    ASSERT_EQ(getNodes().size(), 0u);

    auto node0 = ZL_Compressor_cloneNode(
            compressor_, ZL_NODE_DELTA_INT, &localParams_);
    ASSERT_EQ(getNodes().size(), 1u);
    ASSERT_EQ(getNodes()[0], node0);
}

TEST_F(CompressorTest, selectStartingGraphID)
{
    ZL_GraphID startingGraph;
    ASSERT_FALSE(ZL_Compressor_getStartingGraphID(compressor_, &startingGraph));
    ASSERT_EQ(startingGraph, ZL_GRAPH_ILLEGAL);

    ZL_REQUIRE_SUCCESS(ZL_Compressor_selectStartingGraphID(
            compressor_, ZL_GRAPH_FIELD_LZ));
    ASSERT_TRUE(ZL_Compressor_getStartingGraphID(compressor_, &startingGraph));
    ASSERT_EQ(startingGraph, ZL_GRAPH_FIELD_LZ);
}

TEST_F(CompressorTest, Graph_getGraphType)
{
    ASSERT_EQ(
            ZL_GraphType_standard,
            ZL_Compressor_getGraphType(compressor_, ZL_GRAPH_STORE));
    ASSERT_EQ(
            ZL_GraphType_standard,
            ZL_Compressor_getGraphType(compressor_, ZL_GRAPH_CONSTANT));
    ASSERT_EQ(
            ZL_GraphType_standard,
            ZL_Compressor_getGraphType(compressor_, ZL_GRAPH_DELTA_ZSTD));
    ASSERT_EQ(
            ZL_GraphType_standard,
            ZL_Compressor_getGraphType(compressor_, ZL_GRAPH_FIELD_LZ));
    ASSERT_EQ(
            ZL_GraphType_standard,
            ZL_Compressor_getGraphType(compressor_, ZL_GRAPH_ZSTD));
    ASSERT_EQ(
            ZL_GraphType_standard,
            ZL_Compressor_getGraphType(
                    compressor_, ZL_GRAPH_GENERIC_LZ_BACKEND));
    ASSERT_EQ(
            ZL_GraphType_standard,
            ZL_Compressor_getGraphType(compressor_, ZL_GRAPH_COMPRESS_GENERIC));

    ASSERT_EQ(
            ZL_GraphType_static,
            ZL_Compressor_getGraphType(compressor_, makeStaticGraph()));
    ASSERT_EQ(
            ZL_GraphType_selector,
            ZL_Compressor_getGraphType(compressor_, makeSelectorGraph()));
    ASSERT_EQ(
            ZL_GraphType_multiInput,
            ZL_Compressor_getGraphType(compressor_, makeDynamicGraph()));
    ASSERT_EQ(
            ZL_GraphType_multiInput,
            ZL_Compressor_getGraphType(compressor_, makeMultiInputGraph()));
    ASSERT_EQ(
            ZL_GraphType_parameterized,
            ZL_Compressor_getGraphType(compressor_, makeParameterizedGraph()));
}

TEST_F(CompressorTest, Graph_getName)
{
    ASSERT_EQ(
            std::string("zl.store"),
            ZL_Compressor_Graph_getName(compressor_, ZL_GRAPH_STORE));
    ASSERT_EQ(
            std::string("zl.zstd"),
            ZL_Compressor_Graph_getName(compressor_, ZL_GRAPH_ZSTD));
    ASSERT_EQ(
            std::string("zl.field_lz"),
            ZL_Compressor_Graph_getName(compressor_, ZL_GRAPH_FIELD_LZ));

    ASSERT_EQ(
            std::string("static#0"),
            ZL_Compressor_Graph_getName(compressor_, makeStaticGraph()));
    ASSERT_EQ(
            std::string("selector#1"),
            ZL_Compressor_Graph_getName(compressor_, makeSelectorGraph()));
    ASSERT_EQ(
            std::string("dynamic#2"),
            ZL_Compressor_Graph_getName(compressor_, makeDynamicGraph()));
    ASSERT_EQ(
            std::string("multi_input#3"),
            ZL_Compressor_Graph_getName(compressor_, makeMultiInputGraph()));
}

TEST_F(CompressorTest, Graph_getInputMask)
{
    auto test = [&](std::initializer_list<ZL_Type> types, ZL_GraphID graph) {
        if (types.size() == 1) {
            ASSERT_EQ(
                    *types.begin(),
                    ZL_Compressor_Graph_getInput0Mask(compressor_, graph));
        }
        ASSERT_EQ(
                ZL_Compressor_Graph_getNumInputs(compressor_, graph),
                types.size());
        for (size_t i = 0; i < types.size(); ++i) {
            ASSERT_EQ(
                    types.begin()[i],
                    ZL_Compressor_Graph_getInputMask(compressor_, graph, i));
        }
    };

    test({ ZL_Type_serial }, ZL_GRAPH_ZSTD);
    test({ (ZL_Type)(ZL_Type_struct | ZL_Type_numeric) }, ZL_GRAPH_FIELD_LZ);
    test({ ZL_Type_numeric }, makeStaticGraph());
    test({ (ZL_Type)(ZL_Type_struct | ZL_Type_numeric) }, makeSelectorGraph());
    test({ ZL_Type_serial }, makeDynamicGraph());
    test({ ZL_Type_serial, ZL_Type_numeric }, makeMultiInputGraph());
}

TEST_F(CompressorTest, Graph_isVariableInput)
{
    ASSERT_TRUE(
            ZL_Compressor_Graph_isVariableInput(compressor_, ZL_GRAPH_STORE));
    ASSERT_TRUE(ZL_Compressor_Graph_isVariableInput(
            compressor_, ZL_GRAPH_COMPRESS_GENERIC));
    ASSERT_TRUE(ZL_Compressor_Graph_isVariableInput(
            compressor_, makeMultiInputGraph()));

    ASSERT_FALSE(
            ZL_Compressor_Graph_isVariableInput(compressor_, ZL_GRAPH_ZSTD));
    ASSERT_FALSE(ZL_Compressor_Graph_isVariableInput(
            compressor_, makeStaticGraph()));
    ASSERT_FALSE(ZL_Compressor_Graph_isVariableInput(
            compressor_, makeSelectorGraph()));
    ASSERT_FALSE(ZL_Compressor_Graph_isVariableInput(
            compressor_, makeDynamicGraph()));
    ASSERT_FALSE(ZL_Compressor_Graph_isVariableInput(
            compressor_, makeMultiInputGraph(false)));
}

TEST_F(CompressorTest, Graph_getHeadNode)
{
    ASSERT_EQ(
            ZL_NODE_ILLEGAL,
            ZL_Compressor_Graph_getHeadNode(compressor_, ZL_GRAPH_STORE));
    ASSERT_EQ(
            ZL_NODE_ILLEGAL,
            ZL_Compressor_Graph_getHeadNode(compressor_, ZL_GRAPH_ZSTD));
    ASSERT_EQ(
            ZL_NODE_ILLEGAL,
            ZL_Compressor_Graph_getHeadNode(compressor_, ZL_GRAPH_FIELD_LZ));
    ASSERT_EQ(
            ZL_NODE_ILLEGAL,
            ZL_Compressor_Graph_getHeadNode(compressor_, ZL_GRAPH_DELTA_ZSTD));

    ASSERT_EQ(
            ZL_NODE_FLOAT16_DECONSTRUCT,
            ZL_Compressor_Graph_getHeadNode(compressor_, makeStaticGraph()));
    ASSERT_EQ(
            ZL_NODE_ILLEGAL,
            ZL_Compressor_Graph_getHeadNode(compressor_, makeSelectorGraph()));
    ASSERT_EQ(
            ZL_NODE_ILLEGAL,
            ZL_Compressor_Graph_getHeadNode(compressor_, makeDynamicGraph()));
    ASSERT_EQ(
            ZL_NODE_ILLEGAL,
            ZL_Compressor_Graph_getHeadNode(
                    compressor_, makeMultiInputGraph()));
}

TEST_F(CompressorTest, Graph_getSuccessors)
{
    ASSERT_EQ(
            0u,
            ZL_Compressor_Graph_getSuccessors(compressor_, ZL_GRAPH_STORE)
                    .nbGraphIDs);
    ASSERT_EQ(
            0u,
            ZL_Compressor_Graph_getSuccessors(compressor_, ZL_GRAPH_ZSTD)
                    .nbGraphIDs);
    ASSERT_EQ(
            0u,
            ZL_Compressor_Graph_getSuccessors(compressor_, ZL_GRAPH_FIELD_LZ)
                    .nbGraphIDs);
    ASSERT_EQ(
            0u,
            ZL_Compressor_Graph_getSuccessors(compressor_, ZL_GRAPH_DELTA_ZSTD)
                    .nbGraphIDs);

    ASSERT_EQ(
            2u,
            ZL_Compressor_Graph_getSuccessors(compressor_, makeStaticGraph())
                    .nbGraphIDs);
    ASSERT_EQ(
            ZL_GRAPH_FIELD_LZ,
            ZL_Compressor_Graph_getSuccessors(compressor_, makeStaticGraph())
                    .graphids[0]);
    ASSERT_EQ(
            ZL_GRAPH_ZSTD,
            ZL_Compressor_Graph_getSuccessors(compressor_, makeStaticGraph())
                    .graphids[1]);
    ASSERT_EQ(
            0u,
            ZL_Compressor_Graph_getSuccessors(compressor_, makeSelectorGraph())
                    .nbGraphIDs);
    ASSERT_EQ(
            0u,
            ZL_Compressor_Graph_getSuccessors(compressor_, makeDynamicGraph())
                    .nbGraphIDs);
    ASSERT_EQ(
            0u,
            ZL_Compressor_Graph_getSuccessors(
                    compressor_, makeMultiInputGraph())
                    .nbGraphIDs);
}

TEST_F(CompressorTest, Graph_getCustomNodes)
{
    ASSERT_EQ(
            0u,
            ZL_Compressor_Graph_getCustomNodes(compressor_, ZL_GRAPH_STORE)
                    .nbNodeIDs);
    ASSERT_EQ(
            0u,
            ZL_Compressor_Graph_getCustomNodes(compressor_, ZL_GRAPH_ZSTD)
                    .nbNodeIDs);
    ASSERT_EQ(
            0u,
            ZL_Compressor_Graph_getCustomNodes(compressor_, ZL_GRAPH_FIELD_LZ)
                    .nbNodeIDs);
    ASSERT_EQ(
            0u,
            ZL_Compressor_Graph_getCustomNodes(compressor_, ZL_GRAPH_DELTA_ZSTD)
                    .nbNodeIDs);

    ASSERT_EQ(
            0u,
            ZL_Compressor_Graph_getCustomNodes(compressor_, makeStaticGraph())
                    .nbNodeIDs);
    ASSERT_EQ(
            0u,
            ZL_Compressor_Graph_getCustomNodes(compressor_, makeSelectorGraph())
                    .nbNodeIDs);
    ASSERT_EQ(
            2u,
            ZL_Compressor_Graph_getCustomNodes(compressor_, makeDynamicGraph())
                    .nbNodeIDs);
    ASSERT_EQ(
            1u,
            ZL_Compressor_Graph_getCustomNodes(
                    compressor_, makeMultiInputGraph())
                    .nbNodeIDs);
}

TEST_F(CompressorTest, Graph_getCustomGraphs)
{
    ASSERT_EQ(
            0u,
            ZL_Compressor_Graph_getCustomGraphs(compressor_, ZL_GRAPH_STORE)
                    .nbGraphIDs);
    ASSERT_EQ(
            0u,
            ZL_Compressor_Graph_getCustomGraphs(compressor_, ZL_GRAPH_ZSTD)
                    .nbGraphIDs);
    ASSERT_EQ(
            0u,
            ZL_Compressor_Graph_getCustomGraphs(compressor_, ZL_GRAPH_FIELD_LZ)
                    .nbGraphIDs);
    ASSERT_EQ(
            0u,
            ZL_Compressor_Graph_getCustomGraphs(
                    compressor_, ZL_GRAPH_DELTA_ZSTD)
                    .nbGraphIDs);

    ASSERT_EQ(
            0u,
            ZL_Compressor_Graph_getCustomGraphs(compressor_, makeStaticGraph())
                    .nbGraphIDs);
    ASSERT_EQ(
            2u,
            ZL_Compressor_Graph_getCustomGraphs(
                    compressor_, makeSelectorGraph())
                    .nbGraphIDs);
    ASSERT_EQ(
            1u,
            ZL_Compressor_Graph_getCustomGraphs(compressor_, makeDynamicGraph())
                    .nbGraphIDs);
    ASSERT_EQ(
            1u,
            ZL_Compressor_Graph_getCustomGraphs(
                    compressor_, makeMultiInputGraph())
                    .nbGraphIDs);
}

TEST_F(CompressorTest, Graph_getLocalParams)
{
    expectParamsEmpty(
            ZL_Compressor_Graph_getLocalParams(compressor_, ZL_GRAPH_STORE));
    expectParamsEmpty(
            ZL_Compressor_Graph_getLocalParams(compressor_, ZL_GRAPH_ZSTD));
    expectParamsEmpty(
            ZL_Compressor_Graph_getLocalParams(compressor_, ZL_GRAPH_FIELD_LZ));

    expectParamsEq(
            localParams_,
            ZL_Compressor_Graph_getLocalParams(compressor_, makeStaticGraph()));
    expectParamsEq(
            localParams_,
            ZL_Compressor_Graph_getLocalParams(
                    compressor_, makeSelectorGraph()));
    expectParamsEq(
            localParams_,
            ZL_Compressor_Graph_getLocalParams(
                    compressor_, makeDynamicGraph()));
    expectParamsEq(
            localParams_,
            ZL_Compressor_Graph_getLocalParams(
                    compressor_, makeMultiInputGraph()));

    auto graph = makeParameterizedGraph();
    expectParamsEq(
            localParams_,
            ZL_Compressor_Graph_getLocalParams(compressor_, graph));
}

TEST_F(CompressorTest, Node_isVariableInput)
{
    ASSERT_FALSE(ZL_Compressor_Node_isVariableInput(compressor_, ZL_NODE_ZSTD));
    ASSERT_FALSE(
            ZL_Compressor_Node_isVariableInput(compressor_, ZL_NODE_FIELD_LZ));
    ASSERT_TRUE(ZL_Compressor_Node_isVariableInput(
            compressor_, ZL_NODE_CONCAT_SERIAL));
    ASSERT_TRUE(ZL_Compressor_Node_isVariableInput(
            compressor_, ZL_NODE_DEDUP_NUMERIC));
}

TEST_F(CompressorTest, Node_getLocalParams)
{
    expectParamsEmpty(
            ZL_Compressor_Node_getLocalParams(compressor_, ZL_NODE_ZSTD));
    expectParamsEmpty(
            ZL_Compressor_Node_getLocalParams(compressor_, ZL_NODE_FIELD_LZ));
    expectParamsEmpty(
            ZL_Compressor_Node_getLocalParams(compressor_, ZL_NODE_DELTA_INT));

    auto node = ZL_Compressor_cloneNode(
            compressor_, ZL_NODE_DELTA_INT, &localParams_);
    expectParamsEq(
            localParams_, ZL_Compressor_Node_getLocalParams(compressor_, node));
}
