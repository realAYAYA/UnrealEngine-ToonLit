// Copyright Epic Games, Inc. All Rights Reserved.

#include "rltests/Defs.h"
#include "rltests/conditionaltable/ConditionalTableFixtures.h"

#include "riglogic/conditionaltable/ConditionalTable.h"

#include <pma/resources/AlignedMemoryResource.h>

namespace {

struct CalcTestData {
    float fromValues[2];
    float toValues[2];
    float expected[1];
};

class ConditionalTableTest : public ::testing::TestWithParam<CalcTestData> {
};

}  // namespace

TEST_P(ConditionalTableTest, CheckCalculationBorderCases) {
    pma::AlignedMemoryResource amr;
    const std::size_t conditionalsSize = 2ul;
    auto testData = GetParam();
    auto conditionals = ConditionalTableFactory::withSingleIO(
        rl4::Vector<float>{testData.fromValues, testData.fromValues + conditionalsSize, &amr},
        rl4::Vector<float>{testData.toValues, testData.toValues + conditionalsSize, &amr},
        &amr
        );
    float outputs[] = {0.0f};
    conditionals.calculate(conditionalTableInputs.data(), outputs);
    ASSERT_ELEMENTS_EQ(outputs, testData.expected, 1);
}

INSTANTIATE_TEST_SUITE_P(ConditionalTableTest,
                         ConditionalTableTest,
                         ::testing::Values(
                             // {{fromValues}, {toValues}, {expected}}
                             // In-value below from-value
                             CalcTestData{{0.3f, 0.6f}, {0.6f, 1.0f}, {0.0f}},
                             // In-value equals from-value
                             CalcTestData{{0.1f, 0.6f}, {0.6f, 1.0f}, {0.0f}},
                             // In-value equals to-value
                             CalcTestData{{0.0f, 0.1f}, {0.6f, 1.0f}, {0.2f}},
                             // In-value equals both from-value and to-value
                             CalcTestData{{0.0f, 0.1f}, {0.1f, 1.0f}, {0.2f}},
                             // In-value between from-value and to-value
                             CalcTestData{{0.0f, 0.6f}, {0.6f, 1.0f}, {0.2f}},
                             // In-value above to-value
                             CalcTestData{{0.0f, 0.04f}, {0.04f, 0.09f}, {0.0f}},
                             // Original output not reset, but summed with new value
                             CalcTestData{{0.0f, 0.0f}, {0.2f, 0.2f}, {0.6f}}
                             ));

TEST(ConditionalTableTest, OutputClamped) {
    const std::uint16_t inputCount = 1u;
    const std::uint16_t outputCount = 1u;
    rl4::Vector<std::uint16_t> inputIndices{0};
    rl4::Vector<std::uint16_t> outputIndices{0};
    rl4::Vector<float> slopeValues{1.0f};
    rl4::Vector<float> cutValues{2.0f};
    rl4::Vector<float> fromValues{0.0f};
    rl4::Vector<float> toValues{1.0f};
    float outputs[1] = {};
    rl4::ConditionalTable conditionals{std::move(inputIndices),
                                       std::move(outputIndices),
                                       std::move(fromValues),
                                       std::move(toValues),
                                       std::move(slopeValues),
                                       std::move(cutValues),
                                       inputCount,
                                       outputCount};
    conditionals.calculate(conditionalTableInputs.data(), outputs);
    const float expected[] = {1.0f};
    ASSERT_ELEMENTS_EQ(outputs, expected, 1ul);
}

TEST(ConditionalTableTest, OutputIsResetOnEachCalculation) {
    pma::AlignedMemoryResource amr;
    auto conditionals = ConditionalTableFactory::withSingleIODefaults(&amr);

    float outputs[2ul];
    const float expected[] = {0.6f};

    conditionals.calculate(conditionalTableInputs.data(), outputs);
    ASSERT_ELEMENTS_EQ(outputs, expected, 1ul);

    conditionals.calculate(conditionalTableInputs.data(), outputs);
    ASSERT_ELEMENTS_EQ(outputs, expected, 1ul);
}
