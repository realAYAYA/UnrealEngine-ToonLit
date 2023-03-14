// Copyright Epic Games, Inc. All Rights Reserved.

#include "MemoryCacheStore.h"

#include "Algo/Accumulate.h"
#include "Algo/AllOf.h"
#include "Algo/BinarySearch.h"
#include "Algo/NoneOf.h"
#include "DerivedDataBackendInterface.h"
#include "DerivedDataCachePrivate.h"
#include "DerivedDataCacheRecord.h"
#include "DerivedDataCacheUsageStats.h"
#include "DerivedDataValue.h"
#include "HAL/FileManager.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeRWLock.h"
#include "ProfilingDebugging/CookStats.h"
#include "Serialization/CompactBinary.h"
#include "Templates/UniquePtr.h"

namespace UE::DerivedData
{

/**
 * A simple thread safe, memory based backend. This is used for Async puts and the boot cache.
 */
class FMemoryCacheStore final : public IMemoryCacheStore
{
public:
	explicit FMemoryCacheStore(const TCHAR* InName, int64 InMaxCacheSize = -1, bool bCanBeDisabled = false);
	~FMemoryCacheStore() final;

	// ICacheStore Interface

	void Put(
		TConstArrayView<FCachePutRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutComplete&& OnComplete) final;
	void Get(
		TConstArrayView<FCacheGetRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetComplete&& OnComplete) final;
	void PutValue(
		TConstArrayView<FCachePutValueRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutValueComplete&& OnComplete) final;
	void GetValue(
		TConstArrayView<FCacheGetValueRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetValueComplete&& OnComplete) final;
	void GetChunks(
		TConstArrayView<FCacheGetChunkRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetChunkComplete&& OnComplete) final;

	// ILegacyCacheStore Interface

	void LegacyStats(FDerivedDataCacheStatsNode& OutNode) final;
	bool LegacyDebugOptions(FBackendDebugOptions& Options) final;

	// IMemoryCacheStore Interface

	void Delete(const FCacheKey& Key) final;
	void DeleteValue(const FCacheKey& Key) final;

	void Disable() final;

private:
	FDerivedDataCacheUsageStats UsageStats;

	struct FCacheRecordComponents
	{
		FCbObject Meta;
		TArray<FValueWithId> Values;
	};

	/** Name of this cache (used for debugging) */
	FString Name;

	/** Set of records in this cache. */
	TMap<FCacheKey, FCacheRecordComponents> CacheRecords;
	/** Set of values in this cache. */
	TMap<FCacheKey, FValue> CacheValues;
	/** Maximum size the cached items can grow up to (in bytes) */
	uint64 MaxCacheSize;
	/** When set to true, this cache is disabled...ignore all requests. */
	std::atomic<bool> bDisabled;
	/** Object used for synchronization via a scoped lock						*/
	mutable FRWLock SynchronizationObject;
	/** Current estimated cache size in bytes */
	uint64 CurrentCacheSize;
	/** Indicates that the cache max size has been exceeded. This is used to avoid
		warning spam after the size has reached the limit. */
	bool bMaxSizeExceeded;

