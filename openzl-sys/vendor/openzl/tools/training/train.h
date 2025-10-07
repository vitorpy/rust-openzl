// Copyright (c) Meta Platforms, Inc. and affiliates.

#pragma once

#include "openzl/cpp/Compressor.hpp"
#include "tools/training/train_params.h"
#include "tools/training/utils/utils.h"

namespace openzl::training {

/**
 * This function trains compressor graphs (clustering and/or ACE graphs).
 * It takes in a vector of buffer data, processes it through the training
 * pipeline, and returns a trained serialized compressor.
 *
 * @param inputs The input data to train on.
 * @param compressor The compressor to train.
 * @param trainerName The name of the training algorithm to use (optional).
 * @param maxThreads The maximum number of threads to use for training.
 * @param numSamples The number of samples to use for training (optional).
 *
 * @return A vector shared pointer to the trained serialized compressors.
 *         If `trainParams.paretoFront` is false, the vector will contain a
 *         single compressor. Otherwise, it will contain a Pareto frontier
 *         of compressors.
 */
std::vector<std::shared_ptr<const std::string_view>> train(
        const std::vector<MultiInput>& inputs,
        Compressor& compressor,
        const TrainParams& trainParams);

} // namespace openzl::training
