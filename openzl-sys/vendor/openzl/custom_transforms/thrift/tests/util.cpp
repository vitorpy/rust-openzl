// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "custom_transforms/thrift/directed_selector.h"    // @manual
#include "custom_transforms/thrift/empty_input_selector.h" // @manual
#include "custom_transforms/thrift/tests/util.h"           // @manual
#include "custom_transforms/thrift/thrift_parsers.h"       // @manual
#include "openzl/compress/private_nodes.h"                 // @manual
#include "openzl/zl_data.h"
#include "tools/zstrong_cpp.h" // @manual

namespace zstrong::thrift::tests {
namespace {
// TODO(T190725275) Hard-coding the list of paths is brittle: for example, if we
// add a new special ThriftNodeId, we'd need to update the list manually. The
// data model is pretty stable at this point, but it's still unfortunate that we
// have this tight coupling between the config generator and the test schema. I
// have a plan to improve this in the near future: T171457232.
//
// For the short term, as long as we check this list against the TestStruct
// schema in code review, our coverage should be fine. The schema can be found
// at https://fburl.com/code/chgygv3s.
std::vector<std::pair<ThriftPath, TType>> getAllPaths()
{
    std::vector<std::pair<ThriftPath, TType>> suffixes = {
        { { ThriftNodeId(-1) }, TType::T_BYTE },
        { { ThriftNodeId(-2) }, TType::T_BOOL },
        { { ThriftNodeId(1) }, TType::T_I16 },
        { { ThriftNodeId(42) }, TType::T_I32 },
        { { ThriftNodeId(2) }, TType::T_I64 },
        { { ThriftNodeId(3) }, TType::T_FLOAT },
        { { ThriftNodeId(4) }, TType::T_DOUBLE },
        { { ThriftNodeId(5) }, TType::T_STRING },
        { { ThriftNodeId(5), ThriftNodeId::kLength }, TType::T_U32 },
        { { ThriftNodeId(6), ThriftNodeId::kListElem }, TType::T_BOOL },
        { { ThriftNodeId(6), ThriftNodeId::kLength }, TType::T_U32 },
        { { ThriftNodeId(7), ThriftNodeId::kMapKey }, TType::T_STRING },
        { { ThriftNodeId(7), ThriftNodeId::kMapValue }, TType::T_BOOL },
        { { ThriftNodeId(7), ThriftNodeId::kLength }, TType::T_U32 },
        { { ThriftNodeId(7), ThriftNodeId::kMapKey, ThriftNodeId::kLength },
          TType::T_U32 },
        { { ThriftNodeId(8), ThriftNodeId::kListElem }, TType::T_I32 },
        { { ThriftNodeId(8), ThriftNodeId::kLength }, TType::T_U32 },
        { { ThriftNodeId(-123) }, TType::T_FLOAT }, // include a fake path
    };

    std::vector<ThriftPath> prefixes = {
        { ThriftNodeId(4) },
        { ThriftNodeId(3) },
        { ThriftNodeId(2), ThriftNodeId::kListElem },
        { ThriftNodeId(1), ThriftNodeId::kMapKey },
        { ThriftNodeId(1), ThriftNodeId::kMapValue },
    };

    std::vector<std::pair<ThriftPath, TType>> result;
    result.reserve(prefixes.size() * suffixes.size());
    for (const auto& prefix : prefixes) {
        for (const auto& [innerPath, innerType] : suffixes) {
            auto path = prefix;
            path.insert(path.end(), innerPath.begin(), innerPath.end());
            result.emplace_back(std::move(path), innerType);
        }
    }

    return result;
}

// Work around the annoying kLength bug
std::vector<std::pair<ThriftPath, TType>> purgeLengthOnlySplits(
        const std::vector<std::pair<ThriftPath, TType>>& paths)
{
    std::set<ThriftPath> dataPrefixes;
    for (auto& [path, _] : paths) {
        if (!path.empty() && path.back() != ThriftNodeId::kLength) {
            ThriftPath prefix(path.begin(), path.end() - 1);
            dataPrefixes.insert(std::move(prefix));
            dataPrefixes.insert(path);
        }
    }

    std::decay_t<decltype(paths)> result;
    result.reserve(paths.size());
    for (auto& [path, type] : paths) {
        if (path.empty()) {
            continue;
        }
        if (path.back() == ThriftNodeId::kLength) {
            const ThriftPath prefix(path.begin(), path.end() - 1);
            if (!dataPrefixes.contains(prefix)) {
                // It's illegal to split lengths without data
                continue;
            }
        }
        result.emplace_back(path, type);
    }
    return result;
}

// Support for string length paths is removed in format version 14
std::vector<std::pair<ThriftPath, TType>> purgeStringLengthPaths(
        const std::vector<std::pair<ThriftPath, TType>>& paths)
{
    std::set<ThriftPath> stringVsfPaths;
    for (auto& [path, type] : paths) {
        if (type == TType::T_STRING) {
            stringVsfPaths.insert(path);
        }
    }

    std::decay_t<decltype(paths)> result;
    result.reserve(paths.size());
    for (auto& [path, type] : paths) {
        if (path.empty()) {
            continue;
        }
        if (path.back() == ThriftNodeId::kLength) {
            const ThriftPath prefix(path.begin(), path.end() - 1);
            if (stringVsfPaths.contains(prefix)) {
                // Lengths are already covered by the corresponding VSF path
                continue;
            }
        }
        result.emplace_back(path, type);
    }
    return result;
}
} // namespace

std::string thriftSplitCompress(
        const ZL_VOEncoderDesc& compress,
        std::string_view src,
        std::string_view serializedConfig,
        const int formatVersion)
{
    // TODO(T193417384) Clean this up, share a single Thrift graph creation
    // function with Managed Compression

    /* prepare graph for compression */
    zstrong::CGraph cgraph;
    cgraph.unwrap(ZL_Compressor_setParameter(
            cgraph.get(), ZL_CParam_formatVersion, formatVersion));
    cgraph.unwrap(ZL_Compressor_setParameter(
            cgraph.get(), ZL_CParam_minStreamSize, -1));
    const ZL_CopyParam gp            = { .paramId   = 0,
                                         .paramPtr  = serializedConfig.data(),
                                         .paramSize = serializedConfig.size() };
    const ZL_LocalParams localParams = { .copyParams = { .copyParams   = &gp,
                                                         .nbCopyParams = 1 } };
    const ZL_NodeID nodeWithoutParams =
            ZL_Compressor_registerVOEncoder(cgraph.get(), &compress);
    const ZL_NodeID nodeWithParams = ZL_Compressor_cloneNode(
            cgraph.get(), nodeWithoutParams, &localParams);

    std::vector<ZL_GraphID> thriftSuccessors;
    for (size_t i = 0; i < compress.gd.nbSingletons; i++) {
        thriftSuccessors.push_back(ZL_GRAPH_STORE);
    }

    for (ZL_Type type : std::array<ZL_Type, 3>{
                 ZL_Type_serial, ZL_Type_numeric, ZL_Type_string }) {
        const std::vector<ZL_GraphID> directedSelectorSuccessors{
            { ZL_GRAPH_STORE }
        };
        const ZL_SelectorDesc directed_selector_desc =
                buildDirectedSelectorDesc(
                        type,
                        directedSelectorSuccessors.data(),
                        directedSelectorSuccessors.size());
        const ZL_GraphID directedSelectorGraphID =
                ZL_Compressor_registerSelectorGraph(
                        cgraph.get(), &directed_selector_desc);
        assert(directedSelectorGraphID.gid != ZL_GRAPH_ILLEGAL.gid);
        const std::vector<ZL_GraphID> emptyInputSelectorSuccessors{
            { ZL_GRAPH_STORE, directedSelectorGraphID }
        };
        const ZL_SelectorDesc empty_input_selector_desc =
                buildEmptyInputSelectorDesc(
                        type,
                        emptyInputSelectorSuccessors.data(),
                        emptyInputSelectorSuccessors.size());
        const ZL_GraphID emptyInputSelectorGraphID =
                ZL_Compressor_registerSelectorGraph(
                        cgraph.get(), &empty_input_selector_desc);
        assert(emptyInputSelectorGraphID.gid != ZL_GRAPH_ILLEGAL.gid);
        thriftSuccessors.push_back(emptyInputSelectorGraphID);
    }

    // Successor for cluster lengths
    thriftSuccessors.push_back(
            ZL_Compressor_registerFieldLZGraph(cgraph.get()));

    ZL_GraphID const startingFnode = ZL_Compressor_registerStaticGraph_fromNode(
            cgraph.get(),
            nodeWithParams,
            thriftSuccessors.data(),
            thriftSuccessors.size());
    assert(startingFnode.gid != ZL_GRAPH_ILLEGAL.gid);
    cgraph.unwrap(
            ZL_Compressor_selectStartingGraphID(cgraph.get(), startingFnode));

    /* compress data */
    size_t const bound =
            thriftSplitCompressBound(src.size(), serializedConfig.size());

    /* use new[] so we don't have to initialize memory */
    std::unique_ptr<uint8_t[]> dst(new uint8_t[bound]);
    zstrong::CCtx cctx;
    cctx.unwrap(ZL_CCtx_refCompressor(cctx.get(), cgraph.get()));
    size_t const dstSize = cctx.unwrap(ZL_CCtx_compress(
            cctx.get(), dst.get(), bound, src.data(), src.size()));
    std::string result((const char*)dst.get(), dstSize);
    return result;
}

void thriftSplitDecompress(
        std::string_view compressed,
        std::optional<std::string_view> original)
{
    // works for TCompact, may have to update for TBinary
    size_t const decompressBound =
            std::min<size_t>(10 << 20, compressed.size() * 100);

    /* decompress data */
    std::unique_ptr<uint8_t[]> val(new uint8_t[decompressBound]);
    zstrong::DCtx dctx;
    dctx.unwrap(zstrong::thrift::registerCustomTransforms(dctx.get()));
    dctx.unwrap(ZL_DCtx_decompress(
            dctx.get(),
            val.get(),
            decompressBound,
            compressed.data(),
            compressed.size()));

    if (original.has_value()) {
        /* validate that reconstructed data matches original */
        if (!std::equal(
                    original->begin(),
                    original->end(),
                    (const char*)val.get())) {
            throw std::runtime_error(
                    "Round trip test failed, data corruption detected!");
        }
    }
}

void runThriftSplitterRoundTrip(
        const ZL_VOEncoderDesc& compress,
        std::string_view src,
        std::string_view serializedConfig,
        int const minFormatVersion,
        int const maxFormatVersion)
{
    for (int formatVersion = minFormatVersion;
         formatVersion <= maxFormatVersion;
         ++formatVersion) {
        std::string compressed = thriftSplitCompress(
                compress, src, serializedConfig, formatVersion);
        thriftSplitDecompress(compressed, src);
    }
}

std::string buildValidEncoderConfig(
        int minFormatVersion,
        int seed,
        ConfigGenMode mode,
        int maxFormatVersion)
{
    std::mt19937 gen(seed);
    EncoderConfigBuilder builder;

    // Add a random subset of possible paths to the config
    {
        auto paths = getAllPaths();
        std::shuffle(paths.begin(), paths.end(), gen);
        const double fractionOfPathsToUse = mode == ConfigGenMode::kMoreCoverage
                ? 0.5
                : std::uniform_real_distribution<double>(0.0, 1.0)(gen);
        paths.resize(size_t(fractionOfPathsToUse * paths.size()));
        paths = purgeLengthOnlySplits(paths);
        if (maxFormatVersion >= kMinFormatVersionStringVSF) {
            paths = purgeStringLengthPaths(paths);
        }
        for (const auto& [path, type] : paths) {
            builder.addPath(path, type);
        }
    }

    // Cluster a random subset of paths
    std::set<ThriftPath> clusteredPaths{};
    if (minFormatVersion >= kMinFormatVersionEncodeClusters) {
        folly::F14FastMap<TType, size_t> clusterIndices;
        clusterIndices.reserve(size_t(TType::T_FLOAT));
        for (const auto& [_, pathInfo] : builder.pathMap()) {
            if (!clusterIndices.contains(pathInfo.type)) {
                clusterIndices.emplace(
                        pathInfo.type, builder.addEmptyCluster(0));
            }
        }

        std::bernoulli_distribution coin(0.5);
        for (const auto& [path, pathInfo] : builder.pathMap()) {
            if (coin(gen)) {
                const size_t clusterIdx = clusterIndices.at(pathInfo.type);
                builder.addPathToCluster(path, clusterIdx);
                clusteredPaths.insert(path);
            }
        }
    }

    // Set successors for type split defaults
    std::discrete_distribution<> dist({ 10, 25, 10, 10, 10, 10, 25 });
    std::vector<ZL_Type> streamTypes{
        ZL_Type_serial, ZL_Type_string, ZL_Type_numeric, ZL_Type_struct
    };
    for (auto const& type : streamTypes) {
        builder.setSuccessorForType(type, dist(gen));
    }

    // Set successors for unclustered paths
    for (auto const& [path, _] : builder.pathMap()) {
        if (!clusteredPaths.contains(path)) {
            builder.setSuccessorForPath(path, 0);
        }
    }

    // Note: deletes empty clusters from the config
    const std::string result = builder.finalize();

    // Assert that the default params yield high coverage
    if (seed == kDefaultConfigSeed && mode == ConfigGenMode::kMoreCoverage) {
        assert(builder.pathMap().size() >= 20);

        if (minFormatVersion >= kMinFormatVersionEncodeClusters) {
            assert(!builder.clusters().empty());
            assert(builder.clusters().front().idList.size() >= 2);

            folly::F14FastSet<TType> clusteredTypes;
            for (int i = 0; i < builder.clusters().size(); ++i) {
                clusteredTypes.insert(builder.getClusterType(i));
            }
            assert(clusteredTypes.size() >= 5);
        }
    }

    return result;
}; // namespace

} // namespace zstrong::thrift::tests
