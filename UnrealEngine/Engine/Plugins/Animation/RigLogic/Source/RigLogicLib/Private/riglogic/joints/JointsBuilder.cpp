// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/joints/JointsBuilder.h"

#include "riglogic/joints/bpcm/builders/Scalar.h"
#ifdef RL_BUILD_WITH_AVX
    #ifdef RL_USE_HALF_FLOATS
        #include "riglogic/joints/bpcm/builders/AVXHalfFloat.h"
    #else
        #include "riglogic/joints/bpcm/builders/AVX.h"
    #endif  // RL_USE_HALF_FLOATS
#endif  // RL_BUILD_WITH_AVX
#ifdef RL_BUILD_WITH_SSE
    #ifdef RL_USE_HALF_FLOATS
        #include "riglogic/joints/bpcm/builders/SSEHalfFloat.h"
    #else
        #include "riglogic/joints/bpcm/builders/SSE.h"
    #endif  // RL_USE_HALF_FLOATS
#endif  // RL_BUILD_WITH_SSE

#include <pma/utils/ManagedInstance.h>

namespace rl4 {

JointsBuilder::~JointsBuilder() = default;

JointsBuilder::JointsBuilderPtr JointsBuilder::create(Configuration config, MemoryResource* memRes) {
    // Work around unused parameter warning when building without SSE and AVX
    static_cast<void>(config);
    #ifdef RL_BUILD_WITH_SSE
        if (config.calculationType == CalculationType::SSE) {
            return pma::UniqueInstance<bpcm::SSEJointsBuilder, JointsBuilder>::with(memRes).create(memRes);
        }
    #endif  // RL_BUILD_WITH_SSE
    #ifdef RL_BUILD_WITH_AVX
        if (config.calculationType == CalculationType::AVX) {
            return pma::UniqueInstance<bpcm::AVXJointsBuilder, JointsBuilder>::with(memRes).create(memRes);
        }
    #endif  // RL_BUILD_WITH_AVX
    return pma::UniqueInstance<bpcm::ScalarJointsBuilder, JointsBuilder>::with(memRes).create(memRes);
}

}  // namespace rl4
