// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataCacheStats.h"
#include "DerivedDataLegacyCacheStore.h"

#include "Algo/BinarySearch.h"

namespace UE::DerivedData
{

void FRequestCounter::AddRequest(const ERequestType Type, const ERequestOp Op, const EStatus Status)
{
	switch (Type)
	{
	case ERequestType::Record:
		switch (Op)
		{
		case ERequestOp::Put:
			++(Status == EStatus::Ok ? PutRecord.Hits : PutRecord.Misses);
			break;
		case ERequestOp::Get:
			++(Status == EStatus::Ok ? GetRecord.Hits : GetRecord.Misses);
			break;
		case ERequestOp::GetChunk:
			++(Status == EStatus::Ok ? GetRecordChunk.Hits : GetRecordChunk.Misses);
			break;
		}
		break;
	case ERequestType::Value:
		switch (Op)
		{
		case ERequestOp::Put:
			++(Status == EStatus::Ok ? PutValue.Hits : PutValue.Misses);
			break;
		case ERequestOp::Get:
			++(Status == EStatus::Ok ? GetValue.Hits : GetValue.Misses);
			break;
		case ERequestOp::GetChunk:
			++(Status == EStatus::Ok ? GetValueChunk.Hits : GetValueChunk.Misses);
			break;
		}
		break;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimeAveragedStat::Add(FMonotonicTimePoint StartTime, FMonotonicTimePoint EndTime, double Value)
{
	checkf(StartTime <= EndTime, TEXT("StartTime %.3f is later than EndTime %.3f."), StartTime.ToSeconds(), EndTime.ToSeconds());

	TUniqueLock Lock(Mutex);

	if (EndTime <= LastTime - Period)
	{
		return;
	}
	else
	{
		const int32 Index = Algo::LowerBoundBy(EndValues, EndTime, &FValue::Time);
		EndValues.Insert({EndTime, Value}, Index);
	}

	if (StartTime < LastTime)
	{
		AccumulatedValue += Value;
		++AccumulatedValueCount;
	}
	else
	{
		const int32 Index = Algo::LowerBoundBy(StartValues, StartTime, &FValue::Time);
		StartValues.Insert({StartTime, Value}, Index);
	}

	const int32 Count = ActiveRanges.Num();
	const int32 StartIndex = Algo::LowerBoundBy(ActiveRanges, StartTime, &FRange::EndTime);
	if (StartIndex == Count)
	{
		ActiveRanges.Add({StartTime, EndTime});
	}
	else
	{
		FRange& StartRange = ActiveRanges[StartIndex];
		if (EndTime < StartRange.StartTime)
		{
			ActiveRanges.Insert({StartTime, EndTime}, StartIndex);
		}
		else
		{
			if (StartTime < StartRange.StartTime)
			{
				StartRange.StartTime = StartTime;
			}

			const int32 EndIndex = Algo::LowerBoundBy(ActiveRanges, EndTime, &FRange::StartTime);
			StartRange.EndTime = FMath::Max(EndTime, ActiveRanges[EndIndex - 1].EndTime);

			ActiveRanges.RemoveAt(StartIndex + 1, EndIndex - StartIndex - 1, EAllowShrinking::No);
		}
	}

	// Avoid unbounded growth by updating when behind by more than one period.
	if (LastTime + Period < EndTime)
	{
		Update(EndTime);
	}
}

double FTimeAveragedStat::GetRate(FMonotonicTimePoint Time)
{
	TUniqueLock Lock(Mutex);
	if (LastTime < Time)
	{
		Update(Time);
	}
	return AverageRate;
}

double FTimeAveragedStat::GetValue(FMonotonicTimePoint Time)
{
	TUniqueLock Lock(Mutex);
	if (LastTime < Time)
	{
		Update(Time);
	}
	return AverageValue;
}

void FTimeAveragedStat::Update(FMonotonicTimePoint Time)
{
	LastTime = Time;

	const FMonotonicTimePoint StartTime = Time - Period;
	const FMonotonicTimePoint EndTime = Time;

	// NOTE: Values are not being prorated when entering or exiting their time range.
	//       While this is technically inaccurate, the error will likely be small due
	//       to the use case being requests of one second averaged over one minute.

	// Add values entering the current period.
	int32 StartCount = 0;
	for (const FValue& Value : StartValues)
	{
		if (EndTime < Value.Time)
		{
			break;
		}
		AccumulatedValue += Value.Value;
		++AccumulatedValueCount;
		++StartCount;
	}
	StartValues.RemoveAt(0, StartCount, EAllowShrinking::No);

	// Remove values exiting the current period.
	int32 EndCount = 0;
	for (const FValue& Value : EndValues)
	{
		if (StartTime <= Value.Time)
		{
			break;
		}
		AccumulatedValue -= Value.Value;
		--AccumulatedValueCount;
		++EndCount;
	}
	EndValues.RemoveAt(0, EndCount, EAllowShrinking::No);

	// Remove ranges before the current period.
	int32 RangeCount = 0;
	for (const FRange& Range : ActiveRanges)
	{
		if (StartTime < Range.EndTime)
		{
			break;
		}
		++RangeCount;
	}
	ActiveRanges.RemoveAt(0, RangeCount, EAllowShrinking::No);

	// Accumulate active time in the current period.
	FMonotonicTimeSpan ActiveTime;
	for (const FRange& Range : ActiveRanges)
	{
		const FMonotonicTimePoint RangeStart = FMath::Max(Range.StartTime, StartTime);
		const FMonotonicTimePoint RangeEnd = FMath::Min(Range.EndTime, EndTime);
		ActiveTime += RangeEnd - RangeStart;
	}

	if (AccumulatedValueCount)
	{
		AverageRate = !ActiveTime.IsZero() ? AccumulatedValue / ActiveTime.ToSeconds() : 0.0;
		AverageValue = AccumulatedValue / AccumulatedValueCount;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FCacheStoreStats::FCacheStoreStats(FCacheStats& InCacheStats, ECacheStoreFlags InFlags, FStringView InType, FStringView InName, FStringView InPath)
	: Type(InType)
	, Name(InName)
	, Path(InPath)
	, Flags(InFlags)
	, CacheStats(InCacheStats)
{
	const FMonotonicTimeSpan Period = FMonotonicTimeSpan::FromSeconds(60.0);
	AverageLatency.SetPeriod(Period);
	AveragePhysicalReadSize.SetPeriod(Period);
	AveragePhysicalWriteSize.SetPeriod(Period);
}

void FCacheStoreStats::SetFlags(ECacheStoreFlags InFlags)
{
	TUniqueLock Lock(Mutex);
	Flags = InFlags;
}

void FCacheStoreStats::SetStatus(ECacheStoreStatusCode InStatusCode, const FText& InStatus)
{
	TUniqueLock Lock(Mutex);
	Status = InStatus;
	StatusCode = InStatusCode;
}

void FCacheStoreStats::SetAttribute(FStringView Key, FStringView Value)
{
	TUniqueLock Lock(Mutex);
	Attributes.Emplace(Key, Value);
}

void FCacheStoreStats::AddRequest(const FCacheStoreRequestStats& Stats)
{
	checkf(Stats.Bucket.IsValid(), TEXT("Stats for request '%s' did not set a bucket."), *Stats.Name);

#if ENABLE_COOK_STATS
	using FCallStats = FCookStats::CallStats;
	using EHitOrMiss = FCallStats::EHitOrMiss;
	using EStatType = FCallStats::EStatType;

	const EHitOrMiss HitOrMiss = Stats.Status == EStatus::Ok ? EHitOrMiss::Hit : EHitOrMiss::Miss;
	const bool bIsGet = Stats.Op != ECacheStoreRequestOp::Put;
	const bool bIsInGameThread = IsInGameThread();
#endif

	{
		// Accumulate only physical size and time from each cache store.
		// Request count and logical size are tracked by the cache hierarchy.

		FCacheBucketStats& BucketStats = CacheStats.GetBucket(Stats.Bucket);
		TUniqueLock Lock(BucketStats.Mutex);

		BucketStats.PhysicalReadSize += Stats.PhysicalReadSize;
		BucketStats.PhysicalWriteSize += Stats.PhysicalWriteSize;
		BucketStats.MainThreadTime += Stats.MainThreadTime;
		BucketStats.OtherThreadTime += Stats.OtherThreadTime;

	#if ENABLE_COOK_STATS
		FCallStats& CallStats = bIsGet ? BucketStats.GetStats : BucketStats.PutStats;
		CallStats.Accumulate(HitOrMiss, EStatType::Cycles, int64(Stats.MainThreadTime.ToSeconds() / FPlatformTime::GetSecondsPerCycle64()), /*bIsInGameThread*/ true);
		CallStats.Accumulate(HitOrMiss, EStatType::Cycles, int64(Stats.OtherThreadTime.ToSeconds() / FPlatformTime::GetSecondsPerCycle64()), /*bIsInGameThread*/ false);
		CallStats.Accumulate(HitOrMiss, EStatType::Bytes, bIsGet ? Stats.PhysicalReadSize : Stats.PhysicalWriteSize, bIsInGameThread);
	#endif
	}

	TUniqueLock Lock(Mutex);

	LogicalReadSize += Stats.LogicalReadSize;
	LogicalWriteSize += Stats.LogicalWriteSize;
	PhysicalReadSize += Stats.PhysicalReadSize;
	PhysicalWriteSize += Stats.PhysicalWriteSize;

	MainThreadTime += Stats.MainThreadTime;
	OtherThreadTime += Stats.OtherThreadTime;

	RequestCount.AddRequest(Stats.Type, Stats.Op, Stats.Status);

	if (!Stats.Latency.IsInfinity())
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("%s: Adding request latency of %.2fms from %s on %s with status %s"),
			*Name, Stats.Latency.ToMilliseconds(), LexToString(Stats.Op), LexToString(Stats.Type), *WriteToString<32>(Stats.Status));
		AverageLatency.Add(Stats.StartTime, Stats.EndTime, Stats.Latency.ToSeconds());
	}

	if (Stats.PhysicalReadSize && !Stats.LogicalWriteSize)
	{
		// Skip physical reads for put operations because they pull down the read rate in time periods with few gets.
		AveragePhysicalReadSize.Add(Stats.StartTime, Stats.EndTime, double(Stats.PhysicalReadSize));
	}

	if (Stats.PhysicalWriteSize && !Stats.LogicalReadSize)
	{
		// Skip physical writes for get operations because they pull down the write rate in time periods with few puts.
		AveragePhysicalWriteSize.Add(Stats.StartTime, Stats.EndTime, double(Stats.PhysicalWriteSize));
	}

#if ENABLE_COOK_STATS
	FCookStats::CallStats& CallStats = bIsGet ? GetStats : PutStats;
	CallStats.Accumulate(HitOrMiss, EStatType::Counter, 1, bIsInGameThread);
	CallStats.Accumulate(HitOrMiss, EStatType::Cycles, int64(Stats.MainThreadTime.ToSeconds() / FPlatformTime::GetSecondsPerCycle64()), /*bIsInGameThread*/ true);
	CallStats.Accumulate(HitOrMiss, EStatType::Cycles, int64(Stats.OtherThreadTime.ToSeconds() / FPlatformTime::GetSecondsPerCycle64()), /*bIsInGameThread*/ false);
	CallStats.Accumulate(HitOrMiss, EStatType::Bytes, bIsGet ? Stats.PhysicalReadSize : Stats.PhysicalWriteSize, bIsInGameThread);
#endif
}

void FCacheStoreStats::AddLatency(FMonotonicTimePoint StartTime, FMonotonicTimePoint EndTime, FMonotonicTimeSpan Latency)
{
	if (!Latency.IsInfinity())
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("%s: Adding non-request latency of %.2fms"),
			*Name, Latency.ToMilliseconds());
		AverageLatency.Add(StartTime, EndTime, Latency.ToSeconds());
	}
}

double FCacheStoreStats::GetAverageLatency()
{
	TUniqueLock Lock(Mutex);
	return AverageLatency.GetValue(FMonotonicTimePoint::Now());
}

void FCacheStoreStats::SetTotalPhysicalSize(uint64 InTotalPhysicalSize)
{
	TUniqueLock Lock(Mutex);
	TotalPhysicalSize = InTotalPhysicalSize;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FCacheBucketStats& FCacheStats::GetBucket(FCacheBucket Bucket)
{
	TUniqueLock Lock(Mutex);
	const int32 Index = Algo::LowerBoundBy(BucketStats, Bucket, [](TUniquePtr<FCacheBucketStats>& Stats) { return Stats->Bucket; });
	if (!BucketStats.IsValidIndex(Index) || BucketStats[Index]->Bucket != Bucket)
	{
		FCacheBucketStats* Stats = new FCacheBucketStats;
		Stats->Bucket = Bucket;
		BucketStats.EmplaceAt(Index, Stats);
	}
	return *BucketStats[Index];
}

} // UE::DerivedData
