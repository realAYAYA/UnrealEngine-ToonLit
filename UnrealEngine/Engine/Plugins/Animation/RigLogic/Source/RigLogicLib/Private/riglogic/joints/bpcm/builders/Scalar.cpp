// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/joints/bpcm/builders/Scalar.h"

#include "riglogic/joints/bpcm/Consts.h"
#include "riglogic/joints/bpcm/strategies/Scalar.h"

#include <pma/utils/ManagedInstance.h>

#include <cstddef>
#include <cstdint>

namespace rl4 {

namespace bpcm {

ScalarJointsBuilder::ScalarJointsBuilder(MemoryResource* memRes_) : FloatStorageBuilder{block8Height, block4Height,
                                                                                        memRes_} {
    strategy = pma::UniqueInstance<ScalarJointCalculationStrategy, CalculationStrategy>::with(memRes).create();
}

ScalarJointsBuilder::~ScalarJointsBuilder() = default;

}  // namespace bpcm

}  // namespace rl4
