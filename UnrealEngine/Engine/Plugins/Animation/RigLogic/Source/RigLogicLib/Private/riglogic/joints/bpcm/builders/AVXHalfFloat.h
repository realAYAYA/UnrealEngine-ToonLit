// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// *INDENT-OFF*
#if defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_AVX)

#include "riglogic/joints/bpcm/JointsBuilderCommon.h"
#include "riglogic/types/Aliases.h"

#include <cstdint>

namespace rl4 {

namespace bpcm {

class AVXJointsBuilder : public JointsBuilderCommon<std::uint16_t> {
    public:
        explicit AVXJointsBuilder(MemoryResource* memRes_);
        ~AVXJointsBuilder();

    protected:
        void setValues(const dna::BehaviorReader* reader) override;

};

}  // namespace bpcm

}  // namespace rl4

#endif  // defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_AVX)
// *INDENT-ON*