	/**
	 * When a memory cache can be disabled, it won't return true for CachedDataProbablyExists calls.
	 * This is to avoid having the Boot DDC tells it has some resources that will suddenly disappear after the boot.
	 * Get() requests will still get fulfilled and other cache level will be properly back-filled 
	 * offering the speed benefit of the boot cache while maintaining coherency at all cache levels.
	 * 
	 * The problem is that most asset types (audio/staticmesh/texture) will always verify if their different LODS/Chunks can be found in the cache using CachedDataProbablyExists.
	 * If any of the LOD/MIP can't be found, a build of the asset is triggered, otherwise they skip asset compilation altogether.
	 * However, we should not skip the compilation based on the CachedDataProbablyExists result of the boot cache because it is a lie and will disappear at some point.
	 * When the boot cache disappears and the streamer tries to fetch a LOD that it has been told was cached, it will fail and will then have no choice but to rebuild the asset synchronously.
	 * This obviously causes heavy game-thread stutters.

	 * However, if the bootcache returns false during CachedDataProbablyExists. The async compilation will be triggered and data will be put in the both the boot.ddc and the local cache.
	 * This way, no more heavy game-thread stutters during streaming...

	 * This can be reproed when you clear the local cache but do not clear the boot.ddc file, but even if it's a corner case, I stumbled upon it enough times that I though it was worth to fix so the caches are coherent.
	 */
	bool bCanBeDisabled = false;
	bool bShuttingDown  = false;

protected:
	FBackendDebugOptions DebugOptions;
};

FMemoryCacheStore::FMemoryCacheStore(const TCHAR* InName, int64 InMaxCacheSize, bool bInCanBeDisabled)
	: Name(InName)
	, MaxCacheSize(InMaxCacheSize < 0 ? 0 : uint64(InMaxCacheSize))
	, bDisabled(false)
	, CurrentCacheSize(0)
	, bMaxSizeExceeded(false)
	, bCanBeDisabled(bInCanBeDisabled)
{
}

FMemoryCacheStore::~FMemoryCacheStore()
{
	bShuttingDown = true;
	Disable();
}

void FMemoryCacheStore::Disable()
{
	check(bCanBeDisabled || bShuttingDown);
	FWriteScopeLock ScopeLock(SynchronizationObject);
	bDisabled = true;
	CacheRecords.Empty();
	CacheValues.Empty();
	CurrentCacheSize = 0;
}

void FMemoryCacheStore::LegacyStats(FDerivedDataCacheStatsNode& OutNode)
{
	OutNode = {!bCanBeDisabled ? TEXT("Memory") : TEXT("Boot"), TEXT(""), /*bIsLocal*/ true};
	OutNode.UsageStats.Add(TEXT(""), UsageStats);
}

bool FMemoryCacheStore::LegacyDebugOptions(FBackendDebugOptions& InOptions)
{
	DebugOptions = InOptions;
	return true;
}

void FMemoryCacheStore::Put(
	const TConstArrayView<FCachePutRequest> Requests,
	IRequestOwner& Owner,
	FOnCachePutComplete&& OnComplete)
{
	if (bDisabled)
	{
		return CompleteWithStatus(Requests, OnComplete, EStatus::Error);
	}

	for (const FCachePutRequest& Request : Requests)
	{
		EStatus Status = EStatus::Error;
		ON_SCOPE_EXIT
		{
			OnComplete(Request.MakeResponse(Status));
		};

		const FCacheRecord& Record = Request.Record;
		const FCacheKey& Key = Record.GetKey();
		const TConstArrayView<FValueWithId> Values = Record.GetValues();

		if (Algo::NoneOf(Values, &FValue::HasData))
		{
			continue;
		}

		if (bCanBeDisabled && DebugOptions.ShouldSimulatePutMiss(Key))
		{
			UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for put of %s from '%s'"),
				*Name, *WriteToString<96>(Key), *Request.Name);
			continue;
		}

		COOK_STAT(auto Timer = UsageStats.TimePut());
		const bool bReplaceExisting = !EnumHasAnyFlags(Request.Policy.GetRecordPolicy(), ECachePolicy::QueryLocal);

		FWriteScopeLock ScopeLock(SynchronizationObject);
		FCacheRecordComponents& Components = CacheRecords.FindOrAdd(Key);
		const bool bHasExisting = !Components.Values.IsEmpty();
		Status = EStatus::Ok;

		if (bHasExisting && !bReplaceExisting && Algo::AllOf(Components.Values, &FValue::HasData))
		{
			continue;
		}

		int64 RequiredSize = 0;
		if (bHasExisting && !bReplaceExisting)
		{
			if (!Components.Meta && Record.GetMeta())
			{
				RequiredSize += Record.GetMeta().GetSize();
			}
			for (const FValueWithId& Value : Components.Values)
			{
				if (!Value.HasData())
				{
					if (const FValueWithId& NewValue = Record.GetValue(Value.GetId());
						NewValue &&
						NewValue.HasData() &&
						NewValue.GetRawHash() == Value.GetRawHash())
					{
						RequiredSize += NewValue.GetData().GetCompressedSize();
					}
					else if (!EnumHasAnyFlags(Request.Policy.GetRecordPolicy(), ECachePolicy::PartialRecord) &&
						EnumHasAnyFlags(Request.Policy.GetValuePolicy(Value.GetId()), ECachePolicy::StoreLocal))
					{
						Status = EStatus::Error;
					}
				}
			}
		}
		else
		{
			RequiredSize = Record.GetMeta().GetSize() - Components.Meta.GetSize();
			RequiredSize -= Algo::TransformAccumulate(Components.Values, [](const FValue& Value) { return Value.GetData().GetCompressedSize(); }, uint64(0));
			for (const FValueWithId& NewValue : Record.GetValues())
			{
				if (NewValue.HasData())
				{
					RequiredSize += NewValue.GetData().GetCompressedSize();
				}
				else if (const int32 ValueIndex = Algo::BinarySearchBy(Components.Values, NewValue.GetId(), &FValueWithId::GetId);
					ValueIndex != INDEX_NONE &&
					Components.Values[ValueIndex].HasData() &&
					Components.Values[ValueIndex].GetRawHash() == NewValue.GetRawHash())
				{
					RequiredSize += Components.Values[ValueIndex].GetData().GetCompressedSize();
				}
				else if (!EnumHasAnyFlags(Request.Policy.GetRecordPolicy(), ECachePolicy::PartialRecord) &&
					EnumHasAnyFlags(Request.Policy.GetValuePolicy(NewValue.GetId()), ECachePolicy::StoreLocal))
				{
					Status = EStatus::Error;
				}
			}
		}

		if (Status == EStatus::Error)
		{
			continue;
		}

		if (MaxCacheSize > 0 && (CurrentCacheSize + RequiredSize) > MaxCacheSize)
		{
			UE_CLOG(!bMaxSizeExceeded, LogDerivedDataCache, Display,
				TEXT("Failed to cache data. Maximum cache size reached. "
				     "CurrentSize %" UINT64_FMT " KiB / MaxSize: %" UINT64_FMT " KiB"),
				CurrentCacheSize / 1024, MaxCacheSize / 1024);
			bMaxSizeExceeded = true;
			Status = EStatus::Ok;
			continue;
		}

		Status = EStatus::Ok;
		CurrentCacheSize += RequiredSize;

		if (!bHasExisting)
		{
			Components.Meta = Record.GetMeta();
			Components.Values = Record.GetValues();
		}
		else if (!bReplaceExisting)
		{
			if (!Components.Meta)
			{
				Components.Meta = Record.GetMeta();
			}
			for (FValueWithId& Value : Components.Values)
			{
				if (!Value.HasData())
				{
					if (const FValueWithId& NewValue = Record.GetValue(Value.GetId()); NewValue && NewValue.GetRawHash() == Value.GetRawHash())
					{
						Value = NewValue;
					}
				}
			}
		}
		else
		{
			FCacheRecordComponents ExistingComponents = MoveTemp(Components);
			Components.Meta = Record.GetMeta();
			Components.Values = Record.GetValues();
			for (FValueWithId& Value : Components.Values)
			{
				if (!Value.HasData())
				{
					if (const int32 ExistingValueIndex = Algo::BinarySearchBy(ExistingComponents.Values, Value.GetId(), &FValueWithId::GetId);
						ExistingValueIndex != INDEX_NONE &&
						ExistingComponents.Values[ExistingValueIndex].HasData() &&
						ExistingComponents.Values[ExistingValueIndex].GetRawHash() == Value.GetRawHash())
					{
						Value = ExistingComponents.Values[ExistingValueIndex];
					}
				}
			}
		}

		COOK_STAT(Timer.AddHit(RequiredSize));
	}
}

