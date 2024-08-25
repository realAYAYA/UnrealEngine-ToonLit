// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCrc.h"
#include "PCGData.h"

#include "Containers/LruCache.h"
#include "UObject/GCObject.h"

class IPCGElement;
class UPCGComponent;
class UPCGNode;
class UPCGSettings;

/** A record kept each data object instance to count how many instances are tracked. */
struct FCachedMemoryRecord
{
	FCachedMemoryRecord() = default;

	/** Number of instances that are contributing to used memory. */
	uint32 InstanceCount = 0;

	/** Memory usage in bytes of one instance. Cached here to save recomputing later. */
	SIZE_T MemoryPerInstance = 0;
};

struct FPCGCacheEntryKey
{
	FPCGCacheEntryKey(const IPCGElement* InElement, const FPCGCrc& InCrc)
		: Element(InElement)
		, Crc(InCrc)
	{
		check(Element);
		ensure(Crc.IsValid());

		Hash = GetTypeHash(Element);
		Hash = HashCombine(Hash, Crc.GetValue());
	}

	bool operator==(const FPCGCacheEntryKey& Other) const
	{
		return Element == Other.Element && Crc == Other.Crc;
	}

	friend uint32 GetTypeHash(const FPCGCacheEntryKey& Key)
	{
		return Key.Hash;
	}

	const IPCGElement* GetElement() const { return Element; }

private:
	const IPCGElement* Element = nullptr;
	FPCGCrc Crc;

	uint32 Hash = 0;
};

/** Core idea is to store cache entries per node, but that will be less efficient
* In cases where we have some subgraph reuse. Under that premise, we can then
* instead store by element, as we will never recreate elements (except for arbitrary tasks)
*/
struct FPCGGraphCache : public FGCObject
{
	FPCGGraphCache();
	~FPCGGraphCache();

	/** Returns true if data was found from the cache, in which case the outputs are written in OutOutput. InNode is optional and for logging only. */
	bool GetFromCache(const UPCGNode* InNode, const IPCGElement* InElement, const FPCGCrc& InCrc, const UPCGComponent* InComponent, FPCGDataCollection& OutOutput) const;

	/** Stores data in the cache for later use */
	void StoreInCache(const IPCGElement* InElement, const FPCGCrc& InCrc, const FPCGDataCollection& InOutput);

	/** Removes all entries from the cache, unroots data, etc. */
	void ClearCache();

	/** While memory usage is more than budget, remove cache entries for elements, LRU policy. Returns true if something removed. */
	bool EnforceMemoryBudget();

	/** True if debugging features enabled. Exposes a CVar so it can be called from editor module. */
	bool IsDebuggingEnabled() const;

#if WITH_EDITOR
	/** Clears any cache entry for the given data. InSettings is optional and for logging only. */
	void CleanFromCache(const IPCGElement* InElement, const UPCGSettings* InSettings = nullptr);

	/** Returns number of copies of data cached for InElement. */
	uint32 GetGraphCacheEntryCount(IPCGElement* InElement) const;
#endif

	//~Begin FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FPCGGraphCache"); }
	//~End FGCObject interface

private:
	// Grow cache entry capacity, preserving current entries.
	void GrowCache_Unsafe();

	/** For each data element in collection, keep records of memory usage and instance count. */
	void AddDataToAccountedMemory(const FPCGDataCollection& InCollection);

	/** For each data element in collection, update records to reflect removal and update total memory figure if all instances removed. */
	void RemoveFromMemoryTotal(const FPCGDataCollection& InCollection);

	// Note: we are not going to serialize this as-is, since the pointers will change
	// We will have to serialize on a node id basis most likely
	TLruCache<FPCGCacheEntryKey, FPCGDataCollection> CacheData;

	/** Map from data UIDs to records. Provides ref counting and caches memory size. */
	TMap<uint64, FCachedMemoryRecord> MemoryRecords;

	/** Total memory usage by all data objects in cache. */
	uint64 TotalMemoryUsed = 0;

	mutable FRWLock CacheLock;
};
