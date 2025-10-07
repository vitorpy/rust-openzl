// Copyright (c) Meta Platforms, Inc. and affiliates.

#pragma once

#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "openzl/openzl.hpp"

namespace openzl {
namespace tests {

class AssertEqException : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

class AssertEqFunctionGraph : public FunctionGraph {
   public:
    AssertEqFunctionGraph(const Input& expected) : expected_(expected) {}

    FunctionGraphDescription functionGraphDescription() const override
    {
        return FunctionGraphDescription{
            .name           = "AssertEq",
            .inputTypeMasks = { TypeMask::Any },
        };
    }

    void graph(GraphState& state) const override
    {
        auto& edge        = state.edges()[0];
        const auto& input = edge.getInput();

        if (input != expected_) {
            throw AssertEqException("Input does not match expectations");
        }
        edge.setDestination(graphs::Compress{}());
    }

   private:
    const Input& expected_;
};

class CodecTest : public testing::Test {
    void testCodecImpl(
            NodeID node,
            poly::span<const Input> input,
            const std::vector<const Input*>& expectedOutputs,
            int formatVersion)
    {
        compressor_.setParameter(CParam::FormatVersion, formatVersion);
        std::vector<GraphID> successors;
        successors.reserve(expectedOutputs.size());
        for (const auto& expectedOutput : expectedOutputs) {
            if (expectedOutput != nullptr) {
                successors.push_back(compressor_.registerFunctionGraph(
                        std::make_shared<AssertEqFunctionGraph>(
                                *expectedOutput)));
            } else {
                successors.push_back(graphs::Compress{}());
            }
        }
        auto graph = compressor_.buildStaticGraph(node, successors);
        compressor_.selectStartingGraph(graph);

        testRoundTrip(input);
    }

   public:
    void SetUp() override
    {
        compressor_ = Compressor();
        compressor_.setParameter(CParam::MinStreamSize, -1);

        cctx_ = CCtx();
        dctx_ = DCtx();
    }

    /**
     * Tests @p node on @p input, expecting @p expectedOutput outputs from the
     * node for each format version between @p minFormatVersion and
     * @p maxFormatVersion.
     */
    void testCodec(
            NodeID node,
            const Input& input,
            const std::vector<const Input*>& expectedOutputs,
            int minFormatVersion,
            int maxFormatVersion = ZL_MAX_FORMAT_VERSION)
    {
        testCodec(
                node,
                { &input, 1 },
                { expectedOutputs },
                minFormatVersion,
                maxFormatVersion);
    }

    /**
     * Tests @p node on @p input, expecting @p expectedOutput outputs from the
     * node for each format version between @p minFormatVersion and
     * @p maxFormatVersion.
     */
    void testCodec(
            NodeID node,
            poly::span<const Input> input,
            const std::vector<const Input*>& expectedOutputs,
            int minFormatVersion,
            int maxFormatVersion = ZL_MAX_FORMAT_VERSION)
    {
        for (int formatVersion =
                     std::max<int>(minFormatVersion, ZL_MIN_FORMAT_VERSION);
             formatVersion <= maxFormatVersion;
             ++formatVersion) {
            testCodecImpl(node, input, expectedOutputs, formatVersion);
        }
    }

    /// Tests that @p input round trips with the compressor_, cctx_, and dctx_.
    std::string testRoundTrip(poly::span<const Input> input)
    {
        cctx_.refCompressor(compressor_);
        auto compressed   = cctx_.compress(input);
        auto roundTripped = dctx_.decompress(compressed);
        EXPECT_EQ(roundTripped.size(), input.size());
        for (size_t i = 0; i < input.size(); ++i) {
            EXPECT_EQ(roundTripped[i], input[i]);
        }
        return compressed;
    }

    /// Tests that @p input round trips with the compressor_, cctx_, and dctx_.
    std::string testRoundTrip(const Input& input)
    {
        return testRoundTrip({ &input, 1 });
    }

    /// Tests that @p input round trips with the compressor_, cctx_, and dctx_.
    std::string testRoundTrip(poly::string_view input)
    {
        return testRoundTrip(Input::refSerial(input));
    }

    Compressor compressor_;
    CCtx cctx_;
    DCtx dctx_;
};

} // namespace tests
} // namespace openzl
