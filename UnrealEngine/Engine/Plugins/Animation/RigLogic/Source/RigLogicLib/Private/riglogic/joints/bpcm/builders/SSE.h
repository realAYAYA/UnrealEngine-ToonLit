// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// *INDENT-OFF*
#if !defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_SSE)

#include "riglogic/joints/bpcm/builders/Float.h"
#include "riglogic/types/Aliases.h"

namespace rl4 {

namespace bpcm {

class SSEJointsBuilder : public FloatStorageBuilder {
    public:
        explicit SSEJointsBuilder(MemoryResource* memRes_);
        ~SSEJointsBuilder();
};

}  // namespace bpcm

}  // namespace rl4

#endif  // !defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_SSE)
// *INDENT-ON*