void FMemoryCacheStore::Get(
	const TConstArrayView<FCacheGetRequest> Requests,
	IRequestOwner& Owner,
	FOnCacheGetComplete&& OnComplete)
{
	if (bDisabled)
	{
		return CompleteWithStatus(Requests, OnComplete, EStatus::Error);
	}

	for (const FCacheGetRequest& Request : Requests)
	{
		const FCacheKey& Key = Request.Key;
		const FCacheRecordPolicy& Policy = Request.Policy;
		COOK_STAT(auto Timer = EnumHasAnyFlags(Policy.GetRecordPolicy(), ECachePolicy::SkipData) ? UsageStats.TimeProbablyExists() : UsageStats.TimeGet());
		FCacheRecordComponents Components;
		EStatus Status = EStatus::Error;

		if (bCanBeDisabled && DebugOptions.ShouldSimulateGetMiss(Key))
		{
			UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for get of %s from '%s'"),
				*Name, *WriteToString<96>(Key), *Request.Name);
		}
		else if (FReadScopeLock ScopeLock(SynchronizationObject); const FCacheRecordComponents* FoundComponents = CacheRecords.Find(Key))
		{
			if (!FoundComponents->Values.IsEmpty())
			{
				Status = EStatus::Ok;
				Components = *FoundComponents;
			}
		}

		FCacheRecordBuilder Builder(Request.Key);

		if (Status == EStatus::Ok)
		{
			const ECachePolicy RecordPolicy = Policy.GetRecordPolicy();

			if (!EnumHasAnyFlags(RecordPolicy, ECachePolicy::SkipMeta))
			{
				Builder.SetMeta(CopyTemp(Components.Meta));
			}

			for (const FValueWithId& Value : Components.Values)
			{
				const ECachePolicy ValuePolicy = Policy.GetValuePolicy(Value.GetId());
				if (!EnumHasAnyFlags(ValuePolicy, ECachePolicy::QueryLocal))
				{
					Builder.AddValue(Value.RemoveData());
					continue;
				}
				if (!Value.HasData())
				{
					Status = EStatus::Error;
					if (!EnumHasAnyFlags(RecordPolicy, ECachePolicy::PartialRecord))
					{
						Builder = FCacheRecordBuilder(Request.Key);
						break;
					}
				}
				const bool bExistsOnly = EnumHasAnyFlags(ValuePolicy, ECachePolicy::SkipData);
				Builder.AddValue(bExistsOnly ? Value.RemoveData() : Value);
			}
		}

		FCacheRecord Record = Builder.Build();
		COOK_STAT(Timer.AddHitOrMiss(
			Status == EStatus::Ok ? FCookStats::CallStats::EHitOrMiss::Hit : FCookStats::CallStats::EHitOrMiss::Miss,
			Private::GetCacheRecordCompressedSize(Record)));
		OnComplete({Request.Name, MoveTemp(Record), Request.UserData, Status});
	}
}

