// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef RL_BUILD_WITH_AVX

#include "rltests/Defs.h"
#include "rltests/joints/bpcm/Helpers.h"
#include "rltests/joints/bpcm/JointFixturesBlock8.h"

#include "riglogic/joints/bpcm/JointsEvaluator.h"
#include "riglogic/joints/bpcm/strategies/AVX.h"
#include "riglogic/types/Aliases.h"

#include <pma/resources/AlignedMemoryResource.h>
#include <pma/utils/ManagedInstance.h>

namespace {

class AVXJointCalculationStrategyTest : public ::testing::TestWithParam<StrategyTestParams> {
    protected:
        void SetUp() {
            strategy = pma::UniqueInstance<rl4::bpcm::AVXJointCalculationStrategy<StorageValueType>,
                                           rl4::bpcm::JointCalculationStrategy<StorageValueType> >::with(&memRes).create();
        }

        template<typename TArray>
        void execute(const rl4::bpcm::Evaluator<StorageValueType>& joints, const TArray& expected, OutputScope scope) {
            rl4::ConstArrayView<float> expectedView{expected[scope.lod].data() + scope.offset, scope.size};
            rl4::AlignedVector<float> outputs{expected.front().size(), {}, & memRes};
            rl4::ConstArrayView<float> inputView{block8::input::values};
            rl4::ConstArrayView<float> outputView{outputs.data() + scope.offset, scope.size};
            joints.calculate(inputView, rl4::ArrayView<float>{outputs}, scope.lod);
            ASSERT_EQ(outputView, expectedView);
        }

    protected:
        pma::AlignedMemoryResource memRes;
        block8::OptimizedStorage<StorageValueType>::StrategyPtr strategy;

};

}  // namespace

TEST_P(AVXJointCalculationStrategyTest, Block8Padded) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 0ul, 5ul};
    auto joints = block8::OptimizedStorage<StorageValueType>::create(std::move(strategy), 0u, &memRes);
    execute(joints, block8::output::valuesPerLOD, scope);
}

TEST_P(AVXJointCalculationStrategyTest, Block8Exact) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 5ul, 8ul};
    auto joints = block8::OptimizedStorage<StorageValueType>::create(std::move(strategy), 1u, &memRes);
    execute(joints, block8::output::valuesPerLOD, scope);
}

TEST_P(AVXJointCalculationStrategyTest, Block16Padded) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 13ul, 9ul};
    auto joints = block8::OptimizedStorage<StorageValueType>::create(std::move(strategy), 2u, &memRes);
    execute(joints, block8::output::valuesPerLOD, scope);
}

TEST_P(AVXJointCalculationStrategyTest, Block16Exact) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 22ul, 16ul};
    auto joints = block8::OptimizedStorage<StorageValueType>::create(std::move(strategy), 3u, &memRes);
    execute(joints, block8::output::valuesPerLOD, scope);
}

TEST_P(AVXJointCalculationStrategyTest, Block24Padded) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 38ul, 17ul};
    auto joints = block8::OptimizedStorage<StorageValueType>::create(std::move(strategy), 4u, &memRes);
    execute(joints, block8::output::valuesPerLOD, scope);
}

TEST_P(AVXJointCalculationStrategyTest, Block24Exact) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 55ul, 24ul};
    auto joints = block8::OptimizedStorage<StorageValueType>::create(std::move(strategy), 5u, &memRes);
    execute(joints, block8::output::valuesPerLOD, scope);
}

TEST_P(AVXJointCalculationStrategyTest, Block32Padded) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 79ul, 25ul};
    auto joints = block8::OptimizedStorage<StorageValueType>::create(std::move(strategy), 6u, &memRes);
    execute(joints, block8::output::valuesPerLOD, scope);
}

TEST_P(AVXJointCalculationStrategyTest, Block32Exact) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 104ul, 32ul};
    auto joints = block8::OptimizedStorage<StorageValueType>::create(std::move(strategy), 7u, &memRes);
    execute(joints, block8::output::valuesPerLOD, scope);
}

TEST_P(AVXJointCalculationStrategyTest, MultipleBlocks) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 0ul, block8::output::valuesPerLOD.front().size()};
    auto joints = block8::OptimizedStorage<StorageValueType>::create(std::move(strategy), &memRes);
    execute(joints, block8::output::valuesPerLOD, scope);
}

INSTANTIATE_TEST_SUITE_P(AVXJointCalculationStrategyTest, AVXJointCalculationStrategyTest, ::testing::Values(
                             StrategyTestParams{0u},
                             StrategyTestParams{1u},
                             StrategyTestParams{2u},
                             StrategyTestParams{3u}));

#endif  // RL_BUILD_WITH_AVX
