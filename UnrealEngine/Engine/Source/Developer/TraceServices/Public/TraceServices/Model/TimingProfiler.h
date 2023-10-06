// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/StringFwd.h"
#include "HAL/Platform.h"
#include "HAL/PlatformMath.h"
#include "Model/MonotonicTimeline.h"
#include "TraceServices/Common/CancellationToken.h"
#include "TraceServices/Containers/Tables.h"
#include "TraceServices/Containers/Timelines.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "UObject/NameTypes.h"

namespace TraceServices
{
template <typename InEventType> class IEditableTimeline;
template <typename RowType> class ITable;

struct FTimingProfilerTimer
{
	const TCHAR* Name = nullptr;
	const TCHAR* File = nullptr;
	uint32 Id = 0;
	union
	{
		struct
		{
			uint32 Line : 24;
			uint32 IsGpuTimer : 1;
		};
		uint32 LineAndFlags = 0; // used only to default initialize Line and IsGpuTimer with 0
	};
};

struct FTimingProfilerEvent
{
	uint32 TimerIndex = uint32(-1);
};

struct FTimingProfilerAggregatedStats
{
	const FTimingProfilerTimer* Timer = nullptr;
	uint64 InstanceCount = 0;
	double TotalInclusiveTime = 0.0;
	double MinInclusiveTime = DBL_MAX;
	double MaxInclusiveTime = -DBL_MAX;
	double AverageInclusiveTime = 0.0;
	double MedianInclusiveTime = 0.0;
	double TotalExclusiveTime = 0.0;
	double MinExclusiveTime = DBL_MAX;
	double MaxExclusiveTime = -DBL_MAX;
	double AverageExclusiveTime = 0.0;
	double MedianExclusiveTime = 0.0;
};

struct FTimingProfilerButterflyNode
{
	const FTimingProfilerTimer* Timer = nullptr;
	uint64 Count = 0;
	double InclusiveTime = 0.0;
	double ExclusiveTime = 0.0;
	const FTimingProfilerButterflyNode* Parent = nullptr;
	TArray<FTimingProfilerButterflyNode*> Children;
};

struct FCreateAggreationParams
{
	double IntervalStart;
	double IntervalEnd;
	TFunction<bool(uint32)> CpuThreadFilter;
	bool IncludeGpu;
	ETraceFrameType FrameType = ETraceFrameType::TraceFrameType_Count;
	TSharedPtr<TraceServices::FCancellationToken> CancellationToken;
};

class ITimingProfilerButterfly
{
public:
	virtual ~ITimingProfilerButterfly() = default;
	virtual const FTimingProfilerButterflyNode& GenerateCallersTree(uint32 TimerId) = 0;
	virtual const FTimingProfilerButterflyNode& GenerateCalleesTree(uint32 TimerId) = 0;
};

class ITimingProfilerTimerReader
{
public:
	virtual const FTimingProfilerTimer* GetTimer(uint32 TimerId) const = 0;
	virtual uint32 GetTimerCount() const = 0;
	virtual TArrayView<const uint8> GetMetadata(uint32 TimerId) const = 0;
};

class ITimingProfilerProvider
	: public IProvider
{
public:
	typedef ITimeline<FTimingProfilerEvent> Timeline;

	virtual ~ITimingProfilerProvider() = default;
	virtual bool GetCpuThreadTimelineIndex(uint32 ThreadId, uint32& OutTimelineIndex) const = 0;
	virtual bool GetGpuTimelineIndex(uint32& OutTimelineIndex) const = 0;
	virtual bool GetGpu2TimelineIndex(uint32& OutTimelineIndex) const = 0;
	virtual bool ReadTimeline(uint32 Index, TFunctionRef<void(const Timeline&)> Callback) const = 0;
	virtual uint32 GetTimelineCount() const = 0;
	virtual void EnumerateTimelines(TFunctionRef<void(const Timeline&)> Callback) const = 0;
	virtual void ReadTimers(TFunctionRef<void(const ITimingProfilerTimerReader&)> Callback) const = 0;

	/**
	* Create a table of aggregated stats.
	*
	* @param Params				The params for the aggregation.
	*/
	virtual ITable<FTimingProfilerAggregatedStats>* CreateAggregation(const FCreateAggreationParams& Params) const = 0;

	/**
	* Create a table of aggregated stats.
	*
	* @param IntervalStart		The start timestamp in seconds.
	* @param IntervalEnd		The end timestamp in seconds.
	* @param CpuThreadFilter	A function to filter the CPU threads to aggregate.
	* @param IncludeGpu			A boolean value to specify if aggregaton should include GPU timelines.
	* @param FrameType			The type of frame to use for frame stats aggregation. ETraceFrameType::TraceFrameType_Count means no frame aggregation.
	*/
	virtual ITimingProfilerButterfly* CreateButterfly(double IntervalStart, double IntervalEnd, TFunctionRef<bool(uint32)> CpuThreadFilter, bool IncludeGpu) const = 0;
};

/*
* An interface that can consume timeline CpuProfiler events from a session.
*/
class IEditableTimingProfilerProvider
	: public IEditableProvider
{
public:
	virtual ~IEditableTimingProfilerProvider() = default;

	/*
	* A new CPU timer object has been found.
	*
	* @param Name	The name attached to the timer.
	* @param File	The source file in which the timer is defined.
	* @param Line	The line number of the source file in which the timer is defined.
	*
	* @return The identity of the CPU timer object.
	*/
	virtual uint32 AddCpuTimer(FStringView Name, const TCHAR* File, uint32 Line) = 0;

	/*
	* Update an existing timer with information. Some information is unavailable when it's created.
	*
	* @param TimerId	The identity of the timer to update.
	* @param Name		The name attached to the timer.
	* @param File		The source file in which the timer is defined.
	* @param Line		The line number of the source file in which the timer is defined.
	*/
	virtual void SetTimerNameAndLocation(uint32 TimerId, FStringView Name, const TCHAR* File, uint32 Line) = 0;

	/*
	* Add metadata to a timer.
	*
	* @param OriginalTimerId	The identity of the timer to add metadata to.
	* @param Metadata			The metadata.
	*
	* @return The identity of the metadata.
	*/
	virtual uint32 AddMetadata(uint32 OriginalTimerId, TArray<uint8>&& Metadata) = 0;

	/*
	* Get metadata for a timer.
	*
	* @param TimerId	The identity of the metadata to retrieve.
	*
	* @return The metadata.
	*/
	virtual TArrayView<const uint8> GetMetadata(uint32 TimerId) const = 0;

	/*
	* Get an object to receive ordered timeline events for a thread.
	*
	* @param ThreadId	The thread for which the events are for.
	*
	* @return The object to receive the serial events for the specified thread.
	*/
	virtual IEditableTimeline<FTimingProfilerEvent>& GetCpuThreadEditableTimeline(uint32 ThreadId) = 0;
};

TRACESERVICES_API FName GetTimingProfilerProviderName();
TRACESERVICES_API const ITimingProfilerProvider* ReadTimingProfilerProvider(const IAnalysisSession& Session);
TRACESERVICES_API IEditableTimingProfilerProvider* EditTimingProfilerProvider(IAnalysisSession& Session);

} // namespace TraceServices
