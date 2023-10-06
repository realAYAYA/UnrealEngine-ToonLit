// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/BitArray.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "Templates/Function.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "UObject/NameTypes.h"

namespace TraceServices
{

typedef int32 FMemoryTrackerId;
typedef int64 FMemoryTagId;

struct FMemoryTrackerInfo
{
	static const FMemoryTrackerId InvalidTrackerId = -1;
	static const FMemoryTrackerId MaxValidTrackerId = 63;

	// Unique identifier of the llm tracker. Can also be used as an index, limited to [0 .. 63] range.
	FMemoryTrackerId Id;

	// Name of the llm tracker.
	FString Name;
};

struct FMemoryTagInfo
{
	static const FMemoryTagId InvalidTagId = 0;

	// Unique identifier of the llm tag.
	FMemoryTagId Id;

	// Name of the stat associated with the llm tag.
	FString Name;

	// Id of parent, 0 if no parent.
	FMemoryTagId ParentId;

	// Bit flags for trackers using this llm tag.
	// The bit position represents the tracker id; this limits the valid tracker ids to range [0 .. 63].
	// It can be updated during analysis (as new trackers / snapshots are analyzed).
	uint64 Trackers;
};

struct FMemoryTagSample
{
	// Value at sample time.
	int64 Value;
};

class IMemoryProvider : public IProvider
{
public:
	virtual ~IMemoryProvider() = default;

	/**
	 * Unique serial index that changes when new tags are registered or when the Trackers flags is updated for a tag.
	 * @return Unique index
	 */
	virtual uint32 GetTagSerial() const = 0;

	/**
	 * Return the number of registered tags.
	 * @return Number of registered tags.
	 */
	virtual uint32 GetTagCount() const = 0;

	/**
	 * Enumerate the registered tags.
	 * @param Callback Function that will be called for each registered tag.
	 */
	virtual void EnumerateTags(TFunctionRef<void(const FMemoryTagInfo&)> Callback) const = 0;

	/**
	 * Returns the meta data for a tag id.
	 * @param Id Tag id
	 * @return Memory tag information.
	 */
	virtual const FMemoryTagInfo* GetTag(FMemoryTagId Id) const = 0;

	/**
	 * Gets the number of samples for a given tag from a given tracker.
	 * @param Tracker Tracer index.
	 * @param Tag The id of the LLM tag.
	 * @return Number of samples that has been recorded.
	 */
	virtual uint64 GetTagSampleCount(FMemoryTrackerId Tracker, FMemoryTagId Tag) const = 0;

	/**
	 * Return the number of registered tracker descriptions.
	 * @return Number of tracker descriptions.
	 */
	virtual uint32 GetTrackerCount() const = 0;

	/**
	 * Enumerate the registered trackers.
	 * @param Callback Function that is called for each registered tracker.
	 */
	virtual void EnumerateTrackers(TFunctionRef<void(const FMemoryTrackerInfo&)> Callback) const = 0;

	/** Enumerates samples (values) for a specified LLM tag.
	 * @param Tracker Tracker index.
	 * @param Tag The id/index of the LLM tag.
	 * @param StartSample The inclusive start index in the sample array of specified memory tag.
	 * @param EndSample The exclusive end index in the sample array of specified memory tag.
	 * @param Callback A callback function called for each sample enumerated.
	 * @param bIncludeRangeNeighbours Includes the sample immediately before and after the selected range.
	 */
	virtual void EnumerateTagSamples(FMemoryTrackerId Tracker, FMemoryTagId Tag, double StartTime, double EndTime, bool bIncludeRangeNeighbours, TFunctionRef<void(double Time, double Duration, const FMemoryTagSample&)> Callback) const = 0;
};

TRACESERVICES_API FName GetMemoryProviderName();
TRACESERVICES_API const IMemoryProvider* ReadMemoryProvider(const IAnalysisSession& Session);

} // namespace TraceServices
