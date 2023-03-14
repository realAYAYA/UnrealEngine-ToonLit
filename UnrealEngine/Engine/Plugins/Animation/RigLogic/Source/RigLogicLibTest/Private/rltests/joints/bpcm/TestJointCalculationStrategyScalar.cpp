// Copyright Epic Games, Inc. All Rights Reserved.

#include "rltests/Defs.h"
#include "rltests/joints/bpcm/Helpers.h"
#include "rltests/joints/bpcm/JointFixturesBlock4.h"

#include "riglogic/joints/bpcm/JointsEvaluator.h"
#include "riglogic/joints/bpcm/strategies/Scalar.h"
#include "riglogic/types/Aliases.h"

#include <pma/resources/AlignedMemoryResource.h>
#include <pma/utils/ManagedInstance.h>

namespace {

class ScalarJointCalculationStrategyTest : public ::testing::TestWithParam<StrategyTestParams> {
    protected:
        void SetUp() {
            strategy = pma::UniqueInstance<rl4::bpcm::ScalarJointCalculationStrategy,
                                           rl4::bpcm::JointCalculationStrategy<float> >::with(&memRes).create();
        }

        template<typename TArray>
        void execute(const rl4::bpcm::Evaluator<float>& joints, const TArray& expected, OutputScope scope) {
            rl4::ConstArrayView<float> expectedView{expected[scope.lod].data() + scope.offset, scope.size};
            rl4::AlignedVector<float> outputs{expected.front().size(), {}, & memRes};
            rl4::ConstArrayView<float> inputView{block4::input::values};
            rl4::ConstArrayView<float> outputView{outputs.data() + scope.offset, scope.size};
            joints.calculate(inputView, rl4::ArrayView<float>{outputs}, scope.lod);
            ASSERT_EQ(outputView, expectedView);
        }

    protected:
        pma::AlignedMemoryResource memRes;
        block4::OptimizedStorage<float>::StrategyPtr strategy;
};

}  // namespace

TEST_P(ScalarJointCalculationStrategyTest, Block4Padded) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 0ul, 1ul};
    auto joints = block4::OptimizedStorage<float>::create(std::move(strategy), 0u, &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

TEST_P(ScalarJointCalculationStrategyTest, Block4Exact) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 1ul, 4ul};
    auto joints = block4::OptimizedStorage<float>::create(std::move(strategy), 1u, &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

TEST_P(ScalarJointCalculationStrategyTest, Block8Padded) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 5ul, 7ul};
    auto joints = block4::OptimizedStorage<float>::create(std::move(strategy), 2u, &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

TEST_P(ScalarJointCalculationStrategyTest, Block8Exact) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 12ul, 8ul};
    auto joints = block4::OptimizedStorage<float>::create(std::move(strategy), 3u, &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

TEST_P(ScalarJointCalculationStrategyTest, Block12Padded) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 20ul, 9ul};
    auto joints = block4::OptimizedStorage<float>::create(std::move(strategy), 4u, &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

TEST_P(ScalarJointCalculationStrategyTest, Block12Exact) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 29ul, 12ul};
    auto joints = block4::OptimizedStorage<float>::create(std::move(strategy), 5u, &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

TEST_P(ScalarJointCalculationStrategyTest, Block16Padded) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 41ul, 14ul};
    auto joints = block4::OptimizedStorage<float>::create(std::move(strategy), 6u, &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

TEST_P(ScalarJointCalculationStrategyTest, Block16Exact) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 55ul, 16ul};
    auto joints = block4::OptimizedStorage<float>::create(std::move(strategy), 7u, &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

TEST_P(ScalarJointCalculationStrategyTest, MultipleBlocks) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 0ul, block4::output::valuesPerLOD.front().size()};
    auto joints = block4::OptimizedStorage<float>::create(std::move(strategy), &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

TEST_P(ScalarJointCalculationStrategyTest, InputRegionA) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 71ul, 2ul};
    auto joints = block4::OptimizedStorage<float>::create(std::move(strategy), 8u, &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

TEST_P(ScalarJointCalculationStrategyTest, InputRegionB) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 73ul, 2ul};
    auto joints = block4::OptimizedStorage<float>::create(std::move(strategy), 9u, &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

INSTANTIATE_TEST_SUITE_P(ScalarJointCalculationStrategyTest, ScalarJointCalculationStrategyTest, ::testing::Values(
                             StrategyTestParams{0u},
                             StrategyTestParams{1u},
                             StrategyTestParams{2u},
                             StrategyTestParams{3u}
                             ));
