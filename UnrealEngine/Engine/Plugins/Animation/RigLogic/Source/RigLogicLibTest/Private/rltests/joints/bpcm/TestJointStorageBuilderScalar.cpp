// Copyright Epic Games, Inc. All Rights Reserved.

#include "rltests/Defs.h"
#include "rltests/joints/bpcm/Assertions.h"
#include "rltests/joints/bpcm/JointFixturesBlock4.h"

#include "riglogic/joints/JointsBuilder.h"
#include "riglogic/joints/JointsFactory.h"
#include "riglogic/joints/bpcm/JointsEvaluator.h"
#include "riglogic/joints/bpcm/strategies/Scalar.h"
#include "riglogic/riglogic/RigLogic.h"

#include <pma/resources/AlignedMemoryResource.h>
#include <pma/utils/ManagedInstance.h>

namespace {

class ScalarJointStorageBuilderTest : public ::testing::Test {
    protected:
        void buildStorage() {
            rl4::Configuration config{rl4::RigLogic::CalculationType::Scalar};
            auto builder = rl4::JointsBuilder::create(config, &memRes);
            builder->allocateStorage(&reader);
            builder->fillStorage(&reader);
            auto joints = builder->build();
            auto jointsImpl = static_cast<rl4::bpcm::Evaluator<float>*>(joints.get());

            auto strategy = pma::UniqueInstance<rl4::bpcm::ScalarJointCalculationStrategy,
                                                rl4::bpcm::JointCalculationStrategy<float> >::with(&memRes).create();
            auto expected = block4::OptimizedStorage<float>::create(std::move(strategy), &memRes);

            rl4::bpcm::Evaluator<float>::Accessor::assertRawDataEqual(*jointsImpl, expected);
            rl4::bpcm::Evaluator<float>::Accessor::assertJointGroupsEqual(*jointsImpl, expected);
            rl4::bpcm::Evaluator<float>::Accessor::assertLODsEqual(*jointsImpl, expected);
        }

    protected:
        pma::AlignedMemoryResource memRes;
        block4::CanonicalReader reader;

};

}  // namespace

TEST_F(ScalarJointStorageBuilderTest, LayoutOptimization) {
    this->buildStorage();
}
