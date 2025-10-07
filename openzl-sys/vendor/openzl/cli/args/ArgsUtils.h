// Copyright (c) Meta Platforms, Inc. and affiliates.

#pragma once

#include <string>

#include "openzl/cpp/Compressor.hpp"

namespace openzl {
namespace cli {
/**
 * Check that the output file does not exist or that --force is specified.
 */
void checkOutput(const std::string& path, bool force);

/**
 * Validates the provided arguments to create a compressor and then returns the
 * created compressor.
 */
std::unique_ptr<Compressor> createCompressorFromArgs(
        const std::optional<std::string>& profileName,
        const std::optional<std::string>& profileArg,
        const std::optional<std::string>& compressorPath);

} // namespace cli
} // namespace openzl
