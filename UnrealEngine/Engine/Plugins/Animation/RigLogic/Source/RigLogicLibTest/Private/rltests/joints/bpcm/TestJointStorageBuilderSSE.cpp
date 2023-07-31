// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef RL_BUILD_WITH_SSE

#include "rltests/Defs.h"
#include "rltests/joints/bpcm/Assertions.h"
#include "rltests/joints/bpcm/JointFixturesBlock4.h"

#include "riglogic/joints/JointsBuilder.h"
#include "riglogic/joints/bpcm/JointsEvaluator.h"
#include "riglogic/joints/bpcm/strategies/SSE.h"
#include "riglogic/riglogic/RigLogic.h"

#include <pma/resources/AlignedMemoryResource.h>
#include <pma/utils/ManagedInstance.h>

namespace {

template<typename TValue>
class SSEJointStorageBuilderTest : public ::testing::Test {
    protected:
        void buildStorage() {
            rl4::Configuration config{rl4::RigLogic::CalculationType::SSE};
            auto builder = rl4::JointsBuilder::create(config, &memRes);
            builder->allocateStorage(&reader);
            builder->fillStorage(&reader);
            auto joints = builder->build();
            auto jointsImpl = static_cast<rl4::bpcm::Evaluator<TValue>*>(joints.get());

            auto strategy = pma::UniqueInstance<rl4::bpcm::SSEJointCalculationStrategy<TValue>,
                                                rl4::bpcm::JointCalculationStrategy<TValue> >::with(&memRes).create();
            auto expected = block4::OptimizedStorage<TValue>::create(std::move(strategy), &memRes);

            rl4::bpcm::Evaluator<TValue>::Accessor::assertRawDataEqual(*jointsImpl, expected);
            rl4::bpcm::Evaluator<TValue>::Accessor::assertJointGroupsEqual(*jointsImpl, expected);
            rl4::bpcm::Evaluator<TValue>::Accessor::assertLODsEqual(*jointsImpl, expected);
        }

    protected:
        pma::AlignedMemoryResource memRes;
        block4::CanonicalReader reader;

};

}  // namespace

using StorageValueTypeList = ::testing::Types<StorageValueType>;
TYPED_TEST_SUITE(SSEJointStorageBuilderTest, StorageValueTypeList, );

TYPED_TEST(SSEJointStorageBuilderTest, LayoutOptimization) {
    this->buildStorage();
}

#endif  // RL_BUILD_WITH_SSE
