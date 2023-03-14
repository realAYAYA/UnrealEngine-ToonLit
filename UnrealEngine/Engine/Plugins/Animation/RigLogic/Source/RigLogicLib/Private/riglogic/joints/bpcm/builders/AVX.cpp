// Copyright Epic Games, Inc. All Rights Reserved.

#if !defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_AVX)

#include "riglogic/joints/bpcm/builders/AVX.h"

#include "riglogic/joints/bpcm/Consts.h"
#include "riglogic/joints/bpcm/strategies/AVX.h"

#include <pma/utils/ManagedInstance.h>

#include <cstddef>
#include <cstdint>

namespace rl4 {

namespace bpcm {

AVXJointsBuilder::AVXJointsBuilder(MemoryResource* memRes_) : FloatStorageBuilder{block16Height, block8Height, memRes_} {
    strategy = pma::UniqueInstance<AVXJointCalculationStrategy<float>, CalculationStrategy>::with(memRes_).create();
}

AVXJointsBuilder::~AVXJointsBuilder() = default;

}  // namespace bpcm

}  // namespace rl4

#endif  // !defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_AVX)
