// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProfilingDebugging/HealthSnapshot.h"
#include "Performance/EnginePerformanceTargets.h"
#include "Misc/TimeGuard.h"
#include "ContentStreaming.h"
#include "ChartCreation.h"

#include "HAL/MemoryMisc.h"
#include "Engine/Engine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HealthSnapshot)

DEFINE_LOG_CATEGORY_STATIC(LogHealthSnapshot, Log, All);

//////////////////////////////////////////////////////////////////////
// FFortHealthSnapshot

namespace HealthSnapshot
{
	static int32 GLogHealthSnapshot = 1;
	static FAutoConsoleVariableRef CVarHealthSnapshot(
		TEXT("health.logHealthSnapshot"),
		GLogHealthSnapshot,
		TEXT("Log health snapshot)\n"),
		ECVF_Default);
}

TSharedPtr<FPerformanceTrackingChart> UHealthSnapshotBlueprintLibrary::PerformanceChart;

FHealthSnapshot::FHealthSnapshot(const TCHAR* InTitle)
	: LLMTotalMemoryMB(0)
	, MeasuredPerfTime(0)
	, HitchesPerMinute(0)
	, AvgHitchTime(0)
	, MVP(0)
	, AvgFPS(0)
{
	SCOPE_TIME_GUARD_MS(TEXT("Health Snapshot"), 4);

	Title = InTitle;

	CaptureMemoryStats();
}

FHealthSnapshot::FHealthSnapshot(const TCHAR* InTitle, const FPerformanceTrackingChart* GameplayFPSChart)
	: FHealthSnapshot(InTitle)
{
	CapturePerformanceStats(GameplayFPSChart);
}

void FHealthSnapshot::CaptureMemoryStats()
{
	const float InvToMb = 1.0 / (1024 * 1024);

#if ENABLE_LOW_LEVEL_MEM_TRACKER
	if (FLowLevelMemTracker::Get().IsEnabled())
	{
		LLMTotalMemoryMB = FLowLevelMemTracker::Get().GetTagAmountForTracker(ELLMTracker::Default, ELLMTag::TrackedTotal) * InvToMb;
	}
#endif

	FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();

	// How much is our process using from the OS
	PhysicalMemoryMB.Size = MemoryStats.TotalPhysical * InvToMb;				// Total physical (system) memory
	PhysicalMemoryMB.Used = MemoryStats.UsedPhysical * InvToMb;				// Total free physical(system)  memory
	PhysicalMemoryMB.Peak = MemoryStats.PeakUsedPhysical * InvToMb;

	// Get used CPU memory from the allocator
	FGenericMemoryStats MallocStats;
	if (GMalloc)
	{
		static SIZE_T MaxAllocated = 0;
		SIZE_T Allocated = 0;

		GMalloc->GetAllocatorStats(MallocStats);
		SIZE_T *MallocTotalAllocated = nullptr;
		if ((MallocTotalAllocated = MallocStats.Data.Find("TotalAllocated")) != nullptr)
		{
			Allocated = *MallocTotalAllocated;
		}

		if (Allocated > 0)
		{
			MaxAllocated = FMath::Max(MaxAllocated, Allocated);
			CPUMemoryMB.Used = Allocated * InvToMb;
			CPUMemoryMB.Peak = MaxAllocated * InvToMb;
		}
	}

	PlatformMemoryStats = MemoryStats.GetPlatformSpecificStats();

	if (FPlatformProperties::SupportsTextureStreaming() && IStreamingManager::Get().IsTextureStreamingEnabled())
	{
		StreamingMemoryMB.Used = IStreamingManager::Get().GetTextureStreamingManager().GetRequiredPoolSize() * InvToMb;
		StreamingMemoryMB.Peak = IStreamingManager::Get().GetTextureStreamingManager().GetMaxEverRequired() * InvToMb;
	}
}

