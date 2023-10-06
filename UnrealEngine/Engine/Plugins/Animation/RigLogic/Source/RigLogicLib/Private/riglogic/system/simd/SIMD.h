// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/system/simd/Detect.h"

#if defined(RL_USE_HALF_FLOATS) && !defined(TRIMD_ENABLE_F16C)
    #define TRIMD_ENABLE_F16C
#endif  // RL_USE_HALF_FLOATS

#if defined(RL_BUILD_WITH_AVX)
    #if !defined(TRIMD_ENABLE_AVX)
        #define TRIMD_ENABLE_AVX
    #endif
    #if !defined(TRIMD_ENABLE_SSE)
        #define TRIMD_ENABLE_SSE
    #endif
#endif  // RL_BUILD_WITH_AVX

#if defined(RL_BUILD_WITH_SSE) && !defined(TRIMD_ENABLE_SSE)
    #define TRIMD_ENABLE_SSE
#endif  // RL_BUILD_WITH_SSE

#include <trimd/TRiMD.h>
