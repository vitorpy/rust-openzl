// Copyright (c) Meta Platforms, Inc. and affiliates.
#pragma once

#include <cstdio>
#include <optional>
#include <random>
#include <string_view>
#include <vector>

#include <folly/File.h>
#include <folly/Range.h>
#include <folly/io/IOBuf.h>
#include <thrift/lib/cpp2/protocol/Serializer.h>

#include "custom_transforms/thrift/kernels/tests/thrift_kernel_test_utils.h"
#include "custom_transforms/tulip_v2/decode_tulip_v2.h"
#include "custom_transforms/tulip_v2/encode_tulip_v2.h"
#include "openzl/zl_version.h"
#include "tests/datagen/random_producer/RNGEngine.h"
#include "tests/datagen/structures/FixedWidthDataProducer.h"
#include "tools/zstrong_cpp.h"

// TODO: Not sure if there is a better way to do this
#if ZL_FBCODE_IS_RELEASE
#    include "openzl/versions/release/custom_transforms/tulip_v2/tests/gen-cpp2/tulip_v2_data_fatal_types.h"
#    include "openzl/versions/release/custom_transforms/tulip_v2/tests/gen-cpp2/tulip_v2_data_types.h"
#    include "openzl/versions/release/custom_transforms/tulip_v2/tests/gen-cpp2/tulip_v2_data_visitation.h"
#else
#    include "data_compression/experimental/zstrong/custom_transforms/tulip_v2/tests/gen-cpp2/tulip_v2_data_fatal_types.h"
#    include "data_compression/experimental/zstrong/custom_transforms/tulip_v2/tests/gen-cpp2/tulip_v2_data_types.h"
#    include "data_compression/experimental/zstrong/custom_transforms/tulip_v2/tests/gen-cpp2/tulip_v2_data_visitation.h"
#endif

namespace zstrong::tulip_v2::tests {
using zstrong::thrift::tests::generate;

inline std::string encodeTulipV2(TulipV2Data const& data)
{
    std::array<char, 3> headerBytes = { char(0x80), char(0x00), char(0x2C) };
    std::string buf(headerBytes.data(), headerBytes.size());
    apache::thrift::CompactSerializer::serialize(data, &buf);
    buf.push_back(0x00);

    return buf;
}

template <typename RNG>
std::string generateTulipV2(size_t n, RNG&& gen)
{
    std::string out;
    for (size_t i = 0; i < n; ++i) {
        out += encodeTulipV2(generate<TulipV2Data>(gen));
        if (i != n - 1) {
            std::bernoulli_distribution separator(0.5);
            if (separator(gen)) {
                out.push_back('\n');
            }
        }
    }
    return out;
}

class TulipV2Producer : public zstrong::tests::datagen::FixedWidthDataProducer {
   public:
    explicit TulipV2Producer(
            std::shared_ptr<zstrong::tests::datagen::RandWrapper> rw,
            size_t maxSamples = 10)
            : FixedWidthDataProducer(rw, 1), dist_(rw, 5, maxSamples)
    {
    }

    ::zstrong::tests::datagen::FixedWidthData operator()(
            ::zstrong::tests::datagen::RandWrapper::NameType name) override
    {
        return { generateTulipV2(
                         dist_(name),
                         zstrong::tests::datagen::RNGEngine<uint32_t>(
                                 this->rw_.get(),
                                 "TulipV2Producer::RNGEngine::operator()")),
                 1 };
    }

    void print(std::ostream& os) const override
    {
        os << "TulipV2Producer(std::string, 1)";
    }

   private:
    ::zstrong::tests::datagen::VecLengthDistribution dist_;
};

inline std::string compressTulipV2(
        std::string_view data,
        TulipV2Successors const& successors,
        std::optional<size_t> minDstCapacity = std::nullopt)
{
    CGraph cgraph;
    auto graph = createTulipV2Graph(cgraph.get(), successors, 0, 100);
    cgraph.unwrap(ZL_Compressor_selectStartingGraphID(cgraph.get(), graph));
    size_t const compressBound = ZL_compressBound(data.size());
    std::string compressed;
    compressed.resize(
            std::max(compressBound, minDstCapacity.value_or(compressBound)));
    CCtx cctx;
    cctx.unwrap(ZL_CCtx_refCompressor(cctx.get(), cgraph.get()));
    cctx.unwrap(ZL_CCtx_setParameter(
            cctx.get(), ZL_CParam_formatVersion, ZL_MAX_FORMAT_VERSION));
    size_t const cSize = cctx.unwrap(ZL_CCtx_compress(
            cctx.get(),
            compressed.data(),
            compressed.size(),
            data.data(),
            data.size()));
    compressed.resize(cSize);
    return compressed;
}

inline std::string decompressTulipV2(
        std::string_view data,
        std::optional<size_t> maxDstSize = std::nullopt)
{
    DCtx dctx;
    dctx.unwrap(registerCustomTransforms(dctx.get(), 0, 100));
    return decompress(dctx, data, maxDstSize);
}

} // namespace zstrong::tulip_v2::tests
