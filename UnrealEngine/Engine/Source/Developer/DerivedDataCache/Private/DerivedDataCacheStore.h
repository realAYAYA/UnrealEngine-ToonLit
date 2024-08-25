// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Mutex.h"
#include "Containers/ContainersFwd.h"
#include "DerivedDataCache.h"
#include "Misc/MonotonicTime.h"

class FText;
struct FDerivedDataCacheResourceStat;

namespace UE::DerivedData { class FCacheRecord; }
namespace UE::DerivedData { class FCacheStoreRequestTimer; }
namespace UE::DerivedData { class FValue; }
namespace UE::DerivedData { class ICacheStoreStats; }
namespace UE::DerivedData { class ILegacyCacheStore; }
namespace UE::DerivedData { struct FCacheStoreRequestStats; }
namespace UE::DerivedData { enum class ECacheStoreRequestOp : uint8; }
namespace UE::DerivedData { enum class ECacheStoreRequestType : uint8; }
namespace UE::DerivedData { enum class EStatus : uint8; }

namespace UE::DerivedData
{

/**
 * Interface to a store of cache records and cache values.
 *
 * Functions on this interface may be called from any thread.
 */
class ICacheStore
{
public:
	virtual ~ICacheStore() = default;

	/**
	 * Asynchronous request to put records in the cache.
	 *
	 * Behavior must match the cache record policy in each request. A few of those requirements:
	 * - Store with Query is expected *not* to overwrite an existing record.
	 * - Store without Query is permitted to overwrite an existing record.
	 * - Response must be Error if any value has unrecoverable missing data, unless the policy has PartialRecord.
	 *
	 * @param Requests     Requests with the cache records to store. Records must have a key.
	 * @param Owner        The owner to execute the request within. See IRequestOwner.
	 * @param OnComplete   A callback invoked for every request as it completes or is canceled.
	 */
	virtual void Put(
		TConstArrayView<FCachePutRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutComplete&& OnComplete) = 0;

	/**
	 * Asynchronous request to get records from the cache.
	 *
	 * Behavior must match the cache record policy in each request. A few of those requirements:
	 * - Data is required if the value policy has Query.
	 * - Data must not be in the response if the value policy has SkipData.
	 * - Response must contain values and available data if the record policy has PartialRecord.
	 * - Response must be Error if any required data is missing, even if the record policy has PartialRecord.
	 *
	 * @param Requests     Requests with the keys of the cache records to fetch.
	 * @param Owner        The owner to execute the request within. See IRequestOwner.
	 * @param OnComplete   A callback invoked for every request as it completes or is canceled.
	 */
	virtual void Get(
		TConstArrayView<FCacheGetRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetComplete&& OnComplete) = 0;

	/**
	 * Asynchronous request to put values in the cache.
	 *
	 * @param Requests     Requests with the cache values to store. Requests must have a key.
	 * @param Owner        The owner to execute the request within. See IRequestOwner.
	 * @param OnComplete   A callback invoked for every request as it completes or is canceled.
	 */
	virtual void PutValue(
		TConstArrayView<FCachePutValueRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutValueComplete&& OnComplete) = 0;

	/**
	 * Asynchronous request to get values from the cache.
	 *
	 * @param Requests     Requests with the keys of the cache values to fetch.
	 * @param Owner        The owner to execute the request within. See IRequestOwner.
	 * @param OnComplete   A callback invoked for every request as it completes or is canceled.
	 */
	virtual void GetValue(
		TConstArrayView<FCacheGetValueRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetValueComplete&& OnComplete) = 0;

	/**
	 * Asynchronous request to get chunks, which are subsets of values, from records or values.
	 *
	 * @param Requests     Requests with the key, ID, offset, and size of each chunk to fetch.
	 * @param Owner        The owner to execute the request within. See IRequestOwner.
	 * @param OnComplete   A callback invoked for every request as it completes or is canceled.
	 */
	virtual void GetChunks(
		TConstArrayView<FCacheGetChunkRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetChunkComplete&& OnComplete) = 0;

