// Copyright (c) Meta Platforms, Inc. and affiliates.
#pragma once
#include <google/protobuf/message.h>
#include <google/protobuf/reflection.h>
#include <string>
#include "openzl/cpp/CCtx.hpp"
#include "openzl/cpp/Compressor.hpp"
#include "tools/protobuf/ProtoGraph.h"
#include "tools/protobuf/Types.h"

namespace openzl {
namespace protobuf {
class ProtoSerializer {
   public:
    ProtoSerializer()
    {
        auto graphid = ZL_Protobuf_registerGraph(compressor_.get());
        compressor_.selectStartingGraph(graphid);

        // CCtx setup
        cctx_.refCompressor(compressor_);
        cctx_.setParameter(CParam::StickyParameters, 1);
        cctx_.setParameter(CParam::CompressionLevel, 1);
        cctx_.setParameter(CParam::FormatVersion, ZL_MAX_FORMAT_VERSION);
    }

    void setCompressor(Compressor&& compressor)
    {
        compressor_ = std::move(compressor);
        cctx_.refCompressor(compressor_);
    }

    Compressor* getCompressor()
    {
        return &compressor_;
    }

    virtual ~ProtoSerializer() = default;

    /**
     * Serializes a protobuf message to a string.
     *
     * @param message The message to serialize.
     * @return The serialized message.
     */
    std::string serialize(const Message& message);

    /**
     * Extracts the the inputs that would be passed to the compressor. These
     * can be used to train an optimal compressor.
     */
    std::vector<Input> getTrainingInputs(const Message& message);

   private:
    Compressor compressor_;
    CCtx cctx_;
};
} // namespace protobuf
} // namespace openzl
