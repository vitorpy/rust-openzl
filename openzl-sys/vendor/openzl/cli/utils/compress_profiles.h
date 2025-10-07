// Copyright (c) Meta Platforms, Inc. and affiliates.

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "openzl/zl_compressor.h"

namespace openzl::cli {

struct ProfileArgs {
    std::string name;

    // Arbitrary (K,V) arguments provided on the command line.
    std::map<std::string, std::string> argmap;
};

class CompressProfile {
   private:
    using GenFunc = std::function<
            ZL_GraphID(ZL_Compressor*, void*, const ProfileArgs&)>;

   public:
    CompressProfile(
            const std::string& name_,
            const std::string& description_,
            GenFunc gen_,
            std::shared_ptr<void> opaque_ = nullptr)
            : name(name_),
              description(description_),
              gen(std::move(gen_)),
              opaque(opaque_)
    {
    }

    virtual ~CompressProfile() = default;

    std::string name;
    std::string description; // useful for documentation as well as printing
    GenFunc gen;
    std::shared_ptr<void> opaque; // an optional opaque helper pointer that's
                                  // passed to the gen function
};

const std::map<std::string, std::shared_ptr<CompressProfile>>&
compressProfiles();

} // namespace openzl::cli
