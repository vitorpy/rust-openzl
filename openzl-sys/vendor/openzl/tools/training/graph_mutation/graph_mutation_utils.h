// Copyright (c) Meta Platforms, Inc. and affiliates.

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "openzl/common/allocation.h"
#include "openzl/cpp/Compressor.hpp"
#include "openzl/shared/a1cbor.h"
#include "openzl/zl_localParams.h"

namespace openzl::training::graph_mutation {

/**
 * @brief Creates a shared_ptr to a string_view from a string.
 *
 * This utility function creates a shared_ptr to a string_view that references
 * the provided string. The string is stored in a bundle object that is managed
 * by the shared_ptr, ensuring that the string_view remains valid as long as
 * the shared_ptr exists.
 *
 * @param str The string to create a string_view from.
 * @return std::shared_ptr<const std::string_view> A shared_ptr to a string_view
 * that references the provided string.
 */
std::shared_ptr<const std::string_view> createSharedStringView(std::string str);

/**
 * @brief Extracts successor graph names from a CBOR string for a specified
 * graph.
 *
 * @param cborStr The CBOR string to parse.
 * @param graphName The exact name of the graph whose successors
 * to extract.
 * @return A vector of successor graph names.
 */
std::vector<std::string> extractSuccessorsFromCbor(
        const std::string_view& cborStr,
        const std::string& graphName);

/**
 * @brief Extracts node names from a CBOR string for a specified
 * graph.
 *
 * @param cborStr The CBOR string to parse.
 * @param targetGraphPrefix The base name of the graph whose successors
 * to extract.
 * @return A vector of successor graph names.
 */
std::vector<std::string> extractNodesFromCbor(
        const std::string_view& cborStr,
        const std::string& targetGraphPrefix);

/**
 * @brief Replaces a graph's parameters in a compressor with new local
 * parameters.
 *
 * This function modifies the compressor by updating the specified graph with
 * new local parameters. It serializes the compressor, modifies the CBOR
 * representation, and then returns the serialized modified compressor.
 *
 * @param serializedCompressor The serialized compressor to modify.
 * @param localParams The new local parameters to apply.
 * @param graphName The name of the graph to modify.
 * @return std::shared_ptr<const std::string_view> The serialized modified
 * compressor as CBOR data.
 */
std::shared_ptr<const std::string_view> replaceGraphInCompressor(
        std::string_view serializedCompressor,
        const ZL_LocalParams& localParams,
        const std::string& graphName);

/**
 * @brief Checks if a compressor contains a graph with a specific prefix.
 */
bool hasTargetGraph(
        const Compressor& compressor,
        poly::string_view targetGraphPrefix);

/**
 * @brief Renames a graph throughout the compressor by updating all references
 * and the start field.
 *
 * This function performs a comprehensive rename of a graph by:
 * 1. Updating all references to the old graph name in other graphs' dependency
 * arrays
 * 2. Updating the compressor's start field if it points to the old graph name
 *
 * Note: This function does not rename the actual graph definition key - it
 * assumes the graph has already been renamed at the definition level and only
 * updates references.
 *
 * @param serializedCompressor The serialized compressor data to modify
 * @param oldGraphName The current name of the graph to rename references from
 * @param newGraphName The new name to use in all references
 * @return std::string The updated serialized compressor with renamed references
 * @throws std::runtime_error If the old graph cannot be found or update
 * operations fail
 */
std::string renameGraphInCompressor(
        const std::string_view serializedCompressor,
        std::string_view oldGraphName,
        std::string_view newGraphName);

/**
 * @brief Replaces the base graph of a specific parameterized graph.
 *
 * This function updates the "base" field of a specific parameterized graph
 * to point to a new base graph. Unlike replaceGraphInCompressor2, this only
 * modifies the base field of the specified graph and does not update other
 * references throughout the compressor. It also clears the other fields of
 * the parameterized graph.
 *
 * @param serializedCompressor The serialized compressor to modify.
 * @param parameterizedGraphName The name of the parameterized graph whose
 * base should be updated.
 * @param newBaseGraphName The name of the new base graph to reference.
 * @return The serialized modified compressor as CBOR data.
 */
std::string replaceBaseGraphInCompressor(
        const std::string_view serializedCompressor,
        const std::string& parameterizedGraphName,
        const std::string& newBaseGraphName);

/**
 * @brief Extracts the base name of a graph by splitting at '#'
 * character.
 *
 * @param graphName The full graph name
 * @return The base name (prefix before '#')
 */
std::string_view getGraphBasePrefix(std::string_view graphName);

/**
 * @brief Decodes a serialized compressor into a CBOR structure.
 *
 * @param serialized The serialized compressor to decode.
 * @return A tuple containing the root CBOR item and the arena used to decode
 */
std::tuple<std::shared_ptr<const A1C_Item>, std::shared_ptr<Arena>>
decodeSerializedCompressorIntoCbor(const std::string_view serialized);

/**
 * @brief Encodes a CBOR item into serialized binary data.
 *
 * @param root The CBOR item to encode
 * @return std::shared_ptr<const std::string_view> A shared pointer to a string
 * view containing the serialized binary data
 * @throws std::runtime_error If encoding fails
 */
std::shared_ptr<const std::string_view> encodeCborAsSerialized(
        const A1C_Item* root);

/**
 * @brief Finds all graphs with a specific prefix in a serialized compressor.
 *
 * This function decodes the serialized compressor into a CBOR structure and
 * searches for all graphs whose base prefix (determined by getGraphBasePrefix)
 * matches the given prefix. A serialized compressor is a CBOR-encoded data
 * structure containing compression graph definitions.
 *
 * @param serializedCompressor The serialized compressor containing the CBOR
 * data.
 * @param prefix The prefix to search for in graph names.
 * @return std::vector<std::string> A vector of graph names that match the
 * prefix.
 */
std::vector<std::string> findAllGraphsWithPrefix(
        std::string_view serializedCompressor,
        const std::string& prefix);

/**
 * @brief Gets the maximum ID from all graphs with '#' suffix in a serialized
 * compressor.
 *
 * This function decodes the serialized compressor into a CBOR structure and
 * finds the maximum ID from all graphs that have a '#' suffix. The ID is
 * extracted from the suffix after the '#' character, which is guaranteed to
 * be a positive integer. Graphs without '#' are skipped.
 *
 * @param serializedCompressor The serialized compressor containing the CBOR
 * data.
 * @return int The maximum ID found, or 0 if no graphs with '#' suffix exist.
 */
int getMaximumIdFromSerialized(std::string_view serializedCompressor);

} // namespace openzl::training::graph_mutation
