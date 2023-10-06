// Copyright Epic Games, Inc. All Rights Reserved.

#include "GpuProfilerTrace.h"
#include "GPUProfiler.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "Trace/Trace.inl"
#include "RHI.h"
#include "Trace/Detail/Field.h"
#include "Trace/Detail/Important/ImportantLogScope.h"
#include "Trace/Detail/LogScope.h"

#if GPUPROFILERTRACE_ENABLED

namespace GpuProfilerTrace
{

static TAutoConsoleVariable<int32> CVarGpuProfilerMaxEventBufferSizeKB(
	TEXT("r.GpuProfilerMaxEventBufferSizeKB"),
	32,
	TEXT("Size of the scratch buffer in kB."),
	ECVF_Default);


struct
{
	int64							CalibrationBias;
	FGPUTimingCalibrationTimestamp	Calibration;
	uint64							TimestampBase;
	uint64							LastTimestamp;
	uint32							RenderingFrameNumber;
	uint32							EventBufferSize;
	bool							bActive;
	uint8*							EventBuffer = nullptr;
	uint32							MaxEventBufferSize = 0;
} GCurrentFrame;

static TSet<uint32> GEventNames;

UE_TRACE_CHANNEL_EXTERN(GpuChannel, RHI_API)
UE_TRACE_CHANNEL_DEFINE(GpuChannel)

UE_TRACE_EVENT_BEGIN(GpuProfiler, EventSpec, NoSync|Important)
	UE_TRACE_EVENT_FIELD(uint32, EventType)
	UE_TRACE_EVENT_FIELD(uint16[], Name)
UE_TRACE_EVENT_END()

// GPU Index 0
UE_TRACE_EVENT_BEGIN(GpuProfiler, Frame)
	UE_TRACE_EVENT_FIELD(uint64, CalibrationBias)
	UE_TRACE_EVENT_FIELD(uint64, TimestampBase)
	UE_TRACE_EVENT_FIELD(uint32, RenderingFrameNumber)
	UE_TRACE_EVENT_FIELD(uint8[], Data)
UE_TRACE_EVENT_END()

// GPU Index 1
UE_TRACE_EVENT_BEGIN(GpuProfiler, Frame2)
	UE_TRACE_EVENT_FIELD(uint64, CalibrationBias)
	UE_TRACE_EVENT_FIELD(uint64, TimestampBase)
	UE_TRACE_EVENT_FIELD(uint32, RenderingFrameNumber)
	UE_TRACE_EVENT_FIELD(uint8[], Data)
UE_TRACE_EVENT_END()

} // namespace GpuProfilerTrace

void FGpuProfilerTrace::BeginFrame(FGPUTimingCalibrationTimestamp& Calibration)
{
	using namespace GpuProfilerTrace;

	if (!bool(GpuChannel))
	{
		return;
	}

	GCurrentFrame.Calibration = Calibration;
	ensure(GCurrentFrame.Calibration.CPUMicroseconds > 0 && GCurrentFrame.Calibration.GPUMicroseconds > 0);
	GCurrentFrame.TimestampBase = 0;
	GCurrentFrame.EventBufferSize = 0;
	GCurrentFrame.bActive = true;

	int32 NeededSize = CVarGpuProfilerMaxEventBufferSizeKB.GetValueOnRenderThread() * 1024;
	if ((GCurrentFrame.MaxEventBufferSize != NeededSize) && (NeededSize > 0))
	{
		FMemory::Free(GCurrentFrame.EventBuffer);
		GCurrentFrame.EventBuffer = (uint8*)FMemory::Malloc(NeededSize);
		GCurrentFrame.MaxEventBufferSize = NeededSize;
	}
}

void FGpuProfilerTrace::SpecifyEventByName(const FName& Name)
{
	using namespace GpuProfilerTrace;

	if (!GCurrentFrame.bActive)
	{
		return;
	}

	// This function is only called from FRealtimeGPUProfilerFrame::UpdateStats
	// at the end of the frame, so the access to this container is thread safe

	uint32 Index = Name.GetComparisonIndex().ToUnstableInt();
	if (!GEventNames.Contains(Index))
	{
		GEventNames.Add(Index);

		FString String = Name.ToString();
		uint32 NameLength = String.Len() + 1;
		static_assert(sizeof(TCHAR) == sizeof(uint16), "");

		UE_TRACE_LOG(GpuProfiler, EventSpec, GpuChannel, NameLength * sizeof(uint16))
			<< EventSpec.EventType(Index)
			<< EventSpec.Name((const uint16*)(*String), NameLength);
	}
}

