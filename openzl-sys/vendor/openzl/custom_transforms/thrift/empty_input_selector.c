// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "custom_transforms/thrift/empty_input_selector.h" // @manual

#include "openzl/zl_data.h"

static ZL_GraphID empty_input_selector_impl(
        const ZL_Selector* selCtx,
        const ZL_Input* inputStream,
        const ZL_GraphID* customGraphs,
        size_t nbCustomGraphs)
{
    (void)selCtx;
    assert(customGraphs != NULL);
    assert(nbCustomGraphs == 2);
    if (customGraphs == NULL || nbCustomGraphs != 2) {
        return (ZL_GraphID){
            0
        }; // return an illegal graph id to indicate failure.
    }
    size_t nbElts = ZL_Input_numElts(inputStream);
    return customGraphs[!!nbElts];
}

ZL_SelectorDesc buildEmptyInputSelectorDesc(
        ZL_Type type,
        const ZL_GraphID* successors,
        size_t nbSuccessors)
{
    return (ZL_SelectorDesc){
        .selector_f     = empty_input_selector_impl,
        .inStreamType   = type,
        .customGraphs   = successors,
        .nbCustomGraphs = nbSuccessors,
        .localParams = { .intParams  = { .intParams = NULL, .nbIntParams = 0 },
                         .copyParams = { .copyParams   = NULL,
                                         .nbCopyParams = 0 } },
    };
}
