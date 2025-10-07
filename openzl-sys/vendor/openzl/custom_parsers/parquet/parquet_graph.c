// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "parquet_graph.h"

#include "custom_parsers/parquet/parquet_lexer.h"
#include "openzl/compress/graphs/generic_clustering_graph.h"
#include "openzl/zl_dyngraph.h"
#include "openzl/zl_errors.h"
#include "openzl/zl_graph_api.h"

#define ZL_TRY_SET_EL(_var, _expr) ZL_TRY_SET_T(ZL_EdgeList, _var, _expr)

// Run the conversion node for the given type and width.
static ZL_Report
runConversion(ZL_Edge* in, ZL_Edge** out, ZL_Type type, size_t width)
{
    ZL_RESULT_DECLARE_SCOPE_REPORT(in);
    if (type == ZL_Type_serial) {
        *out = in;
        return ZL_returnSuccess();
    }

    ZL_EdgeList el;
    if (type == ZL_Type_numeric) {
        if (width == 1) {
            ZL_TRY_SET_EL(el, ZL_Edge_runNode(in, ZL_NODE_INTERPRET_AS_LE8));
        } else if (width == 2) {
            ZL_TRY_SET_EL(el, ZL_Edge_runNode(in, ZL_NODE_INTERPRET_AS_LE16));
        } else if (width == 4) {
            ZL_TRY_SET_EL(el, ZL_Edge_runNode(in, ZL_NODE_INTERPRET_AS_LE32));
        } else if (width == 8) {
            ZL_TRY_SET_EL(el, ZL_Edge_runNode(in, ZL_NODE_INTERPRET_AS_LE64));
        } else {
            return ZL_REPORT_ERROR(GENERIC, "Unsupported width %zu", width);
        }
    } else if (type == ZL_Type_struct) {
        ZL_IntParam const intParam = { ZL_trlip_tokenSize, (int32_t)width };
        ZL_LocalParams lParams     = { .intParams = { &intParam, 1 } };
        ZL_TRY_SET_EL(
                el,
                ZL_Edge_runNode_withParams(
                        in, ZL_NODE_CONVERT_SERIAL_TO_TOKENX, &lParams));
    } else {
        return ZL_REPORT_ERROR(GENERIC, "Unsupported type %d", type);
    }

    ZL_ERR_IF_NE(el.nbEdges, 1, GENERIC, "Unexpected number of edges");
    *out = el.edges[0];

    return ZL_returnSuccess();
}

