// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "ProfilingDebugging/MemoryTrace.h"
#include "Templates/Function.h"
#include "Templates/UniquePtr.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "UObject/NameTypes.h"

#include <new>

namespace TraceServices
{

// Id type for tags
typedef uint32 TagIdType;

class IAllocationsProvider : public IProvider
{
public:
	// Allocation query rules.
	// The enum uses the following naming convention:
	//     A, B, C, D = time markers
	//     a = time when "alloc" event occurs
	//     f = time when "free" event occurs (can be infinite)
	// Ex.: "AaBf" means "all memory allocations allocated between time A and time B and freed after time B".
	enum class EQueryRule
	{
		aAf,     // active allocs at A
		afA,     // before
		Aaf,     // after
		aAfB,    // decline
		AaBf,    // growth
		aAfaBf,  // decline + growth (used as "growth vs. decline")
		AfB,     // free events
		AaB,     // alloc events
		AafB,    // short living allocs
		aABf,    // long living allocs
		AaBCf,   // memory leaks
		AaBfC,   // limited lifetime
		aABfC,   // decline of long living allocs
		AaBCfD,  // specific lifetime
		//A_vs_B,  // compare A vs. B; {aAf} vs. {aBf}
		//A_or_B,  // live at A or at B; {aAf} U {aBf}
		//A_xor_B, // live either at A or at B; ({aAf} U {aBf}) \ {aABf}
	};

	struct TRACESERVICES_API FQueryParams
	{
		EQueryRule Rule;
		double TimeA;
		double TimeB;
		double TimeC;
		double TimeD;
	};

	struct TRACESERVICES_API FAllocation
	{
		uint32 GetStartEventIndex() const;
		uint32 GetEndEventIndex() const;
		double GetStartTime() const;
		double GetEndTime() const;
		uint64 GetAddress() const;
		uint64 GetSize() const;
		uint32 GetAlignment() const;
		uint32 GetAllocThreadId() const;
		uint32 GetFreeThreadId() const;
		uint32 GetAllocCallstackId() const;
		uint32 GetFreeCallstackId() const;
		uint32 GetMetadataId() const;
		TagIdType GetTag() const;
		HeapId GetRootHeap() const;
		bool IsHeap() const;
	};

	class TRACESERVICES_API FAllocations
	{
	public:
		void operator delete (void* Address);
		uint32 Num() const;
		const FAllocation* Get(uint32 Index) const;
	};

	typedef TUniquePtr<const FAllocations> FQueryResult;

	enum class EQueryStatus
	{
		Unknown,
		Done,
		Working,
		Available,
	};

	struct TRACESERVICES_API FQueryStatus
	{
		FQueryResult NextResult() const;

		EQueryStatus Status;
		mutable UPTRINT Handle;
	};

	struct TRACESERVICES_API FHeapSpec
	{
		HeapId Id;
		FHeapSpec* Parent;
		TArray<FHeapSpec*> Children;
		const TCHAR* Name;
		EMemoryTraceHeapFlags Flags;
	};

	typedef UPTRINT FQueryHandle;

public:
	virtual ~IAllocationsProvider() = default;

	virtual void BeginRead() const = 0;
	virtual void EndRead() const = 0;
	virtual void ReadAccessCheck() const = 0;

	virtual bool IsInitialized() const = 0;

	// Enumerates the discovered tags.
	virtual void EnumerateTags(TFunctionRef<void(const TCHAR*, const TCHAR*, TagIdType, TagIdType)> Callback) const = 0;

	// Returns the display name of the specified LLM tag.
	// Lifetime of returned string matches the session lifetime.
	virtual const TCHAR* GetTagName(TagIdType Tag) const = 0;
	virtual const TCHAR* GetTagFullPath(TagIdType Tag) const = 0;

	virtual void EnumerateRootHeaps(TFunctionRef<void(HeapId Id, const FHeapSpec&)> Callback) const = 0;
	virtual void EnumerateHeaps(TFunctionRef<void(HeapId Id, const FHeapSpec&)> Callback) const = 0;

	// Returns the number of points in each timeline (Min/Max Total Allocated Memory, Min/Max Live Allocations, Total Alloc Events, Total Free Events).
	virtual int32 GetTimelineNumPoints() const = 0;

	// Returns the inclusive index range [StartIndex, EndIndex] for a time range [StartTime, EndTime].
	// Index values are in range { -1, 0, .. , N-1, N }, where N = GetTimelineNumPoints().
	virtual void GetTimelineIndexRange(double StartTime, double EndTime, int32& StartIndex, int32& EndIndex) const = 0;

	// Enumerates the Min Total Allocated Memory timeline points in the inclusive index interval [StartIndex, EndIndex].
	virtual void EnumerateMinTotalAllocatedMemoryTimeline(int32 StartIndex, int32 EndIndex, TFunctionRef<void(double Time, double Duration, uint64 Value)> Callback) const = 0;

	// Enumerates the Max Total Allocated Memory timeline points in the inclusive index interval [StartIndex, EndIndex].
	virtual void EnumerateMaxTotalAllocatedMemoryTimeline(int32 StartIndex, int32 EndIndex, TFunctionRef<void(double Time, double Duration, uint64 Value)> Callback) const = 0;

	// Enumerates the Min Live Allocations timeline points in the inclusive index interval [StartIndex, EndIndex].
	virtual void EnumerateMinLiveAllocationsTimeline(int32 StartIndex, int32 EndIndex, TFunctionRef<void(double Time, double Duration, uint32 Value)> Callback) const = 0;

	// Enumerates the Max Live Allocations timeline points in the inclusive index interval [StartIndex, EndIndex].
	virtual void EnumerateMaxLiveAllocationsTimeline(int32 StartIndex, int32 EndIndex, TFunctionRef<void(double Time, double Duration, uint32 Value)> Callback) const = 0;

	// Enumerates the Alloc Events timeline points in the inclusive index interval [StartIndex, EndIndex].
	virtual void EnumerateAllocEventsTimeline(int32 StartIndex, int32 EndIndex, TFunctionRef<void(double Time, double Duration, uint32 Value)> Callback) const = 0;

	// Enumerates the Free Events timeline points in the inclusive index interval [StartIndex, EndIndex].
	virtual void EnumerateFreeEventsTimeline(int32 StartIndex, int32 EndIndex, TFunctionRef<void(double Time, double Duration, uint32 Value)> Callback) const = 0;

	virtual FQueryHandle StartQuery(const FQueryParams& Params) const = 0;
	virtual void CancelQuery(FQueryHandle Query) const = 0;
	virtual const FQueryStatus PollQuery(FQueryHandle Query) const = 0;
};

TRACESERVICES_API FName GetAllocationsProviderName();
TRACESERVICES_API const IAllocationsProvider* ReadAllocationsProvider(const IAnalysisSession& Session);

} // namespace TraceServices
