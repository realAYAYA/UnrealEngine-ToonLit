// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <stdint.h>

enum class EScudoEventType : uint8_t
{
	Invalid = 0,
	Alloc = 1, // Ptr != 0, Size != 0, Alignment might be 0 if not specified
	Free = 2,  // Ptr != 0, Size == 0, Alignment == 0
	Time = 3   // Time in stored in Ptr, Size == 0, Alignment == 0
};

typedef void (*ScudoMemoryTraceHook)(EScudoEventType EventType, uint64_t Ptr, uint64_t Size, uint32_t Alignment, const void* Frames, uint32_t FrameCount);

extern "C" void ScudoMemoryTrace_SetHook(ScudoMemoryTraceHook TraceHook);
