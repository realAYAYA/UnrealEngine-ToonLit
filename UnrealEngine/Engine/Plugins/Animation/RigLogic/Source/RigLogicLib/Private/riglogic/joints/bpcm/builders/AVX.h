// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// *INDENT-OFF*
#if !defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_AVX)

#include "riglogic/joints/bpcm/builders/Float.h"
#include "riglogic/types/Aliases.h"

namespace rl4 {

namespace bpcm {

class AVXJointsBuilder : public FloatStorageBuilder {
    public:
        explicit AVXJointsBuilder(MemoryResource* memRes_);
        ~AVXJointsBuilder();

};

}  // namespace bpcm

}  // namespace rl4

#endif  // !defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_AVX)
// *INDENT-ON*