static ZL_Report parquetGraphInner(
        ZL_Graph* graph,
        ZL_Edge* ins[],
        size_t nbIns,
        ZL_ParquetLexer* lexer)
{
    ZL_RESULT_DECLARE_SCOPE_REPORT(graph);
    ZL_ERR_IF_NE(nbIns, 1, graph_invalidNumInputs);
    ZL_Edge* edge               = ins[0];
    ZL_Input const* const input = ZL_Edge_getData(edge);
    const size_t size           = ZL_Input_numElts(input);
    ZL_ErrorContext* errCtx     = ZL_GET_DEFAULT_ERROR_CONTEXT(graph);

    // Will return an error if the input is not a valid Parquet file.
    ZL_ERR_IF_ERR(
            ZL_ParquetLexer_init(lexer, ZL_Input_ptr(input), size, errCtx));

    // Allocate space for segment sizes.
    ZL_TRY_LET_R(maxNbSegments, ZL_ParquetLexer_maxNumTokens(lexer, errCtx));
    ZL_ERR_IF_EQ(maxNbSegments, 0, corruption);
    size_t* const segmentSizes =
            ZL_Graph_getScratchSpace(graph, maxNbSegments * sizeof(size_t));
    uint32_t* const dispatchTags =
            ZL_Graph_getScratchSpace(graph, maxNbSegments * sizeof(uint32_t));

    // Allocate space for token metadata. Note: the tag here is different from
    // the "dispatch tag" above. This tag identifies the schema element that a
    // given data page belongs to.
    uint32_t* const tags =
            ZL_Graph_getScratchSpace(graph, maxNbSegments * sizeof(uint32_t));
    ZL_Type* const types =
            ZL_Graph_getScratchSpace(graph, maxNbSegments * sizeof(ZL_Type));
    size_t* const widths =
            ZL_Graph_getScratchSpace(graph, maxNbSegments * sizeof(size_t));

    ZL_ERR_IF_NULL(segmentSizes, allocation);

    // Header stream metadata
    tags[0]   = 0;
    types[0]  = ZL_Type_serial;
    widths[0] = 1;

    // Iterate over all the tokens in the Parquet file, and fill out
    // segmentSizes and tags. All non-data pages are dispatched to the 0th
    // output edge. All data pages are dispatched to their own output edge.
    size_t nbSegments = 0;
    uint32_t nbTags   = 0;
    while (!ZL_ParquetLexer_finished(lexer)) {
        ZL_ParquetToken tokens[32] = { 0 };
        size_t nbTokens            = 0;
        ZL_TRY_SET_R(nbTokens, ZL_ParquetLexer_lex(lexer, tokens, 32, errCtx));
        for (size_t i = 0; i < nbTokens; ++i) {
            ZL_ERR_IF_GE(nbSegments, maxNbSegments, GENERIC);
            const ZL_ParquetToken token = tokens[i];
            bool isDataPage = token.type == ZL_ParquetTokenType_DataPage;
            // Fill in dispatch instructions.
            segmentSizes[nbSegments] = token.size;
            dispatchTags[nbSegments] = isDataPage ? ++nbTags : 0;

            // Fill in token metadata.
            if (isDataPage) {
                tags[nbTags]   = token.tag;
                types[nbTags]  = token.dataType;
                widths[nbTags] = token.dataWidth;
            }
            ++nbSegments;
        }
    }

    ZL_DispatchInstructions di = {
        .segmentSizes = segmentSizes,
        .nbSegments   = nbSegments,
        .tags         = dispatchTags,
        .nbTags       = nbTags + 1,
    };

    // Split the input according to segmentSizes
    ZL_TRY_LET_T(ZL_EdgeList, el, ZL_Edge_runDispatchNode(edge, &di));
    ZL_ERR_IF_NE(el.nbEdges, nbTags + 3, GENERIC);

    // Set the destination for the tags and segment sizes
    ZL_ERR_IF_ERR(
            ZL_Edge_setDestination(el.edges[0], ZL_GRAPH_COMPRESS_GENERIC));
    ZL_ERR_IF_ERR(
            ZL_Edge_setDestination(el.edges[1], ZL_GRAPH_COMPRESS_GENERIC));

    ZL_Edge** const edges = el.edges + 2;
    size_t const nbEdges  = el.nbEdges - 2;

    // Set the metadata for each edge and run the conversion
    for (size_t i = 0; i < nbEdges; ++i) {
        ZL_Edge* in  = edges[i];
        ZL_Edge* out = NULL;

        // Run the conversion
        ZL_ERR_IF_ERR(runConversion(in, &out, types[i], widths[i]));

        // Set the tag metadata for the clustering node
        ZL_ERR_IF_ERR(ZL_Edge_setIntMetadata(
                out, ZL_CLUSTERING_TAG_METADATA_ID, (int)tags[i]));
        edges[i] = out;
    }

    // Set the input clustering graph as the destination of the edges
    ZL_GraphIDList graphs = ZL_Graph_getCustomGraphs(graph);
    ZL_ERR_IF_NE(graphs.nbGraphIDs, 1, GENERIC);
    ZL_ERR_IF_ERR(ZL_Edge_setParameterizedDestination(
            edges, nbEdges, graphs.graphids[0], NULL));

    return ZL_returnSuccess();
}

// Wrapper around the inner graph function that allocates a lexer
static ZL_Report parquetGraphFn(ZL_Graph* graph, ZL_Edge* sctxs[], size_t nbIns)
{
    ZL_RESULT_DECLARE_SCOPE_REPORT(graph);
    ZL_ParquetLexer* lexer = ZL_ParquetLexer_create();
    ZL_ERR_IF_NULL(lexer, allocation);
    ZL_Report ret = parquetGraphInner(graph, sctxs, nbIns, lexer);
    ZL_ParquetLexer_free(lexer);
    return ret;
}

ZL_GraphID ZL_Parquet_registerGraph(
        ZL_Compressor* compressor,
        ZL_GraphID clusteringGraph)
{
    ZL_GraphID parser = ZL_Compressor_getGraph(compressor, "Parquet Parser");

    if (parser.gid == ZL_GRAPH_ILLEGAL.gid) {
        // Register the anchor graph
        ZL_FunctionGraphDesc desc = {
            .name           = "!Parquet Parser",
            .graph_f        = parquetGraphFn,
            .inputTypeMasks = (ZL_Type[]){ ZL_Type_serial },
            .nbInputs       = 1,
        };

        parser = ZL_Compressor_registerFunctionGraph(compressor, &desc);
    }

    // Register the parameterized graph
    ZL_ParameterizedGraphDesc const desc = {
        .name           = "Parquet Parser",
        .graph          = parser,
        .customGraphs   = &clusteringGraph,
        .nbCustomGraphs = 1,
    };
    return ZL_Compressor_registerParameterizedGraph(compressor, &desc);
}
