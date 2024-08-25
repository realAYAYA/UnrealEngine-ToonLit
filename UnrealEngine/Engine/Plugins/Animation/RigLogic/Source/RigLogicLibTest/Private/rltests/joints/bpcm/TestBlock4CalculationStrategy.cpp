// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/system/simd/Detect.h"

#include "rltests/Defs.h"
#include "rltests/controls/ControlFixtures.h"
#include "rltests/joints/bpcm/BPCMFixturesBlock4.h"
#include "rltests/joints/bpcm/Helpers.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/joints/bpcm/Evaluator.h"
#include "riglogic/joints/bpcm/strategies/Block4.h"
#include "riglogic/system/simd/SIMD.h"

namespace {

template<typename TTestTypes>
class Block4JointCalculationStrategyTest : public ::testing::TestWithParam<StrategyTestParams> {
    protected:
        void SetUp() override {
            Block4JointCalculationStrategyTest::SetUpImpl();
        }

        template<typename TestTypes = TTestTypes>
        typename std::enable_if<std::tuple_size<TestTypes>::value != 0ul, void>::type SetUpImpl() {
            using T = typename std::tuple_element<0, TestTypes>::type;
            using TFVec = typename std::tuple_element<1, TestTypes>::type;
            using TStrategyTestParams = typename std::tuple_element<2, TestTypes>::type;
            params.lod = TStrategyTestParams::lod();

            using CalculationStrategyBase = rl4::bpcm::JointCalculationStrategy<T>;
            using CalculationStrategy = rl4::bpcm::block4::Block4JointCalculationStrategy<T, TFVec>;
            strategy = pma::UniqueInstance<CalculationStrategy, CalculationStrategyBase>::with(&memRes).create();
        }

        template<typename TestTypes = TTestTypes>
        typename std::enable_if<std::tuple_size<TestTypes>::value == 0ul, void>::type SetUpImpl() {
        }

        template<typename TArray>
        void execute(const rl4::bpcm::Evaluator<StorageValueType>& joints, const TArray& expected, OutputScope scope) {
            if (strategy == nullptr) {
                return;
            }

            auto outputInstance = joints.createInstance(&memRes);
            auto outputBuffer = outputInstance->getOutputBuffer();

            auto inputInstanceFactory =
                ControlsFactory::getInstanceFactory(0, static_cast<std::uint16_t>(block4::input::values.size()), 0, 0);
            auto inputInstance = inputInstanceFactory(&memRes);
            auto inputBuffer = inputInstance->getInputBuffer();
            std::copy(block4::input::values.begin(), block4::input::values.end(), inputBuffer.begin());

            rl4::ConstArrayView<float> expectedView{expected[scope.lod].data() + scope.offset, scope.size};
            rl4::ConstArrayView<float> outputView{outputBuffer.data() + scope.offset, scope.size};
            joints.calculate(inputInstance.get(), outputInstance.get(), scope.lod);
            ASSERT_EQ(outputView, expectedView);
        }

    protected:
        pma::AlignedMemoryResource memRes;
        block4::OptimizedStorage<StorageValueType>::StrategyPtr strategy;
        StrategyTestParams params;

};

}  // namespace

using Block4JointCalculationTypeList = ::testing::Types<
#if defined(RL_BUILD_WITH_AVX) || defined(RL_BUILD_WITH_SSE)
        std::tuple<StorageValueType, trimd::sse::F128, TStrategyTestParams<0> >,
        std::tuple<StorageValueType, trimd::sse::F128, TStrategyTestParams<1> >,
        std::tuple<StorageValueType, trimd::sse::F128, TStrategyTestParams<2> >,
        std::tuple<StorageValueType, trimd::sse::F128, TStrategyTestParams<3> >,
#endif  // RL_BUILD_WITH_AVX || RL_BUILD_WITH_SSE
#if defined(RL_BUILD_WITH_NEON)
        std::tuple<StorageValueType, trimd::neon::F128, TStrategyTestParams<0> >,
        std::tuple<StorageValueType, trimd::neon::F128, TStrategyTestParams<1> >,
        std::tuple<StorageValueType, trimd::neon::F128, TStrategyTestParams<2> >,
        std::tuple<StorageValueType, trimd::neon::F128, TStrategyTestParams<3> >,