void FHealthSnapshot::CapturePerformanceStats(const FPerformanceTrackingChart* GameplayFPSChart)
{
	if (GameplayFPSChart)
	{
		const double TotalTime = GameplayFPSChart->GetTotalTime();
		const int64 FramesCounted = GameplayFPSChart->GetNumFrames();
		const double TargetFPS = 1000.0 / FEnginePerformanceTargets::GetTargetFrameTimeThresholdMS();
		const int64 TotalTargetFrames = TargetFPS * TotalTime;

		const int32 MissedFrames = FMath::Max<int32>(TotalTargetFrames - FramesCounted, 0);
		const float PctMissedFrames = (float)((MissedFrames * 100.0) / (double)TotalTargetFrames);

		MeasuredPerfTime = GameplayFPSChart->AccumulatedChartTime;

		MVP = PctMissedFrames;
		AvgFPS = FramesCounted / TotalTime;
		HitchesPerMinute = GameplayFPSChart->GetAvgHitchesPerMinute();
		AvgHitchTime = GameplayFPSChart->GetAvgHitchFrameLength();

		DrawCalls.Max = GameplayFPSChart->MaxDrawCalls;
		DrawCalls.Min = GameplayFPSChart->MinDrawCalls;
		DrawCalls.Avg = FramesCounted > 0 ? (GameplayFPSChart->TotalDrawCalls / FramesCounted) : 0;
		PrimitivesDrawn.Max = GameplayFPSChart->MaxDrawnPrimitives;
		PrimitivesDrawn.Min = GameplayFPSChart->MinDrawnPrimitives;
		PrimitivesDrawn.Avg = FramesCounted > 0 ? GameplayFPSChart->TotalDrawnPrimitives / FramesCounted : 0;

		FrameTime.Min = GameplayFPSChart->FrametimeHistogram.GetMinOfAllMeasures();
		FrameTime.Max = GameplayFPSChart->FrametimeHistogram.GetMaxOfAllMeasures();
		FrameTime.Avg = GameplayFPSChart->FrametimeHistogram.GetAverageOfAllMeasures();

		DynamicResolution.Min = GameplayFPSChart->DynamicResHistogram.GetMinOfAllMeasures();
		DynamicResolution.Max = GameplayFPSChart->DynamicResHistogram.GetMaxOfAllMeasures();
		DynamicResolution.Avg = GameplayFPSChart->DynamicResHistogram.GetAverageOfAllMeasures();

		PhysicalMemory.Max = GameplayFPSChart->MaxPhysicalMemory;
		PhysicalMemory.Min = GameplayFPSChart->MinPhysicalMemory;
		VirtualMemory.Max = GameplayFPSChart->MaxVirtualMemory;
		VirtualMemory.Min = GameplayFPSChart->MinVirtualMemory;
		if (FramesCounted > 0)
		{
			PhysicalMemory.Avg = GameplayFPSChart->TotalPhysicalMemoryUsed / static_cast<uint64>(FramesCounted);
			VirtualMemory.Avg = GameplayFPSChart->TotalVirtualMemoryUsed / static_cast<uint64>(FramesCounted);
		}

		// What % of frames were bound
		GameThread.PercentFramesBound = FramesCounted > 0 ? (GameplayFPSChart->NumFramesBound_GameThread * 100.0) / FramesCounted : 0;

		// Turn total hitches into a hpm value
		GameThread.HitchesPerMinute = GameplayFPSChart->TotalGameThreadBoundHitchCount / (MeasuredPerfTime / 60);

		// Avg time spent
		GameThread.AvgTime = FramesCounted > 0 ? GameplayFPSChart->TotalFrameTime_GameThread / FramesCounted : 0;

		// Ditto render thread
		RenderThread.PercentFramesBound = FramesCounted > 0 ? (GameplayFPSChart->NumFramesBound_RenderThread * 100.0) / FramesCounted : 0;
		RenderThread.HitchesPerMinute = GameplayFPSChart->TotalRenderThreadBoundHitchCount / (MeasuredPerfTime / 60);
		RenderThread.AvgTime = FramesCounted > 0 ? GameplayFPSChart->TotalFrameTime_RenderThread / FramesCounted : 0;

		// Ditto RHIT
		RHIThread.PercentFramesBound = FramesCounted > 0 ? (GameplayFPSChart->NumFramesBound_RHIThread * 100.0) / FramesCounted : 0;
		RHIThread.HitchesPerMinute = GameplayFPSChart->TotalRHIThreadBoundHitchCount / (MeasuredPerfTime / 60);
		RHIThread.AvgTime = FramesCounted > 0 ? GameplayFPSChart->TotalFrameTime_RHIThread / FramesCounted : 0;

		// ditto GPU
		GPU.PercentFramesBound = FramesCounted > 0 ? (GameplayFPSChart->NumFramesBound_GPU * 100.0) / FramesCounted : 0;
		GPU.HitchesPerMinute = GameplayFPSChart->TotalGPUBoundHitchCount / (MeasuredPerfTime / 60);
		GPU.AvgTime = FramesCounted > 0 ? GameplayFPSChart->TotalFrameTime_GPU / FramesCounted : 0;
	}
}

void FHealthSnapshot::Dump(FOutputDevice& Ar)
{
	if (HealthSnapshot::GLogHealthSnapshot)
	{
#if !NO_LOGGING
		const FName CategoryName(LogHealthSnapshot.GetCategoryName());
#else
		const FName CategoryName(TEXT("LogHealthSnapshot"));
#endif

		Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("======= Snapshot: %s ======="), *Title);

		DumpStats(Ar, CategoryName);

		Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("========================================================="));
	}
}

