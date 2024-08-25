// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataCacheStore.h"

#include "Async/UniqueLock.h"
#include "CoreGlobals.h"
#include "DerivedDataCacheRecord.h"
#include "DerivedDataValue.h"
#include "Serialization/CompactBinary.h"

namespace UE::DerivedData
{

FCacheStoreRequestTimer::FCacheStoreRequestTimer(FCacheStoreRequestStats& OutStats)
	: Stats(OutStats)
	, StartTime(FMonotonicTimePoint::Now())
{
}

void FCacheStoreRequestTimer::Stop()
{
	if (StartTime.IsInfinity())
	{
		return;
	}
	const FMonotonicTimePoint EndTime = FMonotonicTimePoint::Now();
	const bool bIsInGameThread = IsInGameThread();
	TUniqueLock Lock(Stats.Mutex);
	(bIsInGameThread ? Stats.MainThreadTime : Stats.OtherThreadTime) += EndTime - StartTime;
	Stats.StartTime = FMath::Min(Stats.StartTime, StartTime);
	Stats.EndTime = FMath::Max(Stats.EndTime, EndTime);
	StartTime = FMonotonicTimePoint::Infinity();
}

void FCacheStoreRequestStats::AddLatency(const FMonotonicTimeSpan InLatency)
{
	if (Latency > InLatency)
	{
		Latency = InLatency;
	}
}

void FCacheStoreRequestStats::AddLogicalRead(const FCacheRecord& Record)
{
	if (const FCbObject& Meta = Record.GetMeta())
	{
		LogicalReadSize += Meta.GetSize();
	}

	for (const FValueWithId& Value : Record.GetValues())
	{
		AddLogicalRead(Value);
	}
}

void FCacheStoreRequestStats::AddLogicalRead(const FValue& Value)
{
	if (Value.HasData())
	{
		LogicalReadSize += Value.GetRawSize();
	}
}

void FCacheStoreRequestStats::AddLogicalWrite(const FValue& Value)
{
	if (Value.HasData())
	{
		LogicalWriteSize += Value.GetRawSize();
	}
}

} // UE::DerivedData