	using ERequestOp = ECacheStoreRequestOp;
	using ERequestType = ECacheStoreRequestType;
	using FRequestStats = FCacheStoreRequestStats;
	using FRequestTimer = FCacheStoreRequestTimer;
};

/**
 * The type of data accessed by a request.
 */
enum class ECacheStoreRequestType : uint8
{
	None = 0,
	Record,
	Value,
};

inline const TCHAR* LexToString(ECacheStoreRequestType CacheStoreRequestType)
{
	switch (CacheStoreRequestType)
	{
	case ECacheStoreRequestType::None:
		return TEXT("None");
	case ECacheStoreRequestType::Record:
		return TEXT("Record");
	case ECacheStoreRequestType::Value:
		return TEXT("Value");
	}

	checkNoEntry();
	return TEXT("Unknown value! (Update LexToString!)");
}

/**
 * The operation performed by the request.
 */
enum class ECacheStoreRequestOp : uint8
{
	None = 0,
	Put,
	Get,
	GetChunk,
};

inline const TCHAR* LexToString(ECacheStoreRequestOp CacheStoreRequestOp)
{
	switch (CacheStoreRequestOp)
	{
	case ECacheStoreRequestOp::None:
		return TEXT("None");
	case ECacheStoreRequestOp::Put:
		return TEXT("Put");
	case ECacheStoreRequestOp::Get:
		return TEXT("Get");
	case ECacheStoreRequestOp::GetChunk:
		return TEXT("GetChunk");
	}

	checkNoEntry();
	return TEXT("Unknown value! (Update LexToString!)");
}

enum class ECacheStoreFlags : uint32
{
	None            = 0,

	/** Accepts requests with a policy of QueryLocal or StoreLocal. Needs matching Query/Store flag. */
	Local           = 1 << 0,
	/** Accepts requests with a policy of QueryRemote or StoreRemote. Needs matching Query/Store flag.*/
	Remote          = 1 << 1,

	/** Accepts requests with a policy of QueryLocal or QueryRemote. Needs matching Local/Remote flag. */
	Query           = 1 << 2,
	/** Accepts requests with a policy of StoreLocal or StoreRemote. Needs matching Local/Remote flag. */
	Store           = 1 << 3,

	/** A put of a record or value contained by this cache store will not store to later cache stores. */
	StopPutStore    = 1 << 4,
	/** A get of a record or value contained by this cache store will not store to later cache stores. */
	StopGetStore    = 1 << 5,
	/** A record or value contained by this cache store will not store to later cache stores. */
	StopStore       = StopPutStore | StopGetStore,
};

ENUM_CLASS_FLAGS(ECacheStoreFlags);

class ICacheStoreOwner
{
public:
	virtual void Add(ILegacyCacheStore* CacheStore, ECacheStoreFlags Flags) = 0;
	virtual void SetFlags(ILegacyCacheStore* CacheStore, ECacheStoreFlags Flags) = 0;
	virtual void RemoveNotSafe(ILegacyCacheStore* CacheStore) = 0;

	/** Returns true if the combined flags of the owned cache stores contain all of these flags. */
	virtual bool HasAllFlags(ECacheStoreFlags Flags) const = 0;

	virtual ICacheStoreStats* CreateStats(ILegacyCacheStore* CacheStore, ECacheStoreFlags Flags, FStringView Type, FStringView Name, FStringView Path = {}) = 0;
	virtual void DestroyStats(ICacheStoreStats* Stats) = 0;

