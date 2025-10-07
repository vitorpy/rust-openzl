// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <vector>

#include "openzl/common/allocation.h"
#include "openzl/compress/graphs/generic_clustering_graph.h"
#include "openzl/cpp/CCtx.hpp"
#include "openzl/cpp/Exception.hpp"
#include "openzl/zl_compressor.h"
#include "openzl/zl_reflection.h"

#include "tools/logger/Logger.h"
#include "tools/training/clustering/clustering_graph_trainer.h"
#include "tools/training/clustering/train_api.h"
#include "tools/training/graph_mutation/graph_mutation_utils.h"
#include "tools/training/sample_collection/training_sample_collector.h"
#include "tools/training/utils/utils.h"

using namespace openzl::training::graph_mutation;
using namespace openzl::tools::logger;

namespace openzl::training {

const std::string CLUSTERING_GRAPH_NAME = "zl.cluster";

namespace {

/**
 * Add a new parameterized version of the clustering graph to the compressor
 * which has ACE successors instead of the original successors.
 * NOTE: this function does not swap this new clustering graph into the
 * original graph, it just registers it on the compressor.
 */
std::string addAceSuccessors(
        Compressor& compressor,
        ZL_GraphID trainedClusteringGraphID)
{
    // Add the same number of ACE successors as there are clustering
    // successors
    auto numClusteringSuccessors =
            extractSuccessorsFromCbor(
                    compressor.serialize(),
                    ZL_Compressor_Graph_getName(
                            compressor.get(), trainedClusteringGraphID))
                    .size();

    std::vector<ZL_GraphID> aceGraphIds;
    aceGraphIds.reserve(numClusteringSuccessors);
    for (size_t i = 0; i < numClusteringSuccessors; i++) {
        aceGraphIds.push_back(ZL_Compressor_buildACEGraph(compressor.get()));
    }

    // Create and register new clustering graph with the ACE successors
    auto localParams = ZL_Compressor_Graph_getLocalParams(
            compressor.get(), trainedClusteringGraphID);

    ZL_ParameterizedGraphDesc const newClusteringGraphDesc = {
        .graph          = trainedClusteringGraphID,
        .customGraphs   = aceGraphIds.data(),
        .nbCustomGraphs = aceGraphIds.size(),
        .localParams    = &localParams
    };

    // Replace original clustering graph with the new one
    auto clusterGraphWithAceSuccessorsID =
            ZL_Compressor_registerParameterizedGraph(
                    compressor.get(), &newClusteringGraphDesc);

    return ZL_Compressor_Graph_getName(
            compressor.get(), clusterGraphWithAceSuccessorsID);
}

/**
 * Add a new parameterized version of the clustering graph to the compressor
 * which has clustered successors.
 * NOTE: this function does not swap this new clustered graph into the
 * original graph, it just registers it on the compressor.
 */
ZL_GraphID clusterSuccessors(
        const std::vector<MultiInput>& inputs,
        Compressor& compressor,
        const TrainParams& trainParams,
        const std::string& clusteringGraphUniqueNameUntrained)
{
    auto cctx                                = refCCtxForTraining(compressor);
    const auto serializedCompressorUntrained = compressor.serialize();

    // Get successors for training
    auto successorsFromSerialization = extractSuccessorsFromCbor(
            serializedCompressorUntrained, clusteringGraphUniqueNameUntrained);
    std::vector<ZL_GraphID> successorsVec;
    successorsVec.reserve(successorsFromSerialization.size());
    for (const auto& successor : successorsFromSerialization) {
        ZL_GraphID graphId =
                ZL_Compressor_getGraph(compressor.get(), successor.c_str());
        successorsVec.push_back(graphId);
    }

    // Get clustering codecs for training
    auto nodesFromSerialization = extractNodesFromCbor(
            serializedCompressorUntrained, CLUSTERING_GRAPH_NAME);

    std::vector<ZL_NodeID> clusteringCodecs;
    clusteringCodecs.reserve(nodesFromSerialization.size());
    for (const auto& node : nodesFromSerialization) {
        ZL_NodeID nodeID =
                ZL_Compressor_getNode(compressor.get(), node.c_str());
        clusteringCodecs.push_back(nodeID);
    }
    auto samples = collectInputStreamsForGraph(
            inputs, clusteringGraphUniqueNameUntrained, cctx);
    Logger::log_c(
            VERBOSE1, "Training cluster with %zu samples", samples.size());

    // Train clustering graph
    const auto arena = detail::NonNullUniqueCPtr<Arena>(
            ALLOC_HeapArena_create(), ALLOC_Arena_freeArena);

    ZL_GraphID startingGraphId;
    ZL_Compressor_getStartingGraphID(compressor.get(), &startingGraphId);

    compressor.selectStartingGraph(ZL_GRAPH_CLUSTERING);
    auto trainedClusteringGraphID = train_cluster(
            compressor.get(),
            *arena,
            samples,
            successorsVec,
            clusteringCodecs,
            {}, // TODO: Compute this inside of train_cluster
            trainParams);
    compressor.selectStartingGraph(startingGraphId);

    return trainedClusteringGraphID;
}

/**
 * Get the unique name of the clustering graph in the compressor's
 * graph. There is expected to be exactly one clustering graph.
 */
std::string getClusteringGraphUniqueName(Compressor& compressor)
{
    auto clusteringGraphNames = findAllGraphsWithPrefix(
            compressor.serialize(), CLUSTERING_GRAPH_NAME);
    if (clusteringGraphNames.size() != 1) {
        throw Exception(
                "Graph must contain a single clustering graph, instead it contains "
                + std::to_string(clusteringGraphNames.size())
                + " clustering graphs");
    }
    return clusteringGraphNames[0];
}

} // namespace

std::shared_ptr<const std::string_view> trainClusteringGraph(
        const std::vector<MultiInput>& inputs,
        Compressor& compressor,
        const TrainParams& trainParams)
{
    // Get name of untrained clustering graph which will be replaced in the
    // final trained graph
    const auto clusteringGraphUniqueNameUntrained =
            getClusteringGraphUniqueName(compressor);

    // Cluster successors (or don't cluster)
    ZL_GraphID trainedClusteringGraphID = trainParams.noClustering
            ? ZL_Compressor_getGraph(
                      compressor.get(),
                      clusteringGraphUniqueNameUntrained.c_str())
            : clusterSuccessors(
                      inputs,
                      compressor,
                      trainParams,
                      clusteringGraphUniqueNameUntrained);

    // Replace default successors with ACE successors or keep original name
    const std::string clusteringGraphUniqueNameFinal =
            trainParams.noAceSuccessors
            ? ZL_Compressor_Graph_getName(
                      compressor.get(), trainedClusteringGraphID)
            : addAceSuccessors(compressor, trainedClusteringGraphID);

    // Replace original clustering graph with the new one that uses
    // clustering and/or ACE successors
    return createSharedStringView(renameGraphInCompressor(
            compressor.serialize(),
            clusteringGraphUniqueNameUntrained,
            clusteringGraphUniqueNameFinal));
}

} // namespace openzl::training
