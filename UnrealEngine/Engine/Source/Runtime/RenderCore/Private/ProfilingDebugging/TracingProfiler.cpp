// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProfilingDebugging/TracingProfiler.h"

#if TRACING_PROFILER

#include "GPUProfiler.h"
#include "HAL/ConsoleManager.h"
#include "HAL/FileManager.h"
#include "HAL/LowLevelMemTracker.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "UObject/NameTypes.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

static TAutoConsoleVariable<int32> GTracingProfileBufferSize(
	TEXT("TracingProfiler.BufferSize"),
	65536,
	TEXT("Defines the maximum umber of events stored in the internal ring buffer of the tracing profiler. ")
	TEXT("Only read at process startup and can't be changed at runtime."),
	ECVF_Default);

FTracingProfiler* FTracingProfiler::Instance = nullptr;

FTracingProfiler* FTracingProfiler::Get()
{
	if (Instance == nullptr)
	{
		Instance = new FTracingProfiler;
	}
	return Instance;
}

static void HandleTracingProfileCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return;
	}
	FString Param = Args[0];
	if (Param == TEXT("START"))
	{
		FTracingProfiler::Get()->BeginCapture();
	}
	else if (Param == TEXT("STOP"))
	{
		FTracingProfiler::Get()->EndCapture();
	}
	else
	{
		int32 CaptureFrames = 0;
		if (FParse::Value(*Param, TEXT("FRAMES="), CaptureFrames))
		{
			FTracingProfiler::Get()->BeginCapture(CaptureFrames);
		}
	}
}

static FAutoConsoleCommand HandleTracingProfileCmd(
	TEXT("TracingProfile"),
	TEXT("Starts or stops tracing profiler"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&HandleTracingProfileCommand)
);

FTracingProfiler::FTracingProfiler()
{
	LLM_SCOPE(ELLMTag::Stats);

	MaxNumCapturedEvents = GTracingProfileBufferSize.GetValueOnAnyThread();

	FEvent DefaultEvent = {};
	CapturedEvents.Init(DefaultEvent, MaxNumCapturedEvents);

	FCoreDelegates::OnBeginFrame.AddLambda([]() { FTracingProfiler::Get()->BeginFrame(); });
	FCoreDelegates::OnEndFrame.AddLambda([]() { FTracingProfiler::Get()->EndFrame(); });

	FCoreDelegates::OnBeginFrameRT.AddLambda([]() { FTracingProfiler::Get()->BeginFrameRT(); });
	FCoreDelegates::OnEndFrameRT.AddLambda([]() { FTracingProfiler::Get()->EndFrameRT(); });
}

void FTracingProfiler::Init()
{
	
}

void FTracingProfiler::BeginFrame()
{
	check(IsInGameThread());

	if (bRequestStartCapture)
	{
		bCapturing = true;
		bRequestStartCapture = false;
	}

	GameThreadFrameBeginCycle = FPlatformTime::Cycles64();
}

static inline uint64 CyclesToMicroseconds64(uint64 Cycles)
{
	return uint64(FPlatformTime::ToSeconds64(Cycles) * 1e6);
}

void FTracingProfiler::EndFrame()
{
	check(IsInGameThread());

	if (!bCapturing)
	{
		return;
	}

	GameThreadFrameEndCycle = FPlatformTime::Cycles64();

	AddCPUEvent(NAME_GameThread,
		GameThreadFrameBeginCycle,
		GameThreadFrameEndCycle,
		GGameThreadId,
		GFrameNumber);

	if (NumFramesToCapture >= 0)
	{
		NumFramesToCapture--;
		if (NumFramesToCapture == 0)
		{
			bRequestStopCapture = true;
		}
	}

	if (bRequestStopCapture)
	{
		FlushRenderingCommands();

		bCapturing = false;
		bRequestStopCapture = false;

		WriteCaptureToFile();

	}

	CaptureFrameNumber++;
}

void FTracingProfiler::BeginFrameRT()
{
	RenderThreadFrameBeginCycle = FPlatformTime::Cycles64();

	if (!bCapturingRT && bCapturing)
	{
		GDynamicRHI->RHICalibrateTimers();
	}

	bCapturingRT = bCapturing;

	RenderThreadId = FPlatformTLS::GetCurrentThreadId();
}