	virtual void LegacyResourceStats(TArray<FDerivedDataCacheResourceStat>& OutStats) const = 0;
};

class ICacheStoreGraph
{
public:
	virtual ILegacyCacheStore* FindOrCreate(const TCHAR* Name) = 0;
};

enum class ECacheStoreStatusCode : uint8
{
	None = 0,
	Warning,
	Error,
};

/**
 * Stats that represent how one cache store processed one request.
 */
struct FCacheStoreRequestStats final
{
	/** A name to identify this request for logging and profiling. An object path is typically sufficient. */
	FSharedString Name;
	/** Bucket that contains the data being accessed by this request. */
	FCacheBucket Bucket;
	/** Size of the raw data and metadata that was read from the cache store. */
	uint64 LogicalReadSize = 0;
	/** Size of the raw data and metadata that was written to the cache store. */
	uint64 LogicalWriteSize = 0;
	/** Size of the compressed data and anything else that was read from the cache store to satisfy this request. */
	uint64 PhysicalReadSize = 0;
	/** Size of the compressed data and anything else that was written to the cache store to satisfy this request. */
	uint64 PhysicalWriteSize = 0;
	/** Minimum latency to the cache store of any of the individual accesses needed to satisfy this request. */
	FMonotonicTimeSpan Latency = FMonotonicTimeSpan::Infinity();
	/** Approximate CPU time on the main thread that was needed to satisfy this request. */
	FMonotonicTimeSpan MainThreadTime;
	/** Approximate CPU time off the main thread that was needed to satisfy this request. */
	FMonotonicTimeSpan OtherThreadTime;
	/** Wall time at which this the cache store started processing of this request. */
	FMonotonicTimePoint StartTime = FMonotonicTimePoint::Infinity();
	/** Wall time at which this the cache store ended processing of this request. */
	FMonotonicTimePoint EndTime = FMonotonicTimePoint() - FMonotonicTimeSpan::Infinity();
	/** The type of data that was accessed by this request. */
	ECacheStoreRequestType Type = ECacheStoreRequestType::None;
	/** The operation that was performed by this request. */
	ECacheStoreRequestOp Op = ECacheStoreRequestOp::None;
	/** The status with which this request completed. */
	EStatus Status = EStatus::Ok;
	/** Mutex that can be used by cache stores that process a request on multiple threads. */
	mutable FMutex Mutex;

	void AddLatency(FMonotonicTimeSpan Latency);

	/** Record a logical read of the metadata and values in the record. */
	void AddLogicalRead(const FCacheRecord& Record);
	/** Record a logical read of the value. Recorded as 0 if the value has no data. */
	void AddLogicalRead(const FValue& Value);
	/** Record a logical write of the value. Recorded as 0 if the value has no data. */
	void AddLogicalWrite(const FValue& Value);
};

/** Updates StartTime, EndTime, MainThreadTime, OtherThreadTime based on its lifetime. */
class FCacheStoreRequestTimer final
{
public:
	explicit FCacheStoreRequestTimer(FCacheStoreRequestStats& Stats);
	inline ~FCacheStoreRequestTimer() { Stop(); }

	void Stop();

	FCacheStoreRequestTimer(const FCacheStoreRequestTimer&) = delete;
	FCacheStoreRequestTimer& operator=(const FCacheStoreRequestTimer&) = delete;

private:
	FCacheStoreRequestStats& Stats;
	FMonotonicTimePoint StartTime;
};

/**
 * Interface to report the status of a cache store and stats for each request that it processes.
 */
class ICacheStoreStats
{
public:
	virtual ~ICacheStoreStats() = default;

	/** Returns the type name of the associated cache store. */
	virtual FStringView GetType() const = 0;
	/** Returns the name of the associated cache store in the hierarchy. */
	virtual FStringView GetName() const = 0;
	/** Returns the path to the associated cache store where applicable. */
	virtual FStringView GetPath() const = 0;

	/** Sets the flags detailing how the associated cache store operates. */
	virtual void SetFlags(ECacheStoreFlags Flags) = 0;
	/** Sets the status of the associated cache store. Persists until the next call. */
	virtual void SetStatus(ECacheStoreStatusCode StatusCode, const FText& Status) = 0;
	/** Sets a named attribute on the associated cache store. Persists until the next call. */
	virtual void SetAttribute(FStringView Key, FStringView Value) = 0;

	/** Adds stats for a single request that was processed by the associated cache store. */
	virtual void AddRequest(const FCacheStoreRequestStats& Stats) = 0;

	/** Adds stats for latency for a measurement that was done for the associated cache store. */
	virtual void AddLatency(FMonotonicTimePoint StartTime, FMonotonicTimePoint EndTime, FMonotonicTimeSpan Latency) = 0;

	/** Gets the average latency value for the current time in seconds. */
	virtual double GetAverageLatency() = 0;

	virtual void SetTotalPhysicalSize(uint64 TotalPhysicalSize) = 0;
};

template <typename RequestRangeType, typename OnCompleteType>
inline void CompleteWithStatus(RequestRangeType&& Requests, OnCompleteType&& OnComplete, EStatus Status)
{
	for (const auto& Request : Requests)
	{
		OnComplete(Request.MakeResponse(Status));
	}
}

} // UE::DerivedData
