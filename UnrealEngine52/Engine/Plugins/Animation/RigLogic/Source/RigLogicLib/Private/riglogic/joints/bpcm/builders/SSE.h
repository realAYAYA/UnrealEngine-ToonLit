// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/system/simd/Detect.h"

// *INDENT-OFF*
#if !defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_SSE)

#include "riglogic/joints/bpcm/BuilderCommon.h"
#include "riglogic/joints/bpcm/Consts.h"
#include "riglogic/joints/bpcm/strategies/SSE.h"
#include "riglogic/types/Aliases.h"

namespace rl4 {

namespace bpcm {

class SSEJointsBuilder : public JointsBuilderCommon<float, block8Height, block4Height, trimd::sse::F128> {
    private:
        using Super = JointsBuilderCommon<float, block8Height, block4Height, trimd::sse::F128>;

    public:
        explicit SSEJointsBuilder(MemoryResource* memRes_) : Super{UniqueInstance<SSEJointCalculationStrategy<float>, CalculationStrategy>::with(memRes_).create(), memRes_} {
        }

        ~SSEJointsBuilder();

};

}  // namespace bpcm

}  // namespace rl4

#endif  // !defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_SSE)
// *INDENT-ON*
