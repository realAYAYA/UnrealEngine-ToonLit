// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/joints/JointsBuilder.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/joints/bpcm/Builder.h"
#include "riglogic/system/simd/Detect.h"

namespace rl4 {

JointsBuilder::~JointsBuilder() = default;

JointsBuilder::Pointer JointsBuilder::create(Configuration config, MemoryResource* memRes) {
    #if defined(__clang__)
        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wunused-local-typedef"
    #endif
    #if !defined(__clang__) && defined(__GNUC__)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wunused-local-typedefs"
    #endif
    #ifdef RL_USE_HALF_FLOATS
        using StorageType = std::uint16_t;
    #else
        using StorageType = float;
    #endif  // RL_USE_HALF_FLOATS
    #if !defined(__clang__) && defined(__GNUC__)
        #pragma GCC diagnostic pop
    #endif
    #if defined(__clang__)
        #pragma clang diagnostic pop
    #endif

    // Work around unused parameter warning when building without SSE and AVX
    static_cast<void>(config);
    #ifdef RL_BUILD_WITH_SSE
        if (config.calculationType == CalculationType::SSE) {
            using SSEJointsBuilder = bpcm::BPCMJointsBuilder<StorageType, trimd::sse::F128>;
            return UniqueInstance<SSEJointsBuilder, JointsBuilder>::with(memRes).create(memRes);
        }
    #endif  // RL_BUILD_WITH_SSE
    #ifdef RL_BUILD_WITH_AVX
        if (config.calculationType == CalculationType::AVX) {
            using AVXJointsBuilder = bpcm::BPCMJointsBuilder<StorageType, trimd::avx::F256>;
            return UniqueInstance<AVXJointsBuilder, JointsBuilder>::with(memRes).create(memRes);
        }
    #endif  // RL_BUILD_WITH_AVX
    #ifdef RL_BUILD_WITH_NEON
        if (config.calculationType == CalculationType::NEON) {
            using NEONJointsBuilder = bpcm::BPCMJointsBuilder<StorageType, trimd::neon::F128>;
            return UniqueInstance<NEONJointsBuilder, JointsBuilder>::with(memRes).create(memRes);
        }
    #endif  // RL_BUILD_WITH_NEON
    using ScalarJointsBuilder = bpcm::BPCMJointsBuilder<float, trimd::scalar::F128>;
    return UniqueInstance<ScalarJointsBuilder, JointsBuilder>::with(memRes).create(memRes);
}

}  // namespace rl4
