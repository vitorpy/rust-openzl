// Copyright (c) Meta Platforms, Inc. and affiliates.

#pragma once

#include "openzl/cpp/CompressIntrospectionHooks.hpp"
#include "openzl/cpp/experimental/trace/Codec.hpp"
#include "openzl/cpp/experimental/trace/Graph.hpp"
#include "openzl/cpp/experimental/trace/StreamVisualizer.hpp"
#include "openzl/cpp/poly/StringView.hpp"
#include "openzl/shared/a1cbor.h"
#include "openzl/zl_opaque_types.h"

#include <map>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace openzl::visualizer {
class CompressionTraceHooks : public openzl::CompressIntrospectionHooks {
   public:
    CompressionTraceHooks()           = default;
    ~CompressionTraceHooks() override = default;

    void printStreamMetadata();

    void printCodecMetadata();

    ZL_Report serializeStreamdumpToCbor(
            A1C_Arena* a1c_arena,
            std::vector<uint8_t>& buffer);

    ZL_Report writeSerializedStreamdump(std::vector<uint8_t>& buffer);

    void setCompressedSize(size_t compressionResultSize);
    size_t fillCSize(std::vector<size_t>& cSize, const ZL_DataID streamID);

    std::pair<
            poly::string_view,
            std::map<size_t, std::pair<poly::string_view, poly::string_view>>>
    getLatestTrace();

    // ***************************************************
    // Overridden functions from CompressIntrospectionHooks
    // ***************************************************
    void on_codecEncode_start(
            ZL_Encoder* encoder,
            const ZL_Compressor* compressor,
            ZL_NodeID nid,
            const ZL_Input* inStreams[],
            size_t nbInStreams) override;

    void on_codecEncode_end(
            ZL_Encoder*,
            const ZL_Output* outStreams[],
            size_t nbOutputs,
            ZL_Report codecExecResult) override;

    void on_ZL_Encoder_getScratchSpace(ZL_Encoder* ei, size_t size) override;

    void on_ZL_Encoder_sendCodecHeader(
            ZL_Encoder* encoder,
            const void* trh,
            size_t trhSize) override;

    void on_ZL_Encoder_createTypedStream(
            ZL_Encoder* encoder,
            int outStreamIndex,
            size_t eltsCapacity,
            size_t eltWidth,
            ZL_Output* createdStream) override;

    void on_migraphEncode_start(
            ZL_Graph* graph,
            const ZL_Compressor* compressor,
            ZL_GraphID gid,
            ZL_Edge* inputs[],
            size_t nbInputs) override;

    void on_migraphEncode_end(
            ZL_Graph*,
            ZL_GraphID successorGraphs[],
            size_t nbSuccessors,
            ZL_Report graphExecResult) override;

    void on_cctx_convertOneInput(
            const ZL_CCtx* const cctx,
            const ZL_Data* const,
            const ZL_Type inType,
            const ZL_Type portTypeMask,
            const ZL_Report conversionResult) override;

    void on_ZL_Graph_getScratchSpace(ZL_Graph* graph, size_t size) override;

    void on_ZL_Edge_setMultiInputDestination_wParams(
            ZL_Graph* graph,
            ZL_Edge* inputs[],
            size_t nbInputs,
            ZL_GraphID gid,
            const ZL_LocalParams* lparams) override;

    void on_ZL_CCtx_compressMultiTypedRef_start(
            ZL_CCtx const* const cctx,
            void const* const dst,
            size_t const dstCapacity,
            ZL_TypedRef const* const inputs[],
            size_t const nbInputs) override;
    void on_ZL_CCtx_compressMultiTypedRef_end(
            ZL_CCtx const* const cctx,
            ZL_Report const result) override;

   private:
    void streamdump(const ZL_Output* stream);

    std::stringstream outStream_; // output stream to write to
    std::map<size_t, std::pair<std::string, std::string>>
            latestStreamdumpCache_; // cache for latest streamdumps. Key is the
                                    // stream ID, value is a pair of strings
                                    // (content, string lengths (or ""))
    std::string latestTraceCache_;  // cache for latest trace

    const ZL_CCtx* cctx_{};
    size_t compressedSize_{};
    size_t currCodecNum_ = 0;
    std::map<ZL_DataID, Stream, ZL_DataIDCustomComparator> streamInfo_;
    std::vector<Codec> codecInfo_;
    std::unordered_map<size_t, std::vector<ZL_DataID>> codecInEdges_;
    std::unordered_map<size_t, std::vector<ZL_DataID>> codecOutEdges_;
    std::unordered_map<
            ZL_DataID,
            std::vector<ZL_DataID>,
            ZL_DataIDHash,
            ZL_DataIDEquality>
            streamSuccessors_;
    std::unordered_map<ZL_DataID, size_t, ZL_DataIDHash, ZL_DataIDEquality>
            streamConsumerCodec_;
    std::vector<std::pair<Graph, std::vector<size_t>>> graphInfo_;
    bool currEncompassingGraph_ = false; // if codecs are running within a graph

    struct ConversionError {
        ZL_DataID streamId;
        ZL_Report failureReport;
    };
    std::optional<ConversionError> maybeConversionError_ = std::nullopt;
};
} // namespace openzl::visualizer
