// Copyright (c) Meta Platforms, Inc. and affiliates.

#ifndef ZSTRONG_ZS_CSV_PARSER_H
#define ZSTRONG_ZS_CSV_PARSER_H

#include "openzl/zl_compressor.h"
#include "openzl/zl_opaque_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

// Parameters for ZL_CsvParser_registerGraph:
// Set to 1 if the first line is a header line, 0 otherwise
#define ZL_PARSER_HAS_HEADER_PID 225
// The character separator between columns. e.g. `,` for comma `|` for pipe
#define ZL_PARSER_SEPARATOR_PID 226
// Whether to use the null-aware parser (1) or not (0)
#define ZL_PARSER_USE_NULL_AWARE_PID 227

/**
 * @brief Registers the csv parser graph. This graph takes a serialized input
 * and parses it assuming it follows the csv format. If @p hasHeader is true,
 * the first line is assumed to be a header. The graph then splits the header
 * row out and sends the remaining rows to the clustering graph provided by @p
 * clusteringGraph. The csv parser makes some assumptions about the input:
 * - The quote character ('"') specificies the start and end of a string field.
 * Delimiters are treated as part of the string literal when in quoted segments
 * - The newline character ('\n') specifies the end of a row, with no endline at
 * the end of the input
 * - Each value may have leading or trailing whitespace
 *
 * @returns The graph ID registered for the csv parser graph
 * @param hasHeader A boolean indicating whether the first line is a header line
 * @param sep The character separator between columns. e.g. ',' for comma
 * @param useNullAware Whether to use the null-aware parser. It coalesces nulls
 * instead of dispatching a null string to empty columns. This is useful when
 * there are a lot of contiguous columns with null values.
 * @param clusteringGraph The clustering graph to send the remaining rows
 * excluding the header to as a successor
 */
ZL_GraphID ZL_CsvParser_registerGraph(
        ZL_Compressor* compressor,
        bool hasHeader,
        char sep,
        bool useNullAware,
        const ZL_GraphID clusteringGraph);

#if defined(__cplusplus)
} // extern "C"
#endif
#endif // ZSTRONG_ZS_CSV_PARSER_H
