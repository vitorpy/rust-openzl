// Copyright (c) Meta Platforms, Inc. and affiliates.

#pragma once

#include <memory>
#include <vector>

#include <string_view>
#include "openzl/cpp/Compressor.hpp"
#include "tools/training/train_params.h"
#include "tools/training/utils/utils.h"

namespace openzl::training {

extern const std::string ACE_GRAPH_NAME;

/**
 * This function trains a graph that contains any number of ACE graphs.
 * It can be run on untrained ACE compressor or re-run on an already-trained
 * ACE compressor.
 *
 * @param inputs The inputs to train on
 * @param serializedCompressorInput The serialized compressor input
 * @param trainParams The training parameters to use
 *
 * @return A vector shared pointer to the trained serialized compressors.
 *         If `trainParams.paretoFront` is false, the vector will contain a
 *         single compressor. Otherwise, it will contain a Pareto frontier
 *         of compressors.
 */
std::vector<std::shared_ptr<const std::string_view>> trainAceCompressor(
        const std::vector<MultiInput>& inputs,
        std::string_view serializedCompressorInput,
        const TrainParams& trainParams);

} // namespace openzl::training