void FTracingProfiler::EndFrameRT()
{
	RenderThreadFrameEndCycle = FPlatformTime::Cycles64();

	if (!bCapturingRT)
	{
		return;
	}

	AddCPUEvent(NAME_RenderThread,
		RenderThreadFrameBeginCycle,
		RenderThreadFrameEndCycle,
		RenderThreadId,
		GFrameNumberRenderThread);
}


int32 FTracingProfiler::GetCaptureFrameNumber()
{
	return CaptureFrameNumber;
}

void FTracingProfiler::BeginCapture(int InNumFramesToCapture)
{
	check(IsInGameThread());

	NumFramesToCapture = InNumFramesToCapture;
	CaptureFrameNumber = 0;

	bRequestStartCapture = true;

	SessionId++;
}

void FTracingProfiler::EndCapture()
{
	bRequestStopCapture = true;
}

bool FTracingProfiler::IsCapturing() const
{
	check(IsInGameThread());
	return bRequestStartCapture || bCapturing;
}

void FTracingProfiler::WriteCaptureToFile()
{
	check(!bCapturing);

	FString Filename = FString::Printf(TEXT("Profile(%s)"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	FString TracingRootPath = FPaths::ProfilingDir() + TEXT("Traces/");
	FString OutputFilename = TracingRootPath + Filename + TEXT(".json");

	FArchive* OutputFile = IFileManager::Get().CreateDebugFileWriter(*OutputFilename);

	auto WriteString = [OutputFile](const char* String)
	{
		OutputFile->Serialize((void*)String, sizeof(ANSICHAR)*FCStringAnsi::Strlen(String));
	};

	WriteString(R"({"traceEvents":[)" "\n");

	enum { StringBufferSize = 1024 };
	char StringBuffer[StringBufferSize];

	const uint32 Pid = FPlatformProcess::GetCurrentProcessId();

	const uint32 NumEvents = CapturedEvents.Num();

	const uint32 LocalSessionId = SessionId;
	auto IsEventValid = [LocalSessionId](const FEvent& Event)
	{
		if (Event.SessionId != LocalSessionId)
		{
			return false;
		}

		if (Event.Type == EEventType::GPU)
		{
			return Event.GPU.EndMicroseconds > Event.GPU.BeginMicroseconds
				&& (Event.GPU.EndMicroseconds - Event.GPU.BeginMicroseconds) < 10000000; // Arbitrary threshold of 10 seconds for any one GPU event
		}
		else
		{
			return Event.CPU.EndCycles > Event.CPU.BeginCycles;
		}
	};

	int32 MaxGPUIndex = -1;
	uint64 FirstCPUTimestampCycles = UINT64_MAX;
	TStaticArray<uint64, MAX_NUM_GPUS> FirstGPUTimestampMicroseconds(UINT64_MAX);
	for (const FEvent& Event : CapturedEvents)
	{
		if (IsEventValid(Event))
		{
			if (Event.Type == EEventType::GPU)
			{
				MaxGPUIndex = FMath::Max<int32>(MaxGPUIndex, Event.GPU.GPUIndex);
				FirstGPUTimestampMicroseconds[Event.GPU.GPUIndex] = FMath::Min(FirstGPUTimestampMicroseconds[Event.GPU.GPUIndex], Event.GPU.BeginMicroseconds);
			}
			else
			{
				FirstCPUTimestampCycles = FMath::Min(FirstCPUTimestampCycles, Event.CPU.BeginCycles);
			}
		}
	}

	// Write metadata (thread names, sorting order, etc.)

	int32 SortIndex = 0; // Lower numbers result in higher position in the visualizer.

	for (int32 GPUIndex = 0; GPUIndex <= MaxGPUIndex; ++GPUIndex)
	{
		FCStringAnsi::Snprintf(StringBuffer, StringBufferSize,
			R"({"pid":%d, "tid":%d, "ph": "M", "name": "thread_name", "args":{"name":"GPU %d"}},)"
			R"({"pid":%d, "tid":%d, "ph": "M", "name": "thread_sort_index", "args":{"sort_index": %d}},)"
			"\n",
			Pid, GPUIndex, GPUIndex, Pid, GPUIndex, SortIndex);
		WriteString(StringBuffer);
		SortIndex++;
	}

	FCStringAnsi::Snprintf(StringBuffer, StringBufferSize,
		R"({"pid":%d, "tid":%d, "ph": "M", "name": "thread_name", "args":{"name":"Render thread"}},)"
		R"({"pid":%d, "tid":%d, "ph": "M", "name": "thread_sort_index", "args":{"sort_index": %d}},)"
		"\n",
		Pid, RenderThreadId, Pid, RenderThreadId, SortIndex);
	WriteString(StringBuffer);
	SortIndex++;

	FCStringAnsi::Snprintf(StringBuffer, StringBufferSize,
		R"({"pid":%d, "tid":%d, "ph": "M", "name": "thread_name", "args":{"name":"Game thread"}},)"
		R"({"pid":%d, "tid":%d, "ph": "M", "name": "thread_sort_index", "args":{"sort_index": %d}},)"
		"\n",
		Pid, GGameThreadId, Pid, GGameThreadId, SortIndex);
	WriteString(StringBuffer);
	SortIndex++;

	// Align GPU and CPU timestamps

	TStaticArray<FGPUTimingCalibrationTimestamp, MAX_NUM_GPUS> CalibrationTimestamps(FGPUTimingCalibrationTimestamp{});
	for (uint32 GPUIndex : FRHIGPUMask::All())
	{
		FGPUTimingCalibrationTimestamp& CalibrationTimestamp = CalibrationTimestamps[GPUIndex];
		CalibrationTimestamp = FGPUTiming::GetCalibrationTimestamp(GPUIndex);

		// If platform does not support GPU/CPU timer alignment, then simply align GPU and CPU on first event
		if (CalibrationTimestamp.CPUMicroseconds == 0 || CalibrationTimestamp.GPUMicroseconds == 0)
		{
			CalibrationTimestamp.CPUMicroseconds = CyclesToMicroseconds64(FirstCPUTimestampCycles);
			CalibrationTimestamp.GPUMicroseconds = FirstGPUTimestampMicroseconds[GPUIndex];
		}
	}

	TStaticArray<uint64, MAX_NUM_GPUS> GPUTimeOffsets;
	for (uint32 GPUIndex : FRHIGPUMask::All())
	{
		GPUTimeOffsets[GPUIndex] = CalibrationTimestamps[GPUIndex].CPUMicroseconds - CalibrationTimestamps[GPUIndex].GPUMicroseconds;
	}

	// Write out all events

	uint32 NumProcessedEvents = 0;
	for (const FEvent& Event : CapturedEvents)
	{
		if (!IsEventValid(Event))
		{
			continue;
		}

		uint64 TimeBeginMicroseconds = 0;
		uint64 TimeEndMicroseconds = 0;

		switch (Event.Type)
		{
		case EEventType::CPU:
			TimeBeginMicroseconds = CyclesToMicroseconds64(Event.CPU.BeginCycles);
			TimeEndMicroseconds = CyclesToMicroseconds64(Event.CPU.EndCycles);
			break;
		case EEventType::GPU:
			// Note: We could also use `clock_sync` metadata to synchronize events in Chrome viewer.
			// Advantage of doing it manually is that trace log can be consumed by a less sophisticated
			// parser that does not implement all the features of Chrome viewer.
			TimeBeginMicroseconds = Event.GPU.BeginMicroseconds + GPUTimeOffsets[Event.GPU.GPUIndex];
			TimeEndMicroseconds = Event.GPU.EndMicroseconds + GPUTimeOffsets[Event.GPU.GPUIndex];
			break;
		default:
			verifyf(0, TEXT("Unexpected profiling event type"));
		}

		uint32 ThreadOrGpuId;
		if (Event.Type == EEventType::CPU)
		{
			ThreadOrGpuId = Event.CPU.ThreadId;
		}
		else
		{
			ThreadOrGpuId = Event.GPU.GPUIndex;
		}

		ANSICHAR EventName[NAME_SIZE];
		Event.Name.GetPlainANSIString(EventName);

		FCStringAnsi::Snprintf(StringBuffer, StringBufferSize,
			R"({"pid":%d, "tid":%d, "ph": "X", "name": "%s", "ts": %llu, "dur": %llu, "args":{"frame":%d}},)"
			"\n",
			Pid, ThreadOrGpuId, EventName, TimeBeginMicroseconds,
			TimeEndMicroseconds - TimeBeginMicroseconds,
			Event.FrameNumber);

		WriteString(StringBuffer);

		++NumProcessedEvents;
	}

	// All done

	WriteString("{}]}");

	OutputFile->Close();
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

#endif //TRACING_PROFILER
