// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <gtest/gtest.h>
#include "openzl/common/stream.h" // For stream usage in hardcoded feature generators
#include "openzl/compress/selectors/ml/gbt.h"
#include "openzl/shared/mem.h" // For uint64_t ... types
#include "tests/compress/ml_selectors/test_zstrong_ml_core_models.h"
#include "tests/zstrong/test_zstrong_fixture.h"

using namespace ::testing;

namespace {
void createStreamAndVerifyPrediction(
        GBTModel& gbtModel,
        std::vector<int>& streamData)
{
    /*
     * Creates a ZL_Data and populates it with the data from streamData. Uses
     * GBTModel to make a prediction and verifies the prediction.
     */
    zstrong::tests::WrappedStream<int> stream(streamData, ZL_Type_numeric);

    ZL_RESULT_OF(Label)
    predicted = GBTModel_predict(&gbtModel, stream.getStream());
    ASSERT_FALSE(ZL_RES_isError(predicted));
    std::string decodedLabel = ZL_RES_value(predicted);

    ASSERT_GE((int)streamData.size(), 2);

    int a = streamData[0];
    int b = streamData[1];

    if (streamData.size() < 3) {
        EXPECT_EQ(decodedLabel, (a & (b ^ 1)) == 1 ? "one" : "zero");
    } else {
        int c = streamData[2];
        std::string expectedLabel;

        if ((a + b + c) % 3 == 0) {
            expectedLabel = "zero";
        } else if ((a + b + c) % 3 == 1) {
            expectedLabel = "one";
        } else {
            expectedLabel = "two";
        }

        EXPECT_EQ(decodedLabel, expectedLabel);
    }
}

} // namespace

class ZstrongCoreBinaryMLTest : public Test {
   protected:
    GBTModel gbtModel;
    // Feature Generator that sets the first two elements as the 'a' and 'b'
    // features

    static ZL_Report featureGenerator(
            const ZL_Input* inputStream,
            VECTOR(LabeledFeature) * features,
            const void* featureContext)
    {
        (void)featureContext;
        ZL_ASSERT_EQ((int)ZL_Input_type(inputStream), (int)ZL_Type_numeric);

        ZL_ASSERT_EQ(ZL_Input_eltWidth(inputStream), 4);
        const uint32_t* data = (const uint32_t*)ZL_Input_ptr(inputStream);

        LabeledFeature aFeature = { "a", float(data[0]) };
        LabeledFeature bFeature = { "b", float(data[1]) };

        bool badAlloc = false;
        badAlloc |= !VECTOR_PUSHBACK(*features, aFeature);
        badAlloc |= !VECTOR_PUSHBACK(*features, bFeature);

        ZL_RET_R_IF(allocation, badAlloc, "Failed to add features to vector");
        return ZL_returnSuccess();
    }

    void SetUp() override
    {
        gbtModel               = getGbtBinaryCoreGbtModel(featureGenerator);
        const ZL_Report report = GBTModel_validate(&gbtModel);
        ASSERT_FALSE(ZL_isError(report));
    }
};

class ZstrongCoreMultiMLTest : public Test {
   protected:
    GBTModel gbtModel;
    // Feature Generator that sets the first three elements as the 'a', 'b', 'c'
    // features
    static ZL_Report featureGenerator(
            const ZL_Input* inputStream,
            VECTOR(LabeledFeature) * features,
            const void* featureContext)
    {
        (void)featureContext;
        ZL_ASSERT_EQ((int)ZL_Input_type(inputStream), (int)ZL_Type_numeric);

        ZL_ASSERT_EQ(ZL_Input_eltWidth(inputStream), 4);
        const uint32_t* data = (const uint32_t*)ZL_Input_ptr(inputStream);

        LabeledFeature aFeature = { "a", float(data[0]) };
        LabeledFeature bFeature = { "b", float(data[1]) };
        LabeledFeature cFeature = { "c", float(data[2]) };

        bool badAlloc = false;
        badAlloc |= !VECTOR_PUSHBACK(*features, aFeature);
        badAlloc |= !VECTOR_PUSHBACK(*features, bFeature);
        badAlloc |= !VECTOR_PUSHBACK(*features, cFeature);

        ZL_RET_R_IF(allocation, badAlloc, "Failed to add features to vector");
        return ZL_returnSuccess();
    }

    void SetUp() override
    {
        gbtModel               = getGbtMulticlassCoreGbtModel(featureGenerator);
        const ZL_Report report = GBTModel_validate(&gbtModel);
        ASSERT_FALSE(ZL_isError(report));
    }
};
TEST_F(ZstrongCoreBinaryMLTest, GBTModelTest)
{
    /*
     * Verify that the binary GBTModel generated from the trained XGBoost
     * has correct predictions. The model predicts the function `a &
     * (b ^ 1)`
     */

    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            std::vector<int> streamData = { a, b };
            createStreamAndVerifyPrediction(gbtModel, streamData);
        }
    }
}

TEST_F(ZstrongCoreMultiMLTest, GBTModelTest)
{
    /*
     * Verify that the multiclass GBTModel generated from the trained XGBoost
     * has correct predictions. The model predicts the function (a + b
     * + c) % 3.
     */

    for (int a = 0; a <= 2; a++) {
        for (int b = 0; b <= 2; b++) {
            for (int c = 0; b <= 2; b++) {
                std::vector<int> streamData = { a, b, c };
                createStreamAndVerifyPrediction(gbtModel, streamData);
            }
        }
    }
}
