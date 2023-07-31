// Copyright Epic Games, Inc. All Rights Reserved.

/**
*
* A lightweight profiler that can output logs compatible with Google Chrome tracing visualizer.
* Captured events are written as a flat array (fixed size ring buffer), without any kind of aggregation.
* Tracing events may be added from multiple threads simultaneously. 
* Old trace events are overwritten when ring buffer wraps.
*/

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "HAL/PlatformAtomics.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "UObject/NameTypes.h"

#ifndef USE_TRACING_PROFILER_IN_TEST_BUILD
#define USE_TRACING_PROFILER_IN_TEST_BUILD 1
#endif

#define TRACING_PROFILER (WITH_ENGINE && !UE_BUILD_SHIPPING && (!UE_BUILD_TEST || USE_TRACING_PROFILER_IN_TEST_BUILD))

#if TRACING_PROFILER

class UE_DEPRECATED(4.26, "Please use Unreal Insights for profiling") FTracingProfiler
{
private:

	static FTracingProfiler* Instance;
	FTracingProfiler();

public:

	enum class EEventType
	{
		CPU,
		GPU,
	};

	struct FEvent
	{
		FName Name;
		uint32 FrameNumber;
		uint32 SessionId;
		EEventType Type;
		union
		{
			struct
			{
				uint64 BeginMicroseconds;
				uint64 EndMicroseconds;
				uint64 GPUIndex;
			} GPU;

			struct
			{
				uint64 BeginCycles;
				uint64 EndCycles;
				uint64 ThreadId;
			} CPU;
		};
	};

	static RENDERCORE_API FTracingProfiler* Get();

	RENDERCORE_API void Init();

	uint32 AddCPUEvent(FName Name,
		uint64 TimestampBeginCycles,
		uint64 TimestempEndCycles,
		uint32 ThreadId,
		uint32 FrameNumber)
	{
		FEvent Event;
		Event.Name = Name;
		Event.Type = EEventType::CPU;
		Event.FrameNumber = FrameNumber;
		Event.SessionId = SessionId;
		Event.CPU.BeginCycles = TimestampBeginCycles;
		Event.CPU.EndCycles = TimestempEndCycles;
		Event.CPU.ThreadId = ThreadId;
		return AddEvent(Event);
	}

	uint32 AddGPUEvent(FName Name,
		uint64 TimestempBeginMicroseconds,
		uint64 TimestampEndMicroseconds,
		uint64 GPUIndex,
		uint32 FrameNumber)
	{
		FEvent Event;
		Event.Name = Name;
		Event.Type = EEventType::GPU;
		Event.FrameNumber = FrameNumber;
		Event.SessionId = SessionId;
		Event.GPU.BeginMicroseconds = TimestempBeginMicroseconds;
		Event.GPU.EndMicroseconds = TimestampEndMicroseconds;
		Event.GPU.GPUIndex = GPUIndex;
		return AddEvent(Event);
	}

	uint32 AddEvent(const FEvent& Event)
	{
		if (!bCapturing)
		{
			return ~0u;
		}

		uint32 EventId = (FPlatformAtomics::InterlockedIncrement(&EventAtomicConter)-1) % MaxNumCapturedEvents;
		CapturedEvents[EventId] = Event;
		return EventId;
	}

	RENDERCORE_API int32 GetCaptureFrameNumber();

	RENDERCORE_API void BeginCapture(int InNumFramesToCapture = -1);
	RENDERCORE_API void EndCapture();

	RENDERCORE_API bool IsCapturing() const;

private:

	void WriteCaptureToFile();

	void BeginFrame();
	void EndFrame();

	void BeginFrameRT();
	void EndFrameRT();

	TArray<FEvent> CapturedEvents;
	uint32 MaxNumCapturedEvents = 0;

	int32 NumFramesToCapture = -1;
	int32 CaptureFrameNumber = 0;

	bool bRequestStartCapture = false;
	bool bRequestStopCapture = false;
	bool bCapturing = false;
	bool bCapturingRT = false;

	uint64 GameThreadFrameBeginCycle = 0;
	uint64 GameThreadFrameEndCycle = 0;

	uint64 RenderThreadFrameBeginCycle = 0;
	uint64 RenderThreadFrameEndCycle = 0;

	uint32 SessionId = 0;

	uint32 Pad[(PLATFORM_CACHE_LINE_SIZE / 4 - 2)];
	volatile int32 EventAtomicConter = 0;

	uint32 RenderThreadId = 0;
};

#endif //TRACING_PROFILER
