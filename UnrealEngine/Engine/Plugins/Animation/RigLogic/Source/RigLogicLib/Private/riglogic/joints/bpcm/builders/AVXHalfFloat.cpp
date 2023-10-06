// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/joints/bpcm/builders/AVXHalfFloat.h"

// *INDENT-OFF*
#if defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_AVX)

namespace rl4 {

namespace bpcm {

AVXJointsBuilder::~AVXJointsBuilder() = default;

}  // namespace bpcm

}  // namespace rl4

#endif  // defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_AVX)
// *INDENT-ON*
