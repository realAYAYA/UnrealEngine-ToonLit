// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_1 
#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"
#include "BufferVectorOperations.h"
#endif

#define USE_AUDIO_DEBUGGING UE_BUILD_DEBUG

SIGNALPROCESSING_API void BreakWhenAudible(float* InBuffer, int32 NumSamples);
SIGNALPROCESSING_API void BreakWhenTooLoud(float* InBuffer, int32 NumSamples);

#if USE_AUDIO_DEBUGGING
#define BREAK_WHEN_AUDIBLE(Ptr, Num) BreakWhenAudible(Ptr, Num);
#define BREAK_WHEN_TOO_LOUD(Ptr, Num) BreakWhenTooLoud(Ptr, Num);
#else
#define BREAK_WHEN_AUDIBLE(Ptr, Num) 
#define BREAK_WHEN_TOO_LOUD(Ptr, Num)
#endif
