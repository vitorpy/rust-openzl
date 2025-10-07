// Copyright (c) Meta Platforms, Inc. and affiliates.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "openzl/zl_selector.h"

/**
 * A selector that behaves as follows:
 *
 *   if (input.empty()) {
 *     return successors[0];
 *   } else {
 *     return successors[1];
 *   }
 */
ZL_SelectorDesc buildEmptyInputSelectorDesc(
        ZL_Type type,
        const ZL_GraphID* successors,
        size_t nbSuccessors);

#ifdef __cplusplus
}
#endif
