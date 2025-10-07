// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "tools/training/ace/ace_compressors.h"

#include "openzl/openzl.hpp"
#include "openzl/zl_reflection.h"
#include "tools/training/ace/ace_sampling.h"

namespace openzl {
namespace training {
namespace {
std::string getName(GraphID graph)
{
    Compressor compressor;
    auto name = ZL_Compressor_Graph_getName(compressor.get(), graph);
    if (name == nullptr) {
        throw Exception("Unknown graph!");
    }
    return name;
}

std::string getName(NodeID node)
{
    Compressor compressor;
    auto name = ZL_Compressor_Node_getName(compressor.get(), node);
    if (name == nullptr) {
        throw Exception("Unknown node!");
    }
    return name;
}

template <typename NodeT>
ACENode buildNode(const NodeT& node)
{
    assert(NodeT::metadata.inputs.size() == 1);
    std::vector<Type> outputTypes;
    outputTypes.reserve(
            NodeT::metadata.singletonOutputs.size()
            + NodeT::metadata.variableOutputs.size());
    for (const auto& meta : NodeT::metadata.singletonOutputs) {
        outputTypes.push_back(meta.type);
    }
    for (const auto& meta : NodeT::metadata.variableOutputs) {
        outputTypes.push_back(meta.type);
    }
    return ACENode{
        .name        = getName(NodeT::node),
        .params      = node.parameters(),
        .inputType   = NodeT::metadata.inputs[0].type,
        .outputTypes = std::move(outputTypes),
    };
}

template <typename GraphT>
ACEGraph buildGraph(const GraphT& graph)
{
    static_assert(GraphT::metadata.inputs.size() == 1);
    return ACEGraph{
        .name          = getName(GraphT::graph),
        .params        = graph.parameters(),
        .inputTypeMask = GraphT::metadata.inputs[0].typeMask,
    };
}

std::vector<ACENode> makeAllNodes()
{
    std::vector<ACENode> n;
    n.push_back(buildNode(nodes::DeltaInt{}));
    n.push_back(buildNode(nodes::TokenizeStruct{}));
    for (auto sorted : { true, false }) {
        n.push_back(buildNode(nodes::TokenizeNumeric{ sorted }));
        n.push_back(buildNode(nodes::TokenizeString{ sorted }));
    }
    for (int numBits = 1; numBits < 64; ++numBits) {
        n.push_back(buildNode(nodes::Bitunpack{ numBits }));
    }
    n.push_back(buildNode(nodes::DivideBy{}));
    n.push_back(buildNode(nodes::RangePack{}));
    for (int level = 0; level < 10; ++level) {
        n.push_back(buildNode(nodes::FieldLz{ level }));
    }
    n.push_back(buildNode(nodes::Float32Deconstruct{}));
    n.push_back(buildNode(nodes::Float16Deconstruct{}));
    n.push_back(buildNode(nodes::BFloat16Deconstruct{}));
    n.push_back(buildNode(nodes::MergeSorted{}));
    n.push_back(buildNode(nodes::ParseInt{}));
    n.push_back(buildNode(nodes::Prefix{}));
    n.push_back(buildNode(nodes::QuantizeOffsets{}));
    n.push_back(buildNode(nodes::QuantizeLengths{}));
    n.push_back(buildNode(nodes::TransposeSplit{}));
    n.push_back(buildNode(nodes::Zigzag{}));
    n.push_back(buildNode(nodes::ConvertSerialToNum8{}));
    n.push_back(buildNode(nodes::ConvertSerialToNumLE16{}));
    n.push_back(buildNode(nodes::ConvertSerialToNumLE32{}));
    n.push_back(buildNode(nodes::ConvertSerialToNumLE64{}));
    n.push_back(buildNode(nodes::ConvertSerialToNumBE16{}));
    n.push_back(buildNode(nodes::ConvertSerialToNumBE32{}));
    n.push_back(buildNode(nodes::ConvertSerialToNumBE64{}));
    for (int width = 1; width <= 32; ++width) {
        n.push_back(buildNode(nodes::ConvertSerialToStruct{ width }));
    }
    n.push_back(buildNode(nodes::ConvertStructToNumLE{}));
    n.push_back(buildNode(nodes::ConvertStructToNumBE{}));
    n.push_back(buildNode(nodes::SeparateStringComponents{}));
    return n;
}

std::vector<ACEGraph> makeAllGraphs()
{
    std::vector<ACEGraph> g;
    g.push_back(buildGraph(graphs::Compress{}));
    g.push_back(buildGraph(graphs::Entropy{}));
    g.push_back(buildGraph(graphs::Bitpack{}));
    g.push_back(buildGraph(graphs::Constant{}));
    for (int level = 0; level < 10; ++level) {
        g.push_back(buildGraph(graphs::FieldLz{ level }));
    }
    for (int level = -5; level < 10; ++level) {
        g.push_back(buildGraph(graphs::Zstd{ level }));
    }
    g.push_back(buildGraph(graphs::Flatpack{}));
    g.push_back(buildGraph(graphs::Store{}));
    return g;
}

std::vector<ACECompressor> makePrebuiltNumericCompressors()
{
    std::vector<ACECompressor> compressors;
    auto graphs = getGraphsComptabileWith(Type::Numeric);
    compressors.reserve(graphs.size());
    for (const auto& graph : graphs) {
        compressors.emplace_back(graph);
    }

    ACECompressor fieldLz(buildGraph(graphs::FieldLz{}));
    ACECompressor zstd(buildGraph(graphs::Zstd{}));
    ACECompressor transpose(buildNode(nodes::TransposeSplit{}), { zstd });
    ACECompressor deltaFieldLz(buildNode(nodes::DeltaInt{}), { fieldLz });
    ACECompressor deltaTranspose(buildNode(nodes::DeltaInt{}), { transpose });
    ACECompressor tokenizeSortedFieldLz(
            buildNode(nodes::TokenizeNumeric{ true }),
            { deltaFieldLz, fieldLz });
    ACECompressor tokenizeFieldLz(
            buildNode(nodes::TokenizeNumeric{ false }),
            { deltaFieldLz, fieldLz });
    ACECompressor zigzagFieldLz(buildNode(nodes::Zigzag{}), { fieldLz });
    ACECompressor deltaZigzagFieldLz(
            buildNode(nodes::DeltaInt{}), { zigzagFieldLz });
    ACECompressor rangePackFieldLz(buildNode(nodes::RangePack{}), { fieldLz });
    ACECompressor rangePackDeltaFieldLz(
            buildNode(nodes::RangePack{}), { deltaFieldLz });
    ACECompressor fse(buildGraph(graphs::Fse{}));
    ACECompressor store(buildGraph(graphs::Store{}));
    ACECompressor quantizeOffsets(
            buildNode(nodes::QuantizeOffsets{}), { fse, store });
    ACECompressor quantizeLengths(
            buildNode(nodes::QuantizeOffsets{}), { fse, store });
    std::vector<ACECompressor> prebuilt = {
        fieldLz,
        zstd,
        transpose,
        deltaFieldLz,
        deltaTranspose,
        tokenizeSortedFieldLz,
        tokenizeFieldLz,
        zigzagFieldLz,
        deltaZigzagFieldLz,
        rangePackFieldLz,
        rangePackDeltaFieldLz,
        quantizeOffsets,
        quantizeLengths,
    };
    compressors.insert(compressors.end(), prebuilt.begin(), prebuilt.end());
    return compressors;
}

std::vector<ACECompressor> makePrebuiltStructCompressors()
{
    std::vector<ACECompressor> compressors;
    auto graphs = getGraphsComptabileWith(Type::Struct);
    compressors.reserve(graphs.size());

    ACECompressor fieldLz(buildGraph(graphs::FieldLz{}));
    ACECompressor zstd(buildGraph(graphs::Zstd{}));
    ACECompressor transpose(buildNode(nodes::TransposeSplit{}), { zstd });
    ACECompressor tokenizeFieldLz(
            buildNode(nodes::TokenizeStruct{}), { transpose, fieldLz });

    compressors.push_back(zstd);
    compressors.push_back(transpose);
    compressors.push_back(tokenizeFieldLz);

    for (const auto& compressor : makePrebuiltNumericCompressors()) {
        compressors.push_back(ACECompressor(
                buildNode(nodes::ConvertStructToNumLE{}), { compressor }));
        compressors.push_back(ACECompressor(
                buildNode(nodes::ConvertStructToNumBE{}), { compressor }));
    }
    return compressors;
}

std::vector<ACECompressor> makePrebuiltSerialCompressors()
{
    std::vector<ACECompressor> compressors;
    auto graphs = getGraphsComptabileWith(Type::Serial);
    compressors.reserve(graphs.size());
    for (const auto& graph : graphs) {
        compressors.emplace_back(graph);
    }

    for (const auto& compressor : makePrebuiltNumericCompressors()) {
        compressors.push_back(ACECompressor(
                buildNode(nodes::ConvertSerialToNum8{}), { compressor }));
        compressors.push_back(ACECompressor(
                buildNode(nodes::ConvertSerialToNumLE16{}), { compressor }));
        compressors.push_back(ACECompressor(
                buildNode(nodes::ConvertSerialToNumBE16{}), { compressor }));
        compressors.push_back(ACECompressor(
                buildNode(nodes::ConvertSerialToNumLE32{}), { compressor }));
        compressors.push_back(ACECompressor(
                buildNode(nodes::ConvertSerialToNumBE32{}), { compressor }));
        compressors.push_back(ACECompressor(
                buildNode(nodes::ConvertSerialToNumLE64{}), { compressor }));
        compressors.push_back(ACECompressor(
                buildNode(nodes::ConvertSerialToNumBE64{}), { compressor }));
    }

    return compressors;
}

std::vector<ACECompressor> makePrebuiltStringCompressors()
{
    std::vector<ACECompressor> compressors;
    auto graphs = getGraphsComptabileWith(Type::String);
    compressors.reserve(graphs.size());
    for (const auto& graph : graphs) {
        compressors.emplace_back(graph);
    }

    ACECompressor fieldLz(buildGraph(graphs::FieldLz{}));
    ACECompressor zstd(buildGraph(graphs::Zstd{}));
    ACECompressor separate(
            buildNode(nodes::SeparateStringComponents{}), { zstd, fieldLz });
    ACECompressor prefix(buildNode(nodes::Prefix{}), { separate, fieldLz });
    ACECompressor tokenizeSorted(
            buildNode(nodes::TokenizeString{ true }), { prefix, fieldLz });
    ACECompressor tokenize(
            buildNode(nodes::TokenizeString{ false }), { separate, fieldLz });

    compressors.push_back(separate);
    compressors.push_back(prefix);
    compressors.push_back(tokenizeSorted);
    compressors.push_back(tokenize);

    return compressors;
}

std::vector<ACECompressor> makePrebuiltCompressors(Type inputType)
{
    switch (inputType) {
        case Type::Serial:
            return makePrebuiltSerialCompressors();
        case Type::Struct:
            return makePrebuiltStructCompressors();
        case Type::Numeric:
            return makePrebuiltNumericCompressors();
        case Type::String:
            return makePrebuiltStringCompressors();
        default:
            assert(false);
            return {};
    }
}
} // namespace

poly::span<const ACENode> getAllNodes()
{
    static auto nodes = new std::vector<ACENode>(makeAllNodes());
    return *nodes;
}

poly::span<const ACEGraph> getAllGraphs()
{
    static auto graphs = new std::vector<ACEGraph>(makeAllGraphs());
    return *graphs;
}

poly::span<const ACENode> getNodesComptabileWith(Type inputType)
{
    static auto nodes = new std::unordered_map<Type, std::vector<ACENode>>([] {
        std::unordered_map<Type, std::vector<ACENode>> m;
        for (auto t :
             { Type::Serial, Type::Struct, Type::Numeric, Type::String }) {
            for (const auto& n : getAllNodes()) {
                if (isCompatible(n.inputType, t)) {
                    m[t].push_back(n);
                }
            }
        }
        return m;
    }());
    return nodes->at(inputType);
}

poly::span<const ACEGraph> getGraphsComptabileWith(Type inputType)
{
    static auto graphs =
            new std::unordered_map<Type, std::vector<ACEGraph>>([] {
                std::unordered_map<Type, std::vector<ACEGraph>> m;
                for (auto t : { Type::Serial,
                                Type::Struct,
                                Type::Numeric,
                                Type::String }) {
                    for (const auto& g : getAllGraphs()) {
                        if (isCompatible(g.inputTypeMask, t)) {
                            m[t].push_back(g);
                        }
                    }
                }
                return m;
            }());
    return graphs->at(inputType);
}

poly::span<const ACECompressor> getPrebuiltCompressors(Type inputType)
{
    static auto compressors =
            new std::unordered_map<Type, std::vector<ACECompressor>>([] {
                std::unordered_map<Type, std::vector<ACECompressor>> m;
                for (auto t : { Type::Serial,
                                Type::Struct,
                                Type::Numeric,
                                Type::String }) {
                    m[t] = makePrebuiltCompressors(t);
                }
                return m;
            }());
    return compressors->at(inputType);
}

ACECompressor buildRandomGraphCompressor(std::mt19937_64& rng, Type inputType)
{
    return ACEGraphCompressor(
            randomChoice(rng, getGraphsComptabileWith(inputType)));
}

ACECompressor
buildRandomNodeCompressor(std::mt19937_64& rng, Type inputType, size_t maxDepth)
{
    if (maxDepth == 0) {
        return buildRandomGraphCompressor(rng, inputType);
    }
    auto node = randomChoice(rng, getNodesComptabileWith(inputType));
    assert(isCompatible(node.inputType, inputType));
    std::vector<std::unique_ptr<ACECompressor>> successors;
    successors.reserve(node.outputTypes.size());
    for (size_t i = 0; i < node.outputTypes.size(); ++i) {
        successors.push_back(std::make_unique<ACECompressor>(
                buildRandomCompressor(rng, node.outputTypes[i], maxDepth - 1)));
    }
    return ACENodeCompressor(std::move(node), std::move(successors));
}

ACECompressor
buildRandomCompressor(std::mt19937_64& rng, Type inputType, size_t maxDepth)
{
    std::bernoulli_distribution dist(0.5);
    if (dist(rng)) {
        auto compressor = buildRandomGraphCompressor(rng, inputType);
        assert(compressor.acceptsInputType(inputType));
        return compressor;
    } else {
        auto compressor =
                buildRandomNodeCompressor(rng, inputType, maxDepth - 1);
        assert(compressor.acceptsInputType(inputType));
        return compressor;
    }
}

ACECompressor buildStoreCompressor()
{
    return ACEGraphCompressor(buildGraph(graphs::Store{}));
}

ACECompressor buildCompressGenericCompressor()
{
    return ACEGraphCompressor(buildGraph(graphs::Compress{}));
}

} // namespace training
} // namespace openzl
