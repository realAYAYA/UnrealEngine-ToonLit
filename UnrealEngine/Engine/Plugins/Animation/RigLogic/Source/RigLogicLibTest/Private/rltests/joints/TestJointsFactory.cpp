// Copyright Epic Games, Inc. All Rights Reserved.

#include "rltests/Defs.h"
#include "rltests/joints/bpcm/JointFixturesBlock4.h"

#include "riglogic/RigLogic.h"
#include "riglogic/joints/JointsFactory.h"

#include <pma/resources/AlignedMemoryResource.h>

TEST(ScalarJointsFactoryTest, NeutralJointsAreCopied) {
    pma::AlignedMemoryResource memRes;
    block4::CanonicalReader reader;
    rl4::Configuration config{rl4::RigLogic::CalculationType::Scalar};
    auto joints = rl4::JointsFactory::create(config, &reader, &memRes);
    const float expected[] = {
        0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 1.0f, 1.0f, 1.0f,
        6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 1.0f, 1.0f, 1.0f,
        12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f, 1.0f, 1.0f, 1.0f
    };
    ASSERT_ELEMENTS_EQ(joints->getRawNeutralValues(), expected, 27ul);
}
