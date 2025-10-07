// Copyright (c) Meta Platforms, Inc. and affiliates.
#include <algorithm>

#include "security/lionhead/utils/lib_ftest/ftest.h"

#include "openzl/shared/pdqsort.h"
#include "tests/fuzz_utils.h"

namespace zstrong {
namespace tests {
namespace {

template <typename T, typename F>
void fuzzPdqsort_inner(F& f)
{
    size_t const eltWidth = sizeof(T);
    std::vector<T> input =
            gen_vec<T>(f, "input_data", InputLengthInElts(eltWidth));
    std::vector<T> verification = input;
    ASSERT_EQ(input, verification);
    pdqsort(input.data(), input.size(), eltWidth);
    // sort the verification vector
    std::sort(verification.begin(), verification.end());
    ASSERT_EQ(input, verification);
}

FUZZ(SortTest, FuzzPDQsort)
{
    size_t const eltWidth = f.choices("elt_width", { 1, 2, 4, 8 });
    switch (eltWidth) {
        case 1:
            fuzzPdqsort_inner<uint8_t>(f);
            break;
        case 2:
            fuzzPdqsort_inner<uint16_t>(f);
            break;
        case 4:
            fuzzPdqsort_inner<uint32_t>(f);
            break;
        case 8:
            fuzzPdqsort_inner<uint64_t>(f);
            break;
    }
}

} // namespace
} // namespace tests
} // namespace zstrong
