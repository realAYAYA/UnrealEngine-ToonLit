// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/PagedArray.h" // TraceServices
#include "HAL/Platform.h"
#include "Templates/Function.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "UObject/NameTypes.h"

namespace TraceServices
{

struct TRACESERVICES_API FTimeRegion
{
	double BeginTime = std::numeric_limits<double>::infinity();
	double EndTime = std::numeric_limits<double>::infinity();
	const TCHAR* Text = nullptr;
	int32 Depth = -1;
};

class TRACESERVICES_API FRegionLane
{
	friend class FRegionProvider;

public:
	FRegionLane(ILinearAllocator& InAllocator) : Regions(InAllocator, 512) {}

	int32 Num() const { return static_cast<int32>(Regions.Num()); }

	/**
	 * Call Callback for every region overlapping the interval defined by IntervalStart and IntervalEnd
	 * @param Callback a callback called for each region. Return false to abort iteration.
	 * @returns true if the enumeration finished, false if it was aborted by the callback returning false
	 */
	bool EnumerateRegions(double IntervalStart, double IntervalEnd, TFunctionRef<bool(const FTimeRegion&)> Callback) const;

private:
	TPagedArray<FTimeRegion> Regions;
};

class TRACESERVICES_API IRegionProvider
	: public IProvider
{
public:
	virtual ~IRegionProvider() override = default;

	/**
	 * @return the amount of currently known regions (including open-ended ones)
	 */
	virtual uint64 GetRegionCount() const = 0;

	/**
	 * @return the number of lanes
	 */
	virtual int32 GetLaneCount() const = 0;

	/**
	 * Direct access to a certain lane at a given index/depth.
	 * The pointer is valid only in the current read scope.
	 * @return a pointer to the lane at the specified depth index or nullptr if Index > GetLaneCount()-1
	 */
	virtual const FRegionLane* GetLane(int32 Index) const = 0;

	/**
	 * Enumerates all regions that overlap a certain time interval. Will enumerate by depth but does not expose lanes.
	 * @param Callback a callback called for each region. Return false to abort iteration.
	 * @returns true if the enumeration finished, false if it was aborted by the callback returning false
	 */
	virtual bool EnumerateRegions(double IntervalStart, double IntervalEnd, TFunctionRef<bool(const FTimeRegion&)> Callback) const = 0;

	/**
	 * Will call Callback(Lane, Depth) for each lane in order.
	 */
	virtual void EnumerateLanes(TFunctionRef<void(const FRegionLane&, const int32)> Callback) const = 0;

	/**
	 * @return A monotonically increasing counter that changes each time new data is added to the provider.
	 * This can be used to detect when to update any (UI-)state dependent on the provider during analysis.
	 */
	virtual uint64 GetUpdateCounter() const = 0;
};

/**
 * The interface to a provider that can consume mutations of region events from a session.
 */
class TRACESERVICES_API IEditableRegionProvider
	: public IEditableProvider
{
public:
	virtual ~IEditableRegionProvider() override = default;

	/**
	 * Append a new begin event of a region from the trace session.
	 *
	 * @param Name		The string name of the region.
	 * @param Time		The time in seconds of the begin event of this region.
	 */
	virtual void AppendRegionBegin(const TCHAR* Name, double Time) = 0;

	/**
	 * Append a new end event of a region from the trace session.
	 *
	 * @param Name		The string name of the region.
	 * @param Time		The time in seconds of the end event of this region.
	 */
	virtual void AppendRegionEnd(const TCHAR* Name, double Time) = 0;

	/**
	 * Called from the analyzer once all events have been processed.
	 * Allows post-processing and error reporting for regions that were never closed.
	 */
	virtual void OnAnalysisSessionEnded() = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

TRACESERVICES_API FName GetRegionProviderName();
TRACESERVICES_API const IRegionProvider& ReadRegionProvider(const IAnalysisSession& Session);
TRACESERVICES_API IEditableRegionProvider& EditRegionProvider(IAnalysisSession& Session);

} // namespace TraceServices