#endif  // RL_BUILD_WITH_NEON
#if !defined(RL_USE_HALF_FLOATS)
        std::tuple<StorageValueType, trimd::scalar::F128, TStrategyTestParams<0> >,
        std::tuple<StorageValueType, trimd::scalar::F128, TStrategyTestParams<1> >,
        std::tuple<StorageValueType, trimd::scalar::F128, TStrategyTestParams<2> >,
        std::tuple<StorageValueType, trimd::scalar::F128, TStrategyTestParams<3> >,
#endif  // RL_USE_HALF_FLOATS
    std::tuple<>
    >;

TYPED_TEST_SUITE(Block4JointCalculationStrategyTest, Block4JointCalculationTypeList, );

TYPED_TEST(Block4JointCalculationStrategyTest, Block4Padded) {
    const OutputScope scope{this->params.lod, 0ul, 1ul};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(this->strategy), 0u, &this->memRes);
    this->execute(joints, block4::output::valuesPerLOD, scope);
}

TYPED_TEST(Block4JointCalculationStrategyTest, Block4Exact) {
    const OutputScope scope{this->params.lod, 1ul, 4ul};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(this->strategy), 1u, &this->memRes);
    this->execute(joints, block4::output::valuesPerLOD, scope);
}

TYPED_TEST(Block4JointCalculationStrategyTest, Block8Padded) {
    const OutputScope scope{this->params.lod, 5ul, 7ul};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(this->strategy), 2u, &this->memRes);
    this->execute(joints, block4::output::valuesPerLOD, scope);
}

TYPED_TEST(Block4JointCalculationStrategyTest, Block8Exact) {
    const OutputScope scope{this->params.lod, 12ul, 8ul};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(this->strategy), 3u, &this->memRes);
    this->execute(joints, block4::output::valuesPerLOD, scope);
}

TYPED_TEST(Block4JointCalculationStrategyTest, Block12Padded) {
    const OutputScope scope{this->params.lod, 20ul, 9ul};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(this->strategy), 4u, &this->memRes);
    this->execute(joints, block4::output::valuesPerLOD, scope);
}

TYPED_TEST(Block4JointCalculationStrategyTest, Block12Exact) {
    const OutputScope scope{this->params.lod, 29ul, 12ul};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(this->strategy), 5u, &this->memRes);
    this->execute(joints, block4::output::valuesPerLOD, scope);
}

TYPED_TEST(Block4JointCalculationStrategyTest, Block16Padded) {
    const OutputScope scope{this->params.lod, 41ul, 14ul};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(this->strategy), 6u, &this->memRes);
    this->execute(joints, block4::output::valuesPerLOD, scope);
}

TYPED_TEST(Block4JointCalculationStrategyTest, Block16Exact) {
    const OutputScope scope{this->params.lod, 55ul, 16ul};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(this->strategy), 7u, &this->memRes);
    this->execute(joints, block4::output::valuesPerLOD, scope);
}

TYPED_TEST(Block4JointCalculationStrategyTest, MultipleBlocks) {
    const OutputScope scope{this->params.lod, 0ul, block4::output::valuesPerLOD.front().size()};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(this->strategy), &this->memRes);
    this->execute(joints, block4::output::valuesPerLOD, scope);
}

TYPED_TEST(Block4JointCalculationStrategyTest, InputRegionA) {
    const OutputScope scope{this->params.lod, 71ul, 2ul};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(this->strategy), 8u, &this->memRes);
    this->execute(joints, block4::output::valuesPerLOD, scope);
}

TYPED_TEST(Block4JointCalculationStrategyTest, InputRegionB) {
    const OutputScope scope{this->params.lod, 73ul, 2ul};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(this->strategy), 9u, &this->memRes);
    this->execute(joints, block4::output::valuesPerLOD, scope);
}