void FMemoryCacheStore::Delete(const FCacheKey& Key)
{
	FWriteScopeLock ScopeLock(SynchronizationObject);
	FCacheRecordComponents Components;
	if (CacheRecords.RemoveAndCopyValue(Key, Components))
	{
		CurrentCacheSize -= Components.Meta.GetSize() +
			Algo::TransformAccumulate(Components.Values, [](const FValue& Value) { return Value.GetData().GetCompressedSize(); }, uint64(0));
		bMaxSizeExceeded = false;
	}
}

void FMemoryCacheStore::PutValue(
	const TConstArrayView<FCachePutValueRequest> Requests,
	IRequestOwner& Owner,
	FOnCachePutValueComplete&& OnComplete)
{
	if (bDisabled)
	{
		return CompleteWithStatus(Requests, OnComplete, EStatus::Error);
	}

	for (const FCachePutValueRequest& Request : Requests)
	{
		EStatus Status = EStatus::Error;
		ON_SCOPE_EXIT
		{
			OnComplete(Request.MakeResponse(Status));
		};

		const FCacheKey& Key = Request.Key;
		const FValue& Value = Request.Value;

		if (!Value.HasData())
		{
			continue;
		}

		if (bCanBeDisabled && DebugOptions.ShouldSimulatePutMiss(Key))
		{
			UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for put of %s from '%s'"),
				*Name, *WriteToString<96>(Key), *Request.Name);
			continue;
		}

		COOK_STAT(auto Timer = UsageStats.TimePut());
		const int64 ValueSize = Value.GetData().GetCompressedSize();
		const bool bReplaceExisting = !EnumHasAnyFlags(Request.Policy, ECachePolicy::QueryLocal);

		FWriteScopeLock ScopeLock(SynchronizationObject);
		FValue* const ExistingValue = CacheValues.Find(Key);

		if (ExistingValue && !bReplaceExisting)
		{
			Status = EStatus::Ok;
			continue;
		}

		const int64 ExistingValueSize = ExistingValue ? ExistingValue->GetData().GetCompressedSize() : 0;
		const int64 RequiredSize = ValueSize - ExistingValueSize;

		if (MaxCacheSize > 0 && (CurrentCacheSize + RequiredSize) > MaxCacheSize)
		{
			UE_CLOG(!bMaxSizeExceeded, LogDerivedDataCache, Display,
				TEXT("Failed to cache data. Maximum cache size reached. "
				     "CurrentSize %" UINT64_FMT " KiB / MaxSize: %" UINT64_FMT " KiB"),
				CurrentCacheSize / 1024, MaxCacheSize / 1024);
			bMaxSizeExceeded = true;
			continue;
		}

		CurrentCacheSize += RequiredSize;
		if (ExistingValue)
		{
			*ExistingValue = Value;
		}
		else
		{
			CacheValues.Add(Key, Value);
		}

		COOK_STAT(Timer.AddHit(ValueSize));
		Status = EStatus::Ok;
	}
}

