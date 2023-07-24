// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/joints/bpcm/builders/SSEHalfFloat.h"

// *INDENT-OFF*
#if defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_SSE)

namespace rl4 {

namespace bpcm {

SSEJointsBuilder::~SSEJointsBuilder() = default;

}  // namespace bpcm

}  // namespace rl4

#endif  // defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_SSE)
// *INDENT-ON*
