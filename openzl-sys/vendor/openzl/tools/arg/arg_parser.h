// Copyright (c) Meta Platforms, Inc. and affiliates.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "tools/arg/command.h"
#include "tools/arg/flag.h"
#include "tools/arg/parsed_args.h"
#include "tools/arg/positional.h"

namespace openzl::arg {

class ArgParser {
   public:
    static constexpr int CMD_UNSPECIFIED = 0;

    // see the documentation in struct Flag, Command, Positional for more info
    // on expected arguments
    void addGlobalFlag(
            const std::string& name,
            char shortName,
            bool hasVal,
            const std::string& help);
    void addGlobalImmediate(
            const std::string& name,
            char shortName,
            bool hasVal,
            const std::string& help);
    void addCommand(int cmd, const std::string& name, char shortName);

    void addCommandFlag(
            int cmd,
            const std::string& name,
            char shortName,
            bool hasVal,
            const std::string& help);
    void addCommandImmediate(
            int cmd,
            const std::string& name,
            char shortName,
            bool hasVal,
            const std::string& help);
    // order matters
    void addCommandPositional(
            int cmd,
            const std::string& name,
            const std::string& help);

    // Populated based on added flags, commands, and positionals
    std::string help() const;

    // Returns the help string for the given command. If the command doesn't
    // exist, only the global flags will be shown.
    std::string help(int cmd) const;

    /**
     * @brief Parse the command line and create a ParsedArgs object.
     *
     * In addition to parsing, this also does some light validation for:
     * - flag validity
     * - existence of flag values for flags that require them
     * - too many positional arguments
     * - positionals before subcommand
     * - multiple subcommands
     * @throws std::runtime_error on failure
     */
    ParsedArgs parse(int argc, char** argv) const;

    /**
     * @brief Validate the parsed arguments, throwing on failure.
     *
     * This does some more validation on top of what parse() does:
     * - a subcommand must be set
     * - every positional argument must be present
     * @throws std::runtime_error on failure
     * @note This is a separate function from parse() because sometimes the user
     * will pass an immediate like --help. It would be wise to process
     * immediates after parse() but before validate(), as clearly the subcommand
     * won't be set, nor will any of the expected positionals be populated.
     */
    void validate(const ParsedArgs& parsedArgs) const;

   private:
    std::string help(const Command& command) const;

    // Get the longform name string of a command, if it exists. Otherwise return
    // the stringified int.
    std::string commandName(int cmd) const;

    std::map<int, std::vector<Flag>> cmdFlags_;
    std::map<int, std::vector<Positional>> cmdPositionals_;
    std::vector<Command> commands_;
    std::map<std::string, Flag> globalFlags_;
};

} // namespace openzl::arg
