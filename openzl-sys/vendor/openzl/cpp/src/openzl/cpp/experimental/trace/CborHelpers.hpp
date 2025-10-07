// Copyright (c) Meta Platforms, Inc. and affiliates.
#pragma once

#include "openzl/cpp/LocalParams.hpp"
#include "openzl/shared/a1cbor.h"
#include "openzl/zl_errors.h"

namespace openzl::visualizer {

ZL_Report addIntValue(A1C_MapBuilder& builder, const char* key, size_t val);

ZL_Report addFloatValue(A1C_MapBuilder& builder, const char* key, double val);

ZL_Report
addStringValue(A1C_MapBuilder& builder, const char* key, const char* val);

ZL_Report addBooleanValue(A1C_MapBuilder& builder, const char* key, bool val);

ZL_Report serializeLocalParams(
        A1C_Arena* a1c_arena,
        A1C_Item* a1c_localParamsParent,
        const LocalParams& lpi);
} // namespace openzl::visualizer
