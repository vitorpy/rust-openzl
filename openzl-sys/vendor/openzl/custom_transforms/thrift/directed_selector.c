// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "custom_transforms/thrift/directed_selector.h" // @manual

#include "openzl/zl_data.h"

static ZL_GraphID directed_selector_impl(
        const ZL_Selector* selCtx,
        const ZL_Input* inputStream,
        const ZL_GraphID* customGraphs,
        size_t nbCustomGraphs)
{
    (void)selCtx;
    assert(customGraphs != NULL);
    assert(nbCustomGraphs > 0);
    ZL_IntMetadata metadata =
            ZL_Input_getIntMetadata(inputStream, kDirectedSelectorMetadataID);
    assert(metadata.isPresent);
    size_t idx = metadata.mValue;
    assert(idx < nbCustomGraphs);
    if (!metadata.isPresent || idx >= nbCustomGraphs || customGraphs == NULL) {
        return (ZL_GraphID){
            0
        }; // return an illegal graph id to indicate failure.
    }
    return customGraphs[idx];
}

ZL_SelectorDesc buildDirectedSelectorDesc(
        ZL_Type type,
        const ZL_GraphID* successors,
        size_t nbSuccessors)
{
    return (ZL_SelectorDesc){
        .selector_f     = directed_selector_impl,
        .inStreamType   = type,
        .customGraphs   = successors,
        .nbCustomGraphs = nbSuccessors,
        .localParams = { .intParams  = { .intParams = NULL, .nbIntParams = 0 },
                         .copyParams = { .copyParams   = NULL,
                                         .nbCopyParams = 0 } },
    };
}
