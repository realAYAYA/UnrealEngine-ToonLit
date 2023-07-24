// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// *INDENT-OFF*
#if defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_AVX)

#include "riglogic/joints/bpcm/BuilderCommon.h"
#include "riglogic/joints/bpcm/Consts.h"
#include "riglogic/joints/bpcm/strategies/AVX.h"
#include "riglogic/types/Aliases.h"

#include <cstdint>

namespace rl4 {

namespace bpcm {

class AVXJointsBuilder : public JointsBuilderCommon<std::uint16_t, block16Height, block8Height, trimd::avx::F256> {
    private:
        using Super = JointsBuilderCommon<std::uint16_t, block16Height, block8Height, trimd::avx::F256>;

    public:
        explicit AVXJointsBuilder(MemoryResource* memRes_) : Super{UniqueInstance<AVXJointCalculationStrategy<std::uint16_t>, CalculationStrategy>::with(memRes_).create(), memRes_} {
        }

        ~AVXJointsBuilder();
};

}  // namespace bpcm

}  // namespace rl4

#endif  // defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_AVX)
// *INDENT-ON*
