// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Build.h"
#include "HAL/Platform.h"
#include "HAL/CriticalSection.h"

#ifndef UE_MUTABLE_TRACK_ALLOCATOR_MEMORY_PEAK
	#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT || UE_BUILD_TEST
		#define UE_MUTABLE_TRACK_ALLOCATOR_MEMORY_PEAK 1
	#else
		#define UE_MUTABLE_TRACK_ALLOCATOR_MEMORY_PEAK 0
	#endif
#endif

namespace mu
{
#if UE_MUTABLE_TRACK_ALLOCATOR_MEMORY_PEAK 
	struct MUTABLERUNTIME_API FGlobalMemoryCounter
	{
	private:
		 static inline SSIZE_T AbsoluteCounter   { 0 };
		 static inline SSIZE_T AbsolutePeakValue { 0 };
		 static inline SSIZE_T Counter           { 0 };
		 static inline SSIZE_T PeakValue         { 0 };
		
		 static inline FCriticalSection Mutex {};

	public:
		 static void Update(SSIZE_T Differential);
		 static void Zero();
		 static void Restore();
		 static SSIZE_T GetPeak();
		 static SSIZE_T GetCounter();
		 static SSIZE_T GetAbsolutePeak();
		 static SSIZE_T GetAbsoluteCounter();
	};
#else
	struct MUTABLERUNTIME_API FGlobalMemoryCounter
	{
		 //static void Update(SSIZE_T Differential);
		 static void Zero();
		 static void Restore();
		 static SSIZE_T GetPeak();
		 static SSIZE_T GetCounter();
		 static SSIZE_T GetAbsolutePeak();
		 static SSIZE_T GetAbsoluteCounter();
	};
#endif
}
