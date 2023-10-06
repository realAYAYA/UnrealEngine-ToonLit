// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/PlatformMemoryHelpers.h"

#include "CoreGlobals.h"
#include "HAL/PlatformMemory.h"

namespace PlatformMemoryHelpers
{

static uint64 CachedMemoryStatsAtFrame = -1;
static FPlatformMemoryStats CachedMemoryStats;

FPlatformMemoryStats GetFrameMemoryStats()
{
	if (IsInGameThread())
	{
		if (GFrameCounter != CachedMemoryStatsAtFrame)
		{
			CachedMemoryStatsAtFrame = GFrameCounter;
			CachedMemoryStats = FPlatformMemory::GetStats();
		}
		return CachedMemoryStats;
	}

	return FPlatformMemory::GetStats();
}

} //PlatformMemoryHelpers