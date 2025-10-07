// Copyright (c) Meta Platforms, Inc. and affiliates.

#pragma once

#include <filesystem>
#include <memory>

#include "cli/args/ArgsUtils.h"
#include "cli/args/GlobalArgs.h"
#include "cli/utils/util.h"

#include "tools/io/InputFile.h"
#include "tools/io/OutputFile.h"

namespace openzl::cli {

class DecompressArgs : GlobalArgs {
   public:
    static void addArgs(arg::ArgParser& parser)
    {
        // Add the command
        parser.addCommand(cmd(), "decompress", 'd');

        // Add the args
        parser.addCommandPositional(cmd(), kInput, "Input file path.");
        parser.addCommandFlag(cmd(), kOutput, 'o', true, "Output file path.");
        parser.addCommandFlag(
                cmd(), kForce, 'f', false, "Overwrite output file.");
    }

    explicit DecompressArgs(const arg::ParsedArgs& parsed) : GlobalArgs(parsed)
    {
        // Validate the input and output paths
        auto inputPath  = parsed.cmdPositional(cmd(), kInput);
        auto outputPath = parsed.cmdFlag(cmd(), kOutput);
        if (!outputPath) {
            if (std::filesystem::path(inputPath).extension() != ".zl") {
                throw InvalidArgsException(
                        "Input file must have a .zl extension to infer output file path!");
            }
            outputPath = std::filesystem::path(inputPath)
                                 .replace_extension()
                                 .string();
        }
        checkOutput(outputPath.value(), parsed.cmdHasFlag(cmd(), kForce));

        // Set the input and output files
        input  = std::make_unique<tools::io::InputFile>(std::move(inputPath));
        output = std::make_unique<tools::io::OutputFile>(
                std::move(outputPath).value());
    }

    static Cmd cmd()
    {
        return Cmd::DECOMPRESS;
    }

    std::unique_ptr<tools::io::Input> input;
    std::unique_ptr<tools::io::Output> output;

   private:
    inline static const std::string kInput  = "input";
    inline static const std::string kOutput = "output";
    inline static const std::string kForce  = "force";
};

} // namespace openzl::cli
