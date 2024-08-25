// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/system/simd/Detect.h"

#include "rltests/Defs.h"
#include "rltests/joints/bpcm/Assertions.h"
#include "rltests/joints/bpcm/BPCMFixturesBlock8.h"
#include "rltests/joints/bpcm/Helpers.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/joints/JointsBuilder.h"
#include "riglogic/joints/bpcm/Evaluator.h"
#include "riglogic/joints/bpcm/strategies/Block8.h"
#include "riglogic/riglogic/RigLogic.h"
#include "riglogic/system/simd/SIMD.h"

namespace {

template<typename TTestTypes>
class Block8JointStorageBuilderTest : public ::testing::Test {
    protected:
        template<typename TestTypes = TTestTypes>
        typename std::enable_if<std::tuple_size<TestTypes>::value == 0ul, void>::type buildStorage() {
        }

        template<typename TestTypes = TTestTypes>
        typename std::enable_if<std::tuple_size<TestTypes>::value != 0ul, void>::type buildStorage() {
            using TValue = typename std::tuple_element<0, TestTypes>::type;
            using TFVec = typename std::tuple_element<1, TestTypes>::type;
            using TCalculationType = typename std::tuple_element<2, TestTypes>::type;

            rl4::Configuration config{};
            config.calculationType = TCalculationType::get();
            auto builder = rl4::JointsBuilder::create(config, &memRes);
            builder->computeStorageRequirements(&reader);
            builder->allocateStorage(&reader);
            builder->fillStorage(&reader);
            auto joints = builder->build();
            auto jointsImpl = static_cast<rl4::bpcm::Evaluator<TValue>*>(joints.get());

            auto strategy = pma::UniqueInstance<rl4::bpcm::block8::Block8JointCalculationStrategy<TValue, TFVec>,
                                                rl4::bpcm::JointCalculationStrategy<TValue> >::with(&memRes).create();
            auto expected = block8::OptimizedStorage<TValue>::create(std::move(strategy), &memRes);

            rl4::bpcm::Evaluator<TValue>::Accessor::assertRawDataEqual(*jointsImpl, expected);
            rl4::bpcm::Evaluator<TValue>::Accessor::assertJointGroupsEqual(*jointsImpl, expected);
            rl4::bpcm::Evaluator<TValue>::Accessor::assertLODsEqual(*jointsImpl, expected);
        }

    protected:
        pma::AlignedMemoryResource memRes;
        block8::CanonicalReader reader;

};

}  // namespace

// Block-8 storage optimizer will execute only if RigLogic is built with AVX support, and AVX is chosen as calculation
// type. In all other cases- Block-4 storage optimizer will run.
using Block8StorageValueTypeList = ::testing::Types<
#if defined(RL_BUILD_WITH_AVX)
        std::tuple<StorageValueType, trimd::avx::F256, TCalculationType<rl4::CalculationType::AVX> >,
#endif  // RL_BUILD_WITH_AVX
    std::tuple<>
    >;
TYPED_TEST_SUITE(Block8JointStorageBuilderTest, Block8StorageValueTypeList, );

TYPED_TEST(Block8JointStorageBuilderTest, LayoutOptimization) {
    this->buildStorage();
}
