// Copyright (c) Meta Platforms, Inc. and affiliates.

#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "tools/arg/flag.h"

namespace openzl::arg {

// forward decl
class ArgParser;
class ParsedArgs {
   public:
    explicit ParsedArgs() {}

    // lookups
    int chosenCmd() const;
    std::optional<std::string> immediate() const;

    bool globalHasFlag(const std::string& name) const;
    std::optional<std::string> globalFlag(const std::string& name) const;
    std::string globalRequiredFlag(const std::string& name) const;

    std::string cmdPositional(int cmd, const std::string& name) const;
    bool cmdHasFlag(int cmd, const std::string& name) const;
    std::optional<std::string> cmdFlag(int cmd, const std::string& name) const;
    std::string cmdRequiredFlag(int cmd, const std::string& name) const;

   private:
    std::map<int, std::map<std::string, std::optional<std::string>>> cmdVals_;
    int chosenCmd_ = 0;

    // copied from ArgParser
    std::map<int, std::vector<Flag>> cmdFlags_;

    friend class ArgParser;
};

} // namespace openzl::arg