void FHealthSnapshot::DumpStats(FOutputDevice& Ar, FName CategoryName)
{
	if (MeasuredPerfTime > 0)
	{
		Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("MeasuredPerfTime: %.02f secs"), MeasuredPerfTime);
		Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("MVP: %.02f%%, AvgFPS:%.2f, HitchesPerMinute: %.2f, Avg Hitch %.02fms"), MVP, AvgFPS, HitchesPerMinute, AvgHitchTime * 1000);
		Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("DynRes: Avg: %.02f%%, Max: %.02f%%, Min: %.02f%%"), DynamicResolution.Avg, DynamicResolution.Max, DynamicResolution.Min);
		Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("FT: Avg: %.02fms, Max: %.02fms, Min: %.02fms"), FrameTime.Avg * 1000, FrameTime.Max * 1000, FrameTime.Min * 1000);
		Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("GT:  Avg %.02fms, Hitches/Min: %.02f, Bound Frames: %.02f%%"), GameThread.AvgTime * 1000, GameThread.HitchesPerMinute, GameThread.PercentFramesBound);
		Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("RT:  Avg %.02fms, Hitches/Min: %.02f, Bound Frames: %.02f%%"), RenderThread.AvgTime * 1000, RenderThread.HitchesPerMinute, RenderThread.PercentFramesBound);
		Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("RHIT: Avg %.02fms, Hitches/Min: %.02f, Bound Frames: %.02f%%"), RHIThread.AvgTime * 1000, RHIThread.HitchesPerMinute, RHIThread.PercentFramesBound);
		Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("GPU: Avg %.02fms, Hitches/Min: %.02f, Bound Frames: %.02f%%"), GPU.AvgTime * 1000, GPU.HitchesPerMinute, GPU.PercentFramesBound);
		Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("DrawCalls: Avg: %d, Max: %d, Min: %d"), DrawCalls.Avg, DrawCalls.Max, DrawCalls.Min);
		Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("DrawnPrims: Avg: %d, Max: %d, Min: %d"), PrimitivesDrawn.Avg, PrimitivesDrawn.Max, PrimitivesDrawn.Min);
		Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("PhysicalMemoryMB: Avg: %d, Max: %d, Min: %d"), PhysicalMemory.Avg, PhysicalMemory.Max, PhysicalMemory.Min);
		Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("VirtualMemoryMB: Avg: %d, Max: %d, Min: %d"), VirtualMemory.Avg, VirtualMemory.Max, VirtualMemory.Min);
	}

	Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("CPU Memory: Used %.2fMB, Peak %.2fMB"), CPUMemoryMB.Used, CPUMemoryMB.Peak);
	Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Physical Memory: Used %.2fMB, Peak %.2fMB"), PhysicalMemoryMB.Used, PhysicalMemoryMB.Peak);
	Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Streaming Memory: Used %.2fMB, Peak %.2fMB"), StreamingMemoryMB.Used, StreamingMemoryMB.Peak);

#if ENABLE_LOW_LEVEL_MEM_TRACKER
	if (FLowLevelMemTracker::Get().IsEnabled())
	{
		Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Low Level Memory : TrackedTotal %.2fMB"), LLMTotalMemoryMB);
}
#endif

	for (int32 StatIdx = 0; StatIdx < PlatformMemoryStats.Num(); ++StatIdx)
	{
		Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("%s: Used %.2f MB"), PlatformMemoryStats[StatIdx].Name, PlatformMemoryStats[StatIdx].Value / (1024.f * 1024.f));
	}
}

UHealthSnapshotBlueprintLibrary::UHealthSnapshotBlueprintLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UHealthSnapshotBlueprintLibrary::StartPerformanceSnapshots()
{
	if (PerformanceChart.IsValid())
	{
		StopPerformanceSnapshots();
	}

	PerformanceChart = MakeShareable(new FPerformanceTrackingChart(FDateTime::Now(), TEXT("HealthSnapshots")));
	GEngine->AddPerformanceDataConsumer(PerformanceChart);
	PerformanceChart->StartCharting();
}

void UHealthSnapshotBlueprintLibrary::StopPerformanceSnapshots()
{
	if (PerformanceChart.IsValid())
	{
		GEngine->RemovePerformanceDataConsumer(PerformanceChart);
		PerformanceChart.Reset();
	}
}

void UHealthSnapshotBlueprintLibrary::LogPerformanceSnapshot(const FString SnapshotTitle, bool bResetStats)
{
	FHealthSnapshot Snapshot(TEXT(""));

	if (PerformanceChart.IsValid())
	{
		Snapshot = FHealthSnapshot(*SnapshotTitle, PerformanceChart.Get());

		if (bResetStats)
		{
			PerformanceChart->Reset(FDateTime::Now());
		}
	}
	else
	{
		Snapshot = FHealthSnapshot(*SnapshotTitle);
	}

	Snapshot.Dump(*GLog);
}

