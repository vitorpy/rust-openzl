// Copyright (c) Meta Platforms, Inc. and affiliates.

#pragma once

#include <vector>

#include "openzl/cpp/Output.hpp"
#include "openzl/cpp/detail/NonNullUniqueCPtr.hpp"
#include "openzl/cpp/poly/Span.hpp"
#include "openzl/cpp/poly/StringView.hpp"
#include "openzl/zl_decompress.h"
#include "openzl/zl_dtransform.h"
#include "openzl/zl_opaque_types.h"

namespace openzl {
class CustomDecoder;

enum class DParam {
    StickyParameters        = ZL_DParam_stickyParameters,
    CheckCompressedChecksum = ZL_DParam_checkCompressedChecksum,
    CheckContentChecksum    = ZL_DParam_checkContentChecksum,
};

class DCtx {
   public:
    /**
     * Creates a new DCtx object owned by the @p DCtx object.
     * @throws on allocation failure.
     */
    DCtx();

    DCtx(const DCtx&) = delete;
    DCtx(DCtx&&)      = default;

    DCtx& operator=(const DCtx&) = delete;
    DCtx& operator=(DCtx&&)      = default;

    ~DCtx() = default;

    /// @returns pointer to the underlying ZL_DCtx* object.
    ZL_DCtx* get()
    {
        return dctx_.get();
    }
    /// @returns const pointer to the underlying ZL_DCtx* object.
    const ZL_DCtx* get() const
    {
        return dctx_.get();
    }

    void setParameter(DParam param, int value);
    int getParameter(DParam) const;
    void resetParameters();

    void decompress(poly::span<Output> outputs, poly::string_view input);
    std::vector<Output> decompress(poly::string_view input);

    void decompressOne(Output& output, poly::string_view input);
    Output decompressOne(poly::string_view input);

    size_t decompressSerial(poly::span<char> output, poly::string_view input);
    std::string decompressSerial(poly::string_view input);

    void registerCustomDecoder(const ZL_MIDecoderDesc& desc);
    void registerCustomDecoder(std::shared_ptr<CustomDecoder> decoder);

    poly::string_view getErrorContextString(ZL_Error error) const;

    template <typename ResultType>
    poly::string_view getErrorContextString(ResultType result) const
    {
        return getErrorContextString(ZL_RES_error(result));
    }

    template <typename ResultType>
    typename ResultType::ValueType unwrap(
            ResultType result,
            poly::string_view msg = {},
            poly::source_location location =
                    poly::source_location::current()) const
    {
        return openzl::unwrap(
                result, std::move(msg), this, std::move(location));
    }

   protected:
    DCtx(ZL_DCtx* dctx, detail::NonNullUniqueCPtr<ZL_DCtx>::DeleterFn deleter)
            : dctx_(dctx, deleter)
    {
    }

   private:
    detail::NonNullUniqueCPtr<ZL_DCtx> dctx_;
};

class DCtxRef : public DCtx {
   public:
    explicit DCtxRef(ZL_DCtx* dctx) : DCtx(dctx, nullptr) {}
};

} // namespace openzl
