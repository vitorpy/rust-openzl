// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <stdexcept>

#include "tools/logger/Logger.h"
#include "tools/training/ace/ace.h"
#include "tools/training/clustering/clustering_graph_trainer.h"
#include "tools/training/graph_mutation/graph_mutation_utils.h"
#include "tools/training/train.h"

using namespace openzl::training::graph_mutation;
using namespace openzl::tools::logger;

namespace openzl::training {

std::vector<std::shared_ptr<const std::string_view>> train(
        const std::vector<MultiInput>& inputs,
        Compressor& compressor,
        const TrainParams& trainParams)
{
    std::vector<std::shared_ptr<const std::string_view>>
            serializedTrainedCompressors;
    if (!trainParams.compressorGenFunc) {
        throw Exception("Compressor generator function is not set.");
    }
    if (graph_mutation::hasTargetGraph(compressor, CLUSTERING_GRAPH_NAME)) {
        serializedTrainedCompressors.clear();
        serializedTrainedCompressors.push_back(
                trainClusteringGraph(inputs, compressor, trainParams));
        auto newCompressor =
                trainParams.compressorGenFunc(*serializedTrainedCompressors[0]);
        compressor = std::move(*newCompressor);
    }

    if (graph_mutation::hasTargetGraph(compressor, ACE_GRAPH_NAME)) {
        serializedTrainedCompressors =
                trainAceCompressor(inputs, compressor.serialize(), trainParams);
    }

    if (serializedTrainedCompressors.empty()) {
        throw Exception("No trainable graph found in compressor.");
    }

    Logger::log(VERBOSE1, "Training completed successfully.");
    // TODO pretty print just the graphs (exclude params etc)
    Logger::log(
            VERBOSE3,
            "Smallest trained graph:",
            std::string(Compressor::convertSerializedToJson(
                                *serializedTrainedCompressors[0]))
                    .c_str());
    return serializedTrainedCompressors;
}
} // namespace openzl::training
