// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "openzl/cpp/FrameInfo.hpp"

namespace openzl {
namespace {
ZL_FrameInfo* createFrameInfo(poly::string_view compressed)
{
    auto info = ZL_FrameInfo_create(compressed.data(), compressed.size());
    if (info == nullptr) {
        throw ExceptionBuilder("FrameInfo: Corrupt OpenZL compressed frame")
                .withErrorCode(ZL_ErrorCode_corruption)
                .build();
    }
    return info;
}
} // namespace

FrameInfo::FrameInfo(poly::string_view compressed)
        : info_(createFrameInfo(compressed), ZL_FrameInfo_free)
{
}

size_t FrameInfo::numOutputs() const
{
    return unwrap(ZL_FrameInfo_getNumOutputs(get()));
}

Type FrameInfo::outputType(size_t index) const
{
    return Type(unwrap(ZL_FrameInfo_getOutputType(get(), (int)index)));
}

size_t FrameInfo::outputContentSize(size_t index) const
{
    return unwrap(ZL_FrameInfo_getDecompressedSize(get(), (int)index));
}

} // namespace openzl
