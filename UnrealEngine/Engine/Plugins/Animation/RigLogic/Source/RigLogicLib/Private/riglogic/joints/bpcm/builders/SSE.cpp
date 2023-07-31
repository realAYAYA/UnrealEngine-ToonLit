// Copyright Epic Games, Inc. All Rights Reserved.

#if !defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_SSE)

#include "riglogic/joints/bpcm/builders/SSE.h"

#include "riglogic/joints/bpcm/Consts.h"
#include "riglogic/joints/bpcm/strategies/SSE.h"

#include <pma/utils/ManagedInstance.h>

#include <cstddef>
#include <cstdint>

namespace rl4 {

namespace bpcm {

SSEJointsBuilder::SSEJointsBuilder(MemoryResource* memRes_) : FloatStorageBuilder{block8Height, block4Height, memRes_} {
    strategy = pma::UniqueInstance<SSEJointCalculationStrategy<float>, CalculationStrategy>::with(memRes).create();
}

SSEJointsBuilder::~SSEJointsBuilder() = default;

}  // namespace bpcm

}  // namespace rl4

#endif  // !defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_SSE)
