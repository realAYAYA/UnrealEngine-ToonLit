// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace rl4 {

/**
    @brief Implementation type for RigLogic calculations.
*/
enum class CalculationType {
    Scalar,  ///< scalar CPU algorithm
    SSE,  ///< vectorized (SSE) CPU algorithm
    AVX,  ///< vectorized (AVX) CPU algorithm (RigLogic must be built with AVX support,
          ///< otherwise it falls back to using the Scalar version)
    NEON  ///< vectorized (NEON) CPU algorithm (RigLogic must be built with NEON support,
          ///< otherwise it falls back to using the Scalar version)
};

struct Configuration {
    CalculationType calculationType = CalculationType::SSE;
    bool loadJoints = true;
    bool loadBlendShapes = true;
    bool loadAnimatedMaps = true;
    bool loadMachineLearnedBehavior = true;
};

}  // namespace rl4