void FGpuProfilerTrace::BeginEventByName(const FName& Name, uint32 FrameNumber, uint64 TimestampMicroseconds)
{
	using namespace GpuProfilerTrace;

	if (!GCurrentFrame.bActive)
	{
		return;
	}

	// Prevent buffer overrun
	if (GCurrentFrame.EventBufferSize + 10 + sizeof(uint32) > GCurrentFrame.MaxEventBufferSize) // 10 is the max size that FTraceUtils::Encode7bit might use + some space for the FName index (uint32)
	{
		UE_LOG(LogRHI, Error, TEXT("GpuProfiler's scratch buffer is out of space for this frame (current size : %d kB). Dropping this frame. The size can be increased dynamically with the console variable r.GpuProfilerMaxEventBufferSizeKB"), GCurrentFrame.MaxEventBufferSize / 1024);

		// Deactivate for the current frame to avoid errors while decoding an incomplete trace
		GCurrentFrame.bActive = false;
		return;
	}

	if (GCurrentFrame.TimestampBase == 0)
	{
		GCurrentFrame.TimestampBase = TimestampMicroseconds;
		GCurrentFrame.LastTimestamp = GCurrentFrame.TimestampBase;
		GCurrentFrame.RenderingFrameNumber = FrameNumber;
		if (!GCurrentFrame.Calibration.GPUMicroseconds)
		{
			GCurrentFrame.Calibration.GPUMicroseconds = TimestampMicroseconds;
		}
	}
	uint8* BufferPtr = GCurrentFrame.EventBuffer + GCurrentFrame.EventBufferSize;
	uint64 TimestampDelta = TimestampMicroseconds - GCurrentFrame.LastTimestamp;
	GCurrentFrame.LastTimestamp = TimestampMicroseconds;
	FTraceUtils::Encode7bit((TimestampDelta << 1ull) | 0x1, BufferPtr);
	*reinterpret_cast<uint32*>(BufferPtr) = uint32(Name.GetComparisonIndex().ToUnstableInt());
	GCurrentFrame.EventBufferSize = BufferPtr - GCurrentFrame.EventBuffer + sizeof(uint32);
}

void FGpuProfilerTrace::EndEvent(uint64 TimestampMicroseconds)
{
	using namespace GpuProfilerTrace;

	if (!GCurrentFrame.bActive)
	{
		return;
	}

	// Prevent buffer overrun
	if (GCurrentFrame.EventBufferSize + 10 > GCurrentFrame.MaxEventBufferSize) // 10 is the max size that FTraceUtils::Encode7bit might use
	{
		UE_LOG(LogRHI, Error, TEXT("GpuProfiler's scratch buffer is out of space for this frame (current size : %d kB). Dropping this frame. The size can be increased dynamically with the console variable r.GpuProfilerMaxEventBufferSizeKB"), GCurrentFrame.MaxEventBufferSize / 1024);

		// Deactivate for the current frame to avoid errors while decoding an incomplete trace
		GCurrentFrame.bActive = false;
		return;
	}

	uint64 TimestampDelta = TimestampMicroseconds - GCurrentFrame.LastTimestamp;
	GCurrentFrame.LastTimestamp = TimestampMicroseconds;
	uint8* BufferPtr = GCurrentFrame.EventBuffer + GCurrentFrame.EventBufferSize;
	FTraceUtils::Encode7bit(TimestampDelta << 1ull, BufferPtr);
	GCurrentFrame.EventBufferSize = BufferPtr - GCurrentFrame.EventBuffer;
}

void FGpuProfilerTrace::EndFrame(uint32 GPUIndex)
{
	using namespace GpuProfilerTrace;

	if (GCurrentFrame.bActive && GCurrentFrame.EventBufferSize)
	{
		// This subtraction is intended to be performed on uint64 to leverage the wrap around behavior defined by the standard
		uint64 Bias = GCurrentFrame.Calibration.CPUMicroseconds - GCurrentFrame.Calibration.GPUMicroseconds;

		if (GPUIndex == 0)
		{
			UE_TRACE_LOG(GpuProfiler, Frame, GpuChannel)
				<< Frame.CalibrationBias(Bias)
				<< Frame.TimestampBase(GCurrentFrame.TimestampBase)
				<< Frame.RenderingFrameNumber(GCurrentFrame.RenderingFrameNumber)
				<< Frame.Data(GCurrentFrame.EventBuffer, GCurrentFrame.EventBufferSize);
		}
		else if (GPUIndex == 1)
		{
			UE_TRACE_LOG(GpuProfiler, Frame2, GpuChannel)
				<< Frame2.CalibrationBias(Bias)
				<< Frame2.TimestampBase(GCurrentFrame.TimestampBase)
				<< Frame2.RenderingFrameNumber(GCurrentFrame.RenderingFrameNumber)
				<< Frame2.Data(GCurrentFrame.EventBuffer, GCurrentFrame.EventBufferSize);
		}
	}

	GCurrentFrame.EventBufferSize = 0;
	GCurrentFrame.bActive = false;
}

void FGpuProfilerTrace::Deinitialize()
{
	using namespace GpuProfilerTrace;

	FMemory::Free(GCurrentFrame.EventBuffer);
	GCurrentFrame.EventBuffer = nullptr;
	GCurrentFrame.MaxEventBufferSize = 0;
}

#endif
