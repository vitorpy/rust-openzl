// Copyright (c) Meta Platforms, Inc. and affiliates.

#pragma once

#include "openzl/codecs/zl_split.h"
#include "openzl/cpp/Compressor.hpp"
#include "openzl/cpp/FunctionGraph.hpp"
#include "openzl/cpp/codecs/Metadata.hpp"
#include "openzl/cpp/codecs/Node.hpp"

namespace openzl {
namespace nodes {
namespace detail {
constexpr NodeMetadata<1, 0, 1> splitMetadata(Type type)
{
    return {
        .inputs          = { InputMetadata{ .type = type } },
        .variableOutputs = { OutputMetadata{ .type = type,
                                             .name = "segments" } },
        .description =
                "Split the input into N segments according to the given `segmentSizes`"
    };
}
} // namespace detail

class Split : public Node {
   public:
    explicit Split(poly::span<const size_t> segmentSizes)
            : segmentSizes_(segmentSizes)
    {
    }

    NodeID baseNode() const override
    {
        throw Exception("Split: Can only call run()");
    }

    Edge::RunNodeResult run(Edge& edge) const override
    {
        auto edges = unwrap(ZL_Edge_runSplitNode(
                edge.get(), segmentSizes_.data(), segmentSizes_.size()));
        return Edge::convert(edges);
    }

    ~Split() override = default;

   private:
    poly::span<const size_t> segmentSizes_;
};

class SplitSerial : public Split {
   public:
    static constexpr NodeMetadata<1, 0, 1> metadata =
            detail::splitMetadata(Type::Serial);

    using Split::Split;
};

class SplitNumeric : public Split {
   public:
    static constexpr NodeMetadata<1, 0, 1> metadata =
            detail::splitMetadata(Type::Numeric);

    using Split::Split;
};

class SplitStruct : public Split {
   public:
    static constexpr NodeMetadata<1, 0, 1> metadata =
            detail::splitMetadata(Type::Struct);

    using Split::Split;
};

class SplitString : public Split {
   public:
    static constexpr NodeMetadata<1, 0, 1> metadata =
            detail::splitMetadata(Type::String);

    using Split::Split;
};
} // namespace nodes
namespace graphs {
struct Split {
    // TODO(terrelln): Make the split graph serializable by switching to a
    // dynamic graph.
    // - Call the correct split node based on the type
    // - Pass in the successors as customGraphs

    // GraphID operator()(
    //         Type type,
    //         poly::span<const size_t> segmentSizes,
    //         poly::span<const GraphID> successors) const
    // {
    // }
};
constexpr Split split;
} // namespace graphs
} // namespace openzl
