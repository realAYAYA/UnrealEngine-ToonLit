// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Mutex.h"
#include "Async/UniqueLock.h"
#include "Containers/Array.h"
#include "DerivedDataCacheKey.h"
#include "DerivedDataCacheStore.h"
#include "Internationalization/Text.h"
#include "Misc/MonotonicTime.h"
#include "ProfilingDebugging/CookStats.h"

namespace UE::DerivedData { class FCacheStats; }

namespace UE::DerivedData
{

class FRequestCounter
{
	struct FCounter
	{
		uint64 Hits = 0;
		uint64 Misses = 0;
	};

	using ERequestOp = ECacheStoreRequestOp;
	using ERequestType = ECacheStoreRequestType;

public:
	void AddRequest(ERequestType Type, ERequestOp Op, EStatus Status);

	constexpr uint64 Puts() const { return PutHits() + PutMisses(); }
	constexpr uint64 PutHits() const { return PutRecord.Hits + PutValue.Hits; }
	constexpr uint64 PutMisses() const { return PutRecord.Misses + PutValue.Misses; }

	constexpr uint64 Gets() const { return GetHits() + GetMisses(); }
	constexpr uint64 GetHits() const { return GetRecord.Hits + GetValue.Hits + GetRecordChunk.Hits + GetValueChunk.Hits; }
	constexpr uint64 GetMisses() const { return GetRecord.Misses + GetValue.Misses + GetRecordChunk.Misses + GetValueChunk.Misses; }

	constexpr uint64 Total() const { return Puts() + Gets(); }
	constexpr uint64 TotalHits() const { return PutHits() + GetHits(); }
	constexpr uint64 TotalMisses() const { return PutMisses() + GetMisses(); }

private:
	FCounter PutRecord;
	FCounter PutValue;
	FCounter GetRecord;
	FCounter GetValue;
	FCounter GetRecordChunk;
	FCounter GetValueChunk;
};

class FTimeAveragedStat
{
public:
	void SetPeriod(FMonotonicTimeSpan NewPeriod) { Period = NewPeriod; }

	void Add(FMonotonicTimePoint StartTime, FMonotonicTimePoint EndTime, double Value);

	/** Calculate and return the average rate at the time. Time must be monotonically increasing. */
	double GetRate(FMonotonicTimePoint Time);

	/** Calculate and return the average value at the time. Time must be monotonically increasing. */
	double GetValue(FMonotonicTimePoint Time);

private:
	void Update(FMonotonicTimePoint Time);

	struct FRange
	{
		FMonotonicTimePoint StartTime;
		FMonotonicTimePoint EndTime;
	};

	struct FValue
	{
		FMonotonicTimePoint Time;
		double Value = 0.0;
	};

	/** Period of time that this stat is averaged over. */
	FMonotonicTimeSpan Period;
	/** Time at which the most recent update occurred. */
	FMonotonicTimePoint LastTime;
	/** Ranges of time that have values. */
	TArray<FRange> ActiveRanges;
	/** Values that start later than the most recent update time. */
	TArray<FValue> StartValues;
	/** Values that end later than the most recent update time. */
	TArray<FValue> EndValues;
	/** Average rate from the last update that had values, or 0. */
	double AverageRate = 0.0;
	/** Average value from the last update that had values, or 0. */
	double AverageValue = 0.0;
	/** Sum of values in the period up to the most recent update time. */
	double AccumulatedValue = 0.0;
	/** Number of values within the period at the most recent update time. */
	int32 AccumulatedValueCount = 0;
	/** Mutex that can be used to synchronize access to the stat from multiple threads. */
	FMutex Mutex;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FCacheBucketStats final
{
public:
	/** Name of the cache bucket. */
	FCacheBucket Bucket;
	/** Size of the raw data and metadata that was read from the cache bucket. */
	uint64 LogicalReadSize = 0;
	/** Size of the raw data and metadata that was written to the cache bucket. */
	uint64 LogicalWriteSize = 0;
	/** Size of the compressed data and anything else that was read from the cache bucket. */
	uint64 PhysicalReadSize = 0;
	/** Size of the compressed data and anything else that was written to the cache bucket. */
	uint64 PhysicalWriteSize = 0;
	/** Approximate CPU time on the main thread that was needed to satisfy requests in the cache bucket. */
	FMonotonicTimeSpan MainThreadTime;
	/** Approximate CPU time off the main thread that was needed to satisfy requests in the cache bucket. */
	FMonotonicTimeSpan OtherThreadTime;
	/** Count of requests that accessed the cache bucket. */
	FRequestCounter RequestCount;
	/** Mutex that can be used to synchronize access to the cache buckets stats from multiple threads. */
	mutable FMutex Mutex;

#if ENABLE_COOK_STATS
	FCookStats::CallStats GetStats;
	FCookStats::CallStats PutStats;
#endif
};

class FCacheStoreStats final : public ICacheStoreStats
{
public:
	FCacheStoreStats(FCacheStats& CacheStats, ECacheStoreFlags Flags, FStringView Type, FStringView Name, FStringView Path);

	FString Type;
	FString Name;
	FString Path;
	FText Status;
	ECacheStoreFlags Flags = ECacheStoreFlags::None;
	ECacheStoreStatusCode StatusCode = ECacheStoreStatusCode::None;
	mutable FMutex Mutex;

	TMap<FString, FString> Attributes;

	FCacheStats& CacheStats;

	/** Size of the raw data and metadata that was read from the cache store. */
	uint64 LogicalReadSize = 0;
	/** Size of the raw data and metadata that was written to the cache store. */
	uint64 LogicalWriteSize = 0;
	/** Size of the compressed data and anything else that was read from the cache store. */
	uint64 PhysicalReadSize = 0;
	/** Size of the compressed data and anything else that was written to the cache store. */
	uint64 PhysicalWriteSize = 0;
	/** Approximate CPU time on the main thread that was needed. */
	FMonotonicTimeSpan MainThreadTime;
	/** Approximate CPU time off the main thread that was needed. */
	FMonotonicTimeSpan OtherThreadTime;
	/** Count of requests that accessed the cache store. */
	FRequestCounter RequestCount;
	/** Total size of the data in the cache store, ~0ull indicate that the information is not available */
	uint64 TotalPhysicalSize = ~0ull;

	FTimeAveragedStat AverageLatency;
	FTimeAveragedStat AveragePhysicalReadSize;
	FTimeAveragedStat AveragePhysicalWriteSize;

#if ENABLE_COOK_STATS
	FCookStats::CallStats GetStats;
	FCookStats::CallStats PutStats;
#endif

private:
	FStringView GetType() const final { return Type; }
	FStringView GetName() const final { return Name; }
	FStringView GetPath() const final { return Path; }
	void SetFlags(ECacheStoreFlags Flags) final;
	void SetStatus(ECacheStoreStatusCode StatusCode, const FText& Status) final;
	void SetAttribute(FStringView Key, FStringView Value) final;
	void AddRequest(const FCacheStoreRequestStats& Stats) final;
	void AddLatency(FMonotonicTimePoint StartTime, FMonotonicTimePoint EndTime, FMonotonicTimeSpan Latency) final;
	double GetAverageLatency() final;
	void SetTotalPhysicalSize(uint64 TotalPhysicalSize) final;
};

class FCacheStats final
{
public:
	TArray<TUniquePtr<FCacheBucketStats>> BucketStats;
	TArray<TUniquePtr<FCacheStoreStats>> StoreStats;
	mutable FMutex Mutex;

	FCacheBucketStats& GetBucket(FCacheBucket Bucket);
};

} // UE::DerivedData
