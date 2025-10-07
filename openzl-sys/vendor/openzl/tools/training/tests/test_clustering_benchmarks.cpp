// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <gtest/gtest.h>

#include "custom_parsers/csv/csv_profile.h"
#include "custom_parsers/dependency_registration.h"
#include "openzl/cpp/Compressor.hpp"
#include "openzl/cpp/Input.hpp"
#include "tools/training/tests/benchmark_files/ppmf_unit_segment.h"
#include "tools/training/train.h"

namespace openzl::tests {
namespace {
class TestClusteringBenchmarks : public testing::Test {
   public:
    void TearDown() override
    {
        inputs_.clear();
    }

    void SetUp() override {}

    float trainAndBenchmarkRatio(
            Compressor& compressor,
            ZL_GraphFn trainingGraphFn)
    {
        // Register the graph to train in the compressor
        trainingGraphFn(compressor.get());
        // Train the compressor and serialize it
        auto serialized = training::train(inputs_, compressor, params_);
        // Compress the data using the trained compressor
        CCtx cctx;
        cctx.setParameter(CParam::FormatVersion, ZL_MAX_FORMAT_VERSION);
        auto newCompressor =
                custom_parsers::createCompressorFromSerialized(*serialized[0]);
        cctx.refCompressor(*newCompressor);
        size_t compressedSize   = 0;
        size_t uncompressedSize = 0;
        for (auto& input : inputs_) {
            auto compressed = cctx.compress(*input);
            compressedSize += compressed.size();
            // Hack that assumes the input is serial
            uncompressedSize += (*input).front().contentSize();
        }
        return double(uncompressedSize) / compressedSize;
    }

   protected:
    std::vector<training::MultiInput> inputs_;
    training::TrainParams params_;
};

TEST_F(TestClusteringBenchmarks, benchmarkPpmfUnit)
{
    Input input = Input::refSerial(ppmfCsvString);
    std::vector<Input> inputs;
    inputs.push_back(std::move(input));
    training::MultiInput multiInput = training::MultiInput(std::move(inputs));
    params_                         = {
                                .compressorGenFunc = custom_parsers::createCompressorFromSerialized,
                                .threads           = 1,
                                .clusteringTrainer = training::ClusteringTrainer::Greedy,
                                .noAceSuccessors   = true,
    };
    inputs_.emplace_back(std::move(multiInput));

    Compressor compressor1, compressor2, compressor3;
    auto compressedRatio = trainAndBenchmarkRatio(
            compressor1, custom_parsers::ZL_createGraph_genericCSVCompressor);
    // Default = greedy clustering
    EXPECT_GT(compressedRatio, 30.0);
    // Bottom-up clustering
    params_.clusteringTrainer = training::ClusteringTrainer::BottomUp;
    compressedRatio           = trainAndBenchmarkRatio(
            compressor2, custom_parsers::ZL_createGraph_genericCSVCompressor);
    EXPECT_GT(compressedRatio, 25.0);
    // Full Split clustering
    params_.clusteringTrainer = training::ClusteringTrainer::FullSplit;
    compressedRatio           = trainAndBenchmarkRatio(
            compressor3, custom_parsers::ZL_createGraph_genericCSVCompressor);
    EXPECT_GT(compressedRatio, 20.0);
}

} // namespace
} // namespace openzl::tests