void FMemoryCacheStore::GetValue(
	const TConstArrayView<FCacheGetValueRequest> Requests,
	IRequestOwner& Owner,
	FOnCacheGetValueComplete&& OnComplete)
{
	if (bDisabled)
	{
		return CompleteWithStatus(Requests, OnComplete, EStatus::Error);
	}

	for (const FCacheGetValueRequest& Request : Requests)
	{
		const FCacheKey& Key = Request.Key;
		const bool bExistsOnly = EnumHasAllFlags(Request.Policy, ECachePolicy::SkipData);
		COOK_STAT(auto Timer = bExistsOnly ? UsageStats.TimeProbablyExists() : UsageStats.TimeGet());

		FValue Value;
		EStatus Status = EStatus::Error;
		if (bCanBeDisabled && DebugOptions.ShouldSimulateGetMiss(Key))
		{
			UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for get of %s from '%s'"),
				*Name, *WriteToString<96>(Key), *Request.Name);
		}
		else if (bExistsOnly && bCanBeDisabled)
		{
		}
		else if (FReadScopeLock ScopeLock(SynchronizationObject); const FValue* CacheValue = CacheValues.Find(Key))
		{
			Status = EStatus::Ok;
			Value = !bExistsOnly ? *CacheValue : CacheValue->RemoveData();
			COOK_STAT(Timer.AddHit(Value.GetData().GetCompressedSize()));
		}

		OnComplete({Request.Name, Request.Key, MoveTemp(Value), Request.UserData, Status});
	}
}

