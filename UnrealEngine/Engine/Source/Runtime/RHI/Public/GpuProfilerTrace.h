// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Trace/Config.h"

#if !defined(GPUPROFILERTRACE_ENABLED)
#if UE_TRACE_ENABLED && !UE_BUILD_SHIPPING
#define GPUPROFILERTRACE_ENABLED 1
#else
#define GPUPROFILERTRACE_ENABLED 0
#endif
#endif

#if GPUPROFILERTRACE_ENABLED

class FName;

struct FGpuProfilerTrace
{
	RHI_API static void BeginFrame(struct FGPUTimingCalibrationTimestamp& Calibration);
	RHI_API static void SpecifyEventByName(const FName& Name);
	RHI_API static void BeginEventByName(const FName& Name, uint32 FrameNumber, uint64 TimestampMicroseconds);
	RHI_API static void EndEvent(uint64 TimestampMicroseconds);
	RHI_API static void EndFrame(uint32 GPUIndex);
	RHI_API static void Deinitialize();
};

#define TRACE_GPUPROFILER_DEFINE_EVENT_TYPE(Name) \
	FGpuProfilerTrace::FEventType PREPROCESSOR_JOIN(__GGpuProfilerEventType, Name)(TEXT(#Name));

#define TRACE_GPUPROFILER_DECLARE_EVENT_TYPE_EXTERN(Name) \
	extern FGpuProfilerTrace::FEventType PREPROCESSOR_JOIN(__GGpuProfilerEventType, Name);

#define TRACE_GPUPROFILER_EVENT_TYPE(Name) \
	&PREPROCESSOR_JOIN(__GGpuProfilerEventType, Name)

#define TRACE_GPUPROFILER_BEGIN_FRAME() \
	FGpuProfilerTrace::BeginFrame();

#define TRACE_GPUPROFILER_BEGIN_EVENT(EventType, FrameNumber, TimestampMicroseconds) \
	FGpuProfilerTrace::BeginEvent(EventType, FrameNumber, TimestampMicroseconds);

#define TRACE_GPUPROFILER_END_EVENT(TimestampMicroseconds) \
	FGpuProfilerTrace::EndEvent(TimestampMicroseconds);

#define TRACE_GPUPROFILER_END_FRAME() \
	FGpuProfilerTrace::EndFrame();

#define TRACE_GPUPROFILER_DEINITIALIZE() \
	FGpuProfilerTrace::Deinitialize();

#else

struct FGpuProfilerTrace
{
	struct FEventType
	{
		FEventType(const TCHAR* Name) {};
	};
};

#define TRACE_GPUPROFILER_DEFINE_EVENT_TYPE(...)
#define TRACE_GPUPROFILER_DECLARE_EVENT_TYPE_EXTERN(...)
#define TRACE_GPUPROFILER_EVENT_TYPE(...) nullptr
#define TRACE_GPUPROFILER_BEGIN_FRAME(...)
#define TRACE_GPUPROFILER_BEGIN_EVENT(...)
#define TRACE_GPUPROFILER_END_EVENT(...)
#define TRACE_GPUPROFILER_END_FRAME(...)
#define TRACE_GPUPROFILER_DEINITIALIZE(...)

#endif
