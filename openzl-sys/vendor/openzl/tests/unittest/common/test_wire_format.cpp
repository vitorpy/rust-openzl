// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <gtest/gtest.h>

#include "openzl/common/errors_internal.h"
#include "openzl/common/wire_format.h"
#include "openzl/shared/mem.h"

namespace {
uint32_t constexpr kMinVersion = ZL_MIN_FORMAT_VERSION;
uint32_t constexpr kMaxVersion = ZL_MAX_FORMAT_VERSION;
} // namespace

TEST(WireFormatTest, SupportedFormatVersions)
{
    ASSERT_LE(kMinVersion, kMaxVersion);
    for (uint32_t v = kMinVersion; v <= kMaxVersion; ++v) {
        ASSERT_TRUE(ZL_isFormatVersionSupported(v));
    }
}

TEST(WireFormatTest, MagicNumberToVersion)
{
    for (uint32_t v = kMinVersion; v <= kMaxVersion; ++v) {
        uint32_t const magic = ZL_getMagicNumber(v);
        ZL_Report const ret  = ZL_getFormatVersionFromMagic(magic);
        ASSERT_FALSE(ZL_isError(ret));
        ASSERT_EQ(ZL_validResult(ret), (size_t)v);
    }
}

TEST(WireFormatTest, MagicNumberFrameFormat)
{
    for (uint32_t v = kMinVersion; v <= kMaxVersion; ++v) {
        char buffer[4];
        ZL_writeMagicNumber(buffer, sizeof(buffer), v);
        ZL_Report const ret =
                ZL_getFormatVersionFromFrame(buffer, sizeof(buffer));
        ASSERT_FALSE(ZL_isError(ret));
        ASSERT_EQ(ZL_validResult(ret), (size_t)v);
    }
}

TEST(WireFormatTest, InvalidMagicNumberFrameFormat)
{
    const uint32_t tooOldMagic = ZSTRONG_MAGIC_NUMBER_BASE + kMinVersion - 1;
    const uint32_t tooNewMagic = ZSTRONG_MAGIC_NUMBER_BASE + kMaxVersion + 1;
    const uint32_t zstdMagic   = 0xFD2FB528u;

    const std::vector<std::pair<uint32_t, ZL_ErrorCode>> expectations{
        { { tooOldMagic, ZL_ErrorCode_formatVersion_unsupported },
          { tooNewMagic, ZL_ErrorCode_formatVersion_unsupported },
          { zstdMagic, ZL_ErrorCode_header_unknown } }
    };

    for (const auto& [magic, code] : expectations) {
        char buffer[4];
        ZL_writeLE32(buffer, magic);
        ZL_Report const ret =
                ZL_getFormatVersionFromFrame(buffer, sizeof(buffer));
        ASSERT_TRUE(ZL_isError(ret));
        ASSERT_EQ(ZL_E_code(ZL_RES_error(ret)), code);
    }
}

TEST(WireFormatTest, DefaultEncodingVersion)
{
    ASSERT_TRUE(ZL_isFormatVersionSupported(ZL_getDefaultEncodingVersion()));
}
