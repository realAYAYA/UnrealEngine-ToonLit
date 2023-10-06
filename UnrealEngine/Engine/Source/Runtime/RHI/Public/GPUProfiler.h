// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GPUProfiler.h: Hierarchical GPU Profiler.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Templates/RefCounting.h"
#include "RHI.h"

/** Stats for a single perf event node. */
class FGPUProfilerEventNodeStats : public FRefCountedObject
{
public:
	FGPUProfilerEventNodeStats() :
		NumDraws(0),
		NumPrimitives(0),
		NumVertices(0),
		NumDispatches(0),
		GroupCount(FIntVector(0, 0, 0)),
		NumTotalDispatches(0),
		NumTotalDraws(0),
		NumTotalPrimitives(0),
		NumTotalVertices(0),
		TimingResult(0),
		NumEvents(0)
	{
	}

	FGPUProfilerEventNodeStats(const FGPUProfilerEventNodeStats& rhs)
	{
		NumDraws = rhs.NumDraws;
		NumPrimitives = rhs.NumPrimitives;
		NumVertices = rhs.NumVertices;
		NumDispatches = rhs.NumDispatches;
		NumTotalDispatches = rhs.NumTotalDispatches;
		NumTotalDraws = rhs.NumDraws;
		NumTotalPrimitives = rhs.NumPrimitives;
		NumTotalVertices = rhs.NumVertices;
		TimingResult = rhs.TimingResult;
		NumEvents = rhs.NumEvents;
	}

	/** Exclusive number of draw calls rendered in this event. */
	uint32 NumDraws;

	/** Exclusive number of primitives rendered in this event. */
	uint32 NumPrimitives;

	/** Exclusive number of vertices rendered in this event. */
	uint32 NumVertices;

	/** Compute stats */
	uint32 NumDispatches;
	FIntVector GroupCount;
	uint32 NumTotalDispatches;

	/** Inclusive number of draw calls rendered in this event and children. */
	uint32 NumTotalDraws;

	/** Inclusive number of primitives rendered in this event and children. */
	uint32 NumTotalPrimitives;

	/** Inclusive number of vertices rendered in this event and children. */
	uint32 NumTotalVertices;

	/** GPU time spent inside the perf event's begin and end, in ms. */
	float TimingResult;

	/** Inclusive number of other perf events that this is the parent of. */
	uint32 NumEvents;

	const FGPUProfilerEventNodeStats operator+=(const FGPUProfilerEventNodeStats& rhs)
	{
		NumDraws += rhs.NumDraws;
		NumPrimitives += rhs.NumPrimitives;
		NumVertices += rhs.NumVertices;
		NumDispatches += rhs.NumDispatches;
		NumTotalDispatches += rhs.NumTotalDispatches;
		NumTotalDraws += rhs.NumDraws;
		NumTotalPrimitives += rhs.NumPrimitives;
		NumTotalVertices += rhs.NumVertices;
		TimingResult += rhs.TimingResult;
		NumEvents += rhs.NumEvents;

		return *this;
	}
};

/** Stats for a single perf event node. */
class FGPUProfilerEventNode : public FGPUProfilerEventNodeStats
{
public:
	FGPUProfilerEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent) :
		FGPUProfilerEventNodeStats(),
		Name(InName),
		Parent(InParent)
	{
	}

	~FGPUProfilerEventNode() {}

	FString Name;

	/** Pointer to parent node so we can walk up the tree on appEndDrawEvent. */
	FGPUProfilerEventNode* Parent;

	/** Children perf event nodes. */
	TArray<TRefCountPtr<FGPUProfilerEventNode> > Children;

	virtual float GetTiming() { return 0.0f; }
	virtual void StartTiming() {}
	virtual void StopTiming() {}
};

/** An entire frame of perf event nodes, including ancillary timers. */
struct FGPUProfilerEventNodeFrame
{
	virtual ~FGPUProfilerEventNodeFrame() {}

	/** Root nodes of the perf event tree. */
	TArray<TRefCountPtr<FGPUProfilerEventNode> > EventTree;

	/** Start this frame of per tracking */
	virtual void StartFrame() {}

	/** End this frame of per tracking, but do not block yet */
	virtual void EndFrame() {}

	/** Dumps perf event information, blocking on GPU. */
	RHI_API void DumpEventTree();

	/** Calculates root timing base frequency (if needed by this RHI) */
	virtual float GetRootTimingResults() { return 0.0f; }

	/** D3D11 Hack */
	virtual void LogDisjointQuery() {}

	virtual bool PlatformDisablesVSync() const { return false; }
};

/**
* Two timestamps performed on GPU and CPU at nearly the same time.
* This can be used to visualize GPU and CPU timing events on the same timeline.
*/
struct FGPUTimingCalibrationTimestamp
{
	uint64 GPUMicroseconds = 0;
	uint64 CPUMicroseconds = 0;
};

/**
 * Holds information if this platform's GPU allows timing
 */
struct FGPUTiming
{
public:
	/**
	 * Whether GPU timing measurements are supported by the driver.
	 *
	 * @return true if GPU timing measurements are supported by the driver.
	 */
	static bool IsSupported()
	{
		return GIsSupported;
	}

	/**
	 * Returns the frequency for the timing values, in number of ticks per seconds.
	 *
	 * @return Frequency for the timing values, in number of ticks per seconds, or 0 if the feature isn't supported.
	 */
	static uint64 GetTimingFrequency(uint32 GPUIndex = 0)
	{
		return GTimingFrequency[GPUIndex];
	}

