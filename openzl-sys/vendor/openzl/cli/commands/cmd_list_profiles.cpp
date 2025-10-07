// Copyright (c) Meta Platforms, Inc. and affiliates.
#include <sstream>

#include "cli/commands/cmd_list_profiles.h"
#include "cli/utils/compress_profiles.h"
#include "tools/logger/Logger.h"

namespace openzl {
namespace cli {

using namespace openzl::tools::logger;

int cmdListProfiles(const ListProfilesArgs&)
{
    std::stringstream ss;
    ss << "Available profiles:\n";
    for (auto const& [_, profile] : compressProfiles()) {
        auto const& name = profile->name;
        auto const& desc = profile->description;
        ss << "  -| " << name << "\t= " << desc << "\n";
    }
    Logger::log(ALWAYS, ss.str());
    return 0;
}
} // namespace cli
} // namespace openzl
