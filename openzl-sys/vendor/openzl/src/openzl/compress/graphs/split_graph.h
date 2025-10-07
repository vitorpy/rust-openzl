// Copyright (c) Meta Platforms, Inc. and affiliates.
#ifndef OPENZL_COMPRESS_GRAPHS_SPLIT_GRAPH_H
#define OPENZL_COMPRESS_GRAPHS_SPLIT_GRAPH_H

#include "openzl/zl_graph_api.h"

/**
 * Invokes the custom node, and then passes each output to the corresponding
 * custom graph.
 */
ZL_Report ZL_splitFnGraph(ZL_Graph* graph, ZL_Edge** inputs, size_t numInputs);

#endif
