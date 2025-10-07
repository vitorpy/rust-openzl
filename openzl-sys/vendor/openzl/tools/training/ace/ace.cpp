// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <memory>
#include <string_view>
#include <vector>

#include "openzl/cpp/Compressor.hpp"
#include "openzl/zl_reflection.h"

#include "custom_parsers/dependency_registration.h"

#include "tools/logger/Logger.h"
#include "tools/training/ace/ace.h"
#include "tools/training/ace/ace_compressor.h"
#include "tools/training/ace/automated_compressor_explorer.h"
#include "tools/training/graph_mutation/graph_mutation_utils.h"
#include "tools/training/sample_collection/training_sample_collector.h"
#include "tools/training/utils/genetic_algorithm.h"
#include "tools/training/utils/utils.h"

namespace openzl::training {

const std::string ACE_GRAPH_NAME = "zl.ace";

namespace {
using namespace openzl::training::graph_mutation;
using namespace openzl::tools::logger;

/**
 * @returns The Pareto-optimal set of compressors for @p samples.
 */
std::vector<std::pair<ACECompressor, ACECompressionResult>> trainBackend(
        std::vector<MultiInput>& samples,
        const TrainParams& trainParams,
        unsigned graphIdx,
        unsigned numGraphs)
{
    if (samples.empty()) {
        return { { buildCompressGenericCompressor(), ACECompressionResult{} } };
    }
    auto flattened = std::vector<Input>();
    for (auto& sample : samples) {
        for (auto& input : *sample) {
            flattened.push_back(InputRef(input.get()));
        }
    }
    poly::optional<std::chrono::seconds> maxTime;
    if (trainParams.maxTimeSecs.has_value()) {
        maxTime = std::chrono::seconds(trainParams.maxTimeSecs.value());
    } else {
        maxTime = poly::nullopt;
    }
    AutomatedCompressorExplorer::Parameters params{
        .numThreads = trainParams.threads.has_value()
                ? trainParams.threads.value()
                : std::thread::hardware_concurrency() / 2,
    };
    params.maxTime = maxTime;
    AutomatedCompressorExplorer ace(flattened, params);
    for (;;) {
        Logger::logProgress(
                INFO,
                ace.progress(),
                "Training ACE graph %u / %u: ACE progress",
                graphIdx,
                numGraphs);
        if (ace.finished()) {
            break;
        }
        ace.step();
    }
    Logger::finalizeProgress(INFO);
    auto solutions = ace.solution();
    if (solutions.empty()) {
        throw Exception("ACE training failed to find a solution");
    }

    std::vector<std::pair<ACECompressor, ACECompressionResult>> result;
    for (auto&& [candidate, _] : solutions) {
        auto benchmark = *candidate.benchmark(flattened);
        result.emplace_back(std::move(candidate), std::move(benchmark));
        if (!trainParams.paretoFrontier) {
            break;
        }
    }
    if (result.empty()) {
        Logger::log(
                WARNINGS,
                "No solution found that meets speed constraints: Falling back to store");
        auto store = buildStoreCompressor();
        return { { store, *store.benchmark(flattened) } };
    }

    // Register the new graph on the compressor and return the new graph ID
    return result;
}

/**
 * @returns A serialized compressor of @p compressor where each backend graph is
 * replaced by the given `ACECompressor`.
 */
std::shared_ptr<const std::string_view> runReplacements(
        Compressor& compressor,
        const std::unordered_map<std::string, ACECompressor>& replacements)
{
    // Add each graph to the compressor
    std::unordered_map<std::string, ZL_GraphID> newGraphIds;
    newGraphIds.reserve(replacements.size());
    for (const auto& [backendGraph, aceCompressor] : replacements) {
        newGraphIds.emplace(backendGraph, aceCompressor.build(compressor));
    }

    // Replace each backend graph with the new GraphID
    std::string serializedForReplacements = compressor.serialize();
    for (const auto& [backendGraph, newGraphId] : newGraphIds) {
        auto result = replaceBaseGraphInCompressor(
                serializedForReplacements,
                backendGraph,
                ZL_Compressor_Graph_getName(compressor.get(), newGraphId));

        serializedForReplacements = std::string(result.begin(), result.end());
    }

    auto json = Compressor::convertSerializedToJson(serializedForReplacements);
    Logger::log(VERBOSE3, "Graph with trained ACE successors: ", json);

    return graph_mutation::createSharedStringView(
            std::move(serializedForReplacements));
}

/**
 * @returns The compressor for each backend graph that has the best ratio, which
 * is just the first compressor because they are sorted by compressed size.
 */
std::unordered_map<std::string, ACECompressor> getSmallestReplacement(
        const std::unordered_map<
                std::string,
                std::vector<std::pair<ACECompressor, ACECompressionResult>>>&
                allCandidates)
{
    std::unordered_map<std::string, ACECompressor> replacements;
    replacements.reserve(allCandidates.size());
    for (const auto& [backendGraph, candidates] : allCandidates) {
        replacements.emplace(backendGraph, candidates[0].first);
    }
    return replacements;
}

/**
 * Searches through the candidates of each backend graph to find a compressor
 * that is at least as fast as @p constraint.
 */
std::unordered_map<std::string, ACECompressor> getReplacementsAsFastAs(
        const std::unordered_map<
                std::string,
                std::vector<std::pair<ACECompressor, ACECompressionResult>>>&
                allCandidates,
        const ACECompressionResult& constraint)
{
    std::unordered_map<std::string, ACECompressor> replacements;
    replacements.reserve(allCandidates.size());
    for (const auto& [backendGraph, candidates] : allCandidates) {
        poly::optional<ACECompressor> replacement;
        for (const auto& [candidate, benchmark] : candidates) {
            if (benchmark.compressionSpeedMBps()
                        >= constraint.compressionSpeedMBps()
                && benchmark.decompressionSpeedMBps()
                        >= constraint.decompressionSpeedMBps()) {
                replacement = candidate;
                break;
            }
        }
        if (replacement.has_value()) {
            replacements.emplace(backendGraph, std::move(*replacement));
        } else {
            replacements.emplace(backendGraph, buildStoreCompressor());
        }
    }

    return replacements;
}

/**
 * Takes the Pareto Frontier of solutions for all sub-compressor, and produces a
 * Pareto-optimal set of solutions for the entire compressor.
 *
 * @note The algorithm used to produce the overall Pareto-optimal set is
 * extremely naive. It was implemented this way due to time pressure, because I
 * don't have the time to develop a more sophisticated algorithm. Ultimately,
 * this is a constraint satisfaction problem.
 */
std::vector<std::shared_ptr<const std::string_view>> combineCandidates(
        const std::function<Compressor()>& makeCompressor,
        const std::unordered_map<
                std::string,
                std::vector<std::pair<ACECompressor, ACECompressionResult>>>&
                allCandidates,
        const std::vector<MultiInput>& inputs)
{
    std::vector<std::shared_ptr<const std::string_view>> results;
    std::vector<std::vector<float>> benchmarks;

    std::vector<poly::span<const Input>> inputSpans;
    inputSpans.reserve(inputs.size());
    for (const auto& input : inputs) {
        inputSpans.push_back(*input);
    }

    size_t numConstraints = 1;
    for (const auto& [_backendGraph, candidates] : allCandidates) {
        numConstraints += candidates.size();
    }

    auto addResultForReplacments =
            [&, index = size_t(0)](
                    const std::unordered_map<std::string, ACECompressor>&
                            replacements) mutable {
                ++index;
                Logger::logProgress(
                        INFO,
                        (double)index / numConstraints,
                        "Computing overall Pareto Frontier: %zu / %zu",
                        index,
                        numConstraints);

                auto replacementCompressor = makeCompressor();
                auto trainedCompressor =
                        runReplacements(replacementCompressor, replacements);

                auto compressor =
                        custom_parsers::createCompressorFromSerialized(
                                *trainedCompressor);

                auto bench = benchmark(*compressor, inputSpans);
                if (!bench.has_value()) {
                    throw std::runtime_error("ACE produced invalid graph");
                }
                if (bench->compressedSize < bench->originalSize + 100) {
                    results.push_back(trainedCompressor);
                    benchmarks.push_back(bench->asFloatVector());
                }
            };

    // Always add the smallest candidate
    addResultForReplacments(getSmallestReplacement(allCandidates));

    // Use each sub-candidate as a constraint. Replace each backend with a graph
    // as fast as that constraint, or store if none exists.
    for (const auto& [_backendGraph, candidates] : allCandidates) {
        for (const auto& [_candidate, constraint] : candidates) {
            addResultForReplacments(
                    getReplacementsAsFastAs(allCandidates, constraint));
        }
    }

    Logger::finalizeProgress(INFO);

    // Then prune down to Pareto-optimal results
    auto paretoFrontier = fastNonDominatedSort(benchmarks).first[0];
    detail::sortByKey(
            paretoFrontier,
            [&](size_t idx) { return benchmarks[idx]; },
            /* reverse */ true);

    std::vector<std::shared_ptr<const std::string_view>> paretoOptimalResults;
    paretoOptimalResults.reserve(paretoFrontier.size());
    for (size_t idx : paretoFrontier) {
        paretoOptimalResults.push_back(results[idx]);
    }

    return paretoOptimalResults;
}

} // namespace

std::vector<std::shared_ptr<const std::string_view>> trainAceCompressor(
        const std::vector<MultiInput>& inputs,
        std::string_view serializedCompressorInput,
        const TrainParams& trainParams)
{
    auto makeCompressor = [&serializedCompressorInput, &trainParams] {
        return std::move(
                *trainParams.compressorGenFunc(serializedCompressorInput));
    };
    auto compressor = makeCompressor();
    auto cctx       = refCCtxForTraining(compressor);

    // We need to create a new serialized compressor because compressor
    // will have different graph IDs from serializedCompressorInput
    std::string serializedUntrainedCompressor        = compressor.serialize();
    const std::vector<std::string> autoBackendGraphs = findAllGraphsWithPrefix(
            serializedUntrainedCompressor, ACE_GRAPH_NAME);

    if (makeCompressor().serialize() != serializedUntrainedCompressor) {
        // HACK: This is not a strong guarantee that the library provides, so
        // make sure to check it. Ultimately we need the ability to clone
        // compressors.
        throw std::logic_error("Deserialization is not determinsitic!");
    }

    Logger::log(
            VERBOSE1,
            "Found ",
            autoBackendGraphs.size(),
            " ACE graphs in compressor");

    auto samples =
            collectInputStreamsForGraphs(inputs, autoBackendGraphs, cctx);

    std::unordered_map<
            std::string,
            std::vector<std::pair<ACECompressor, ACECompressionResult>>>
            candidates;

    size_t graphIdx        = 0;
    const size_t numGraphs = autoBackendGraphs.size();
    for (const auto& backendGraph : autoBackendGraphs) {
        candidates.emplace(
                backendGraph,
                trainBackend(
                        samples[backendGraph],
                        trainParams,
                        ++graphIdx,
                        numGraphs));
    }

    std::vector<std::shared_ptr<const std::string_view>> results;
    if (!trainParams.paretoFrontier) {
        // Each candidate vector has size one ==> no choices to make
        auto replacements = getSmallestReplacement(candidates);
        results.push_back(runReplacements(compressor, replacements));
    } else {
        results = combineCandidates(makeCompressor, candidates, inputs);
    }
    return results;
}
} // namespace openzl::training