void FMemoryCacheStore::DeleteValue(const FCacheKey& Key)
{
	FWriteScopeLock ScopeLock(SynchronizationObject);
	FValue Value;
	if (CacheValues.RemoveAndCopyValue(Key, Value))
	{
		CurrentCacheSize -= Value.GetData().GetCompressedSize();
		bMaxSizeExceeded = false;
	}
}

void FMemoryCacheStore::GetChunks(
	const TConstArrayView<FCacheGetChunkRequest> Requests,
	IRequestOwner& Owner,
	FOnCacheGetChunkComplete&& OnComplete)
{
	if (bDisabled)
	{
		return CompleteWithStatus(Requests, OnComplete, EStatus::Error);
	}

	bool bHasValue = false;
	FValue Value;
	FValueId ValueId;
	FCacheKey ValueKey;
	FCompressedBufferReader Reader;
	for (const FCacheGetChunkRequest& Request : Requests)
	{
		bool bProcessHit = false;
		const bool bExistsOnly = EnumHasAnyFlags(Request.Policy, ECachePolicy::SkipData);
		COOK_STAT(auto Timer = bExistsOnly ? UsageStats.TimeProbablyExists() : UsageStats.TimeGet());
		if (bCanBeDisabled && DebugOptions.ShouldSimulateGetMiss(Request.Key))
		{
			UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for get of %s from '%s'"),
				*Name, *WriteToString<96>(Request.Key, '/', Request.Id), *Request.Name);
		}
		else if (bHasValue && (ValueKey == Request.Key) && (ValueId == Request.Id) && (bExistsOnly || Reader.HasSource()))
		{
			// Value matches the request.
			bProcessHit = true;
		}
		else
		{
			FReadScopeLock ScopeLock(SynchronizationObject);
			if (Request.Id.IsValid())
			{
				if (const FCacheRecordComponents* Components = CacheRecords.Find(Request.Key))
				{
					if (const int32 ValueIndex = Algo::BinarySearchBy(Components->Values, Request.Id, &FValueWithId::GetId); ValueIndex != INDEX_NONE)
					{
						const FValueWithId& ValueWithId = Components->Values[ValueIndex];
						bHasValue = ValueWithId.IsValid();
						Reader.ResetSource();
						Value.Reset();
						Value = ValueWithId;
						ValueId = Request.Id;
						ValueKey = Request.Key;
						Reader.SetSource(Value.GetData());
						bProcessHit = true;
					}
				}
			}
			else
			{
				if (const FValue* ExistingValue = CacheValues.Find(Request.Key))
				{
					bHasValue = true;
					Reader.ResetSource();
					Value.Reset();
					Value = *ExistingValue;
					ValueId.Reset();
					ValueKey = Request.Key;
					Reader.SetSource(Value.GetData());
					bProcessHit = true;
				}
			}
		}

		if (bProcessHit && Request.RawOffset <= Value.GetRawSize())
		{
			const uint64 RawSize = FMath::Min(Value.GetRawSize() - Request.RawOffset, Request.RawSize);
			COOK_STAT(Timer.AddHit(RawSize));
			FSharedBuffer Buffer;
			if (Value.HasData() && !bExistsOnly)
			{
				Buffer = Reader.Decompress(Request.RawOffset, RawSize);
			}
			const EStatus Status = bExistsOnly || Buffer ? EStatus::Ok : EStatus::Error;
			OnComplete({Request.Name, Request.Key, Request.Id, Request.RawOffset,
				RawSize, Value.GetRawHash(), MoveTemp(Buffer), Request.UserData, Status});
		}
		else
		{
			OnComplete(Request.MakeResponse(EStatus::Error));
		}
	}
}

IMemoryCacheStore* CreateMemoryCacheStore(const TCHAR* Name, int64 MaxCacheSize, bool bCanBeDisabled)
{
	return new FMemoryCacheStore(Name, MaxCacheSize, bCanBeDisabled);
}

} // UE::DerivedData
