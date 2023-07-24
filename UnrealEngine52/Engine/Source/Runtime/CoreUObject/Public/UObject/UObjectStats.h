// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "ProfilingDebugging/CsvProfilerConfig.h"
#include <atomic>

#if !defined(CSV_TRACK_UOBJECT_COUNT)
#define CSV_TRACK_UOBJECT_COUNT 0
#endif

#if CSV_PROFILER && CSV_TRACK_UOBJECT_COUNT
namespace UObjectStats
{
	extern COREUOBJECT_API std::atomic<int32> GUObjectCount;

	FORCEINLINE void IncrementUObjectCount()
	{
		GUObjectCount.fetch_add(1, std::memory_order_relaxed);
	}

	FORCEINLINE void DecrementUObjectCount()
	{
		GUObjectCount.fetch_sub(1, std::memory_order_relaxed);
	}

	FORCEINLINE int32 GetUObjectCount()
	{
		return GUObjectCount.load(std::memory_order_relaxed);
	}
}
#endif