	/**
	* Returns a pair of timestamps performed on GPU and CPU at nearly the same time, in microseconds.
	*
	* @return CPU and GPU timestamps, in microseconds. Both are 0 if feature isn't supported.
	*/
	static FGPUTimingCalibrationTimestamp GetCalibrationTimestamp(uint32 GPUIndex = 0)
	{
		return GCalibrationTimestamp[GPUIndex];
	}

	typedef void (PlatformStaticInitialize)(void*);
	static void StaticInitialize(void* UserData, PlatformStaticInitialize* PlatformFunction)
	{
		if (!GAreGlobalsInitialized && PlatformFunction)
		{
			(*PlatformFunction)(UserData);

			if (GetTimingFrequency() != 0)
			{
				GIsSupported = true;
			}
			else
			{
				GIsSupported = false;
			}

			GAreGlobalsInitialized = true;
		}
	}

protected:
	/** Whether the static variables have been initialized. */
	RHI_API static bool		GAreGlobalsInitialized;

	/** Whether GPU timing measurements are supported by the driver. */
	RHI_API static bool		GIsSupported;

	static void SetTimingFrequency(uint64 TimingFrequency, uint32 GPUIndex = 0)
	{
		GTimingFrequency[GPUIndex] = TimingFrequency;
	}

	static void SetCalibrationTimestamp(FGPUTimingCalibrationTimestamp CalibrationTimestamp, uint32 GPUIndex = 0)
	{
		GCalibrationTimestamp[GPUIndex] = CalibrationTimestamp;
	}

private:
	/** Frequency for the timing values, in number of ticks per seconds, or 0 if the feature isn't supported. */
	RHI_API static TStaticArray<uint64, MAX_NUM_GPUS>	GTimingFrequency;

	/**
	* Two timestamps performed on GPU and CPU at nearly the same time.
	* This can be used to visualize GPU and CPU timing events on the same timeline.
	* Both values may be 0 if timer calibration is not available on current platform.
	*/
	RHI_API static TStaticArray<FGPUTimingCalibrationTimestamp, MAX_NUM_GPUS> GCalibrationTimestamp;
};

/** 
 * Encapsulates GPU profiling logic and data. 
 * There's only one global instance of this struct so it should only contain global data, nothing specific to a frame.
 */
struct FGPUProfiler
{
	/** Whether we are currently tracking perf events or not. */
	bool bTrackingEvents;

	/** Whether we are currently tracking data for gpucrash debugging or not */
	bool bTrackingGPUCrashData;

	/** A latched version of GTriggerGPUProfile. This is a form of pseudo-thread safety. We read the value once a frame only. */
	bool bLatchedGProfilingGPU;

	/** A latched version of GTriggerGPUHitchProfile. This is a form of pseudo-thread safety. We read the value once a frame only. */
	bool bLatchedGProfilingGPUHitches;

	/** The previous latched version of GTriggerGPUHitchProfile.*/
	bool bPreviousLatchedGProfilingGPUHitches;

	/** Original state of GEmitDrawEvents before it was overridden for profiling. */
	bool bOriginalGEmitDrawEvents;

	/** GPU hitch profile history debounce...after a hitch, we just ignore frames for a while */
	int32 GPUHitchDebounce;

	/** scope depth to record crash data depth. to limit perf/mem requirements */
	int32 GPUCrashDataDepth;

	/** Current perf event node frame. */
	FGPUProfilerEventNodeFrame* CurrentEventNodeFrame;

	/** Current perf event node. */
	FGPUProfilerEventNode* CurrentEventNode;

	int32 StackDepth;

	FGPUProfiler() :
		bTrackingEvents(false),
		bTrackingGPUCrashData(false),
		bLatchedGProfilingGPU(false),
		bLatchedGProfilingGPUHitches(false),
		bPreviousLatchedGProfilingGPUHitches(false),
		bOriginalGEmitDrawEvents(false),
		GPUHitchDebounce(0),
		GPUCrashDataDepth(-1),
		CurrentEventNodeFrame(NULL),
		CurrentEventNode(NULL),
		StackDepth(0)
	{
	}

	virtual ~FGPUProfiler()
	{
	}

	void RegisterGPUWork(uint32 NumDraws, uint32 NumPrimitives, uint32 NumVertices)
	{
		if (bTrackingEvents && CurrentEventNode)
		{
			check(IsInRenderingThread() || IsInRHIThread());
			CurrentEventNode->NumDraws += NumDraws;
			CurrentEventNode->NumPrimitives += NumPrimitives;
			CurrentEventNode->NumVertices += NumVertices;
		}
	}

	void RegisterGPUWork(uint32 NumPrimitives = 0, uint32 NumVertices = 0)
	{
		RegisterGPUWork(1, NumPrimitives, NumVertices);
	}

	void RegisterGPUDispatch(FIntVector GroupCount)
	{
		if (bTrackingEvents && CurrentEventNode)
		{
			check(IsInRenderingThread() || IsInRHIThread());
			CurrentEventNode->NumDispatches++;
			CurrentEventNode->GroupCount = GroupCount;
		}
	}

	virtual FGPUProfilerEventNode* CreateEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent)
	{
		return new FGPUProfilerEventNode(InName, InParent);
	}

	RHI_API virtual void PushEvent(const TCHAR* Name, FColor Color);
	RHI_API virtual void PopEvent();
};