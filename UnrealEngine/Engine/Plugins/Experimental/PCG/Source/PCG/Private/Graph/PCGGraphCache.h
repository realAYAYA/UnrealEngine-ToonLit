// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGData.h"
#include "PCGElement.h"

class UPCGSettings;
class UPCGComponent;
class AActor;

struct FPCGGraphCacheEntry
{
	FPCGGraphCacheEntry() = default;
	FPCGGraphCacheEntry(const FPCGDataCollection& InInput, const UPCGSettings* InSettings, const UPCGComponent* InComponent, const FPCGDataCollection& InOutput, TWeakObjectPtr<UObject> InOwner, FPCGRootSet& OutRootSet);

	bool Matches(const FPCGDataCollection& InInput, int32 InSettingsCrc32, int32 InComponentSeed) const;

	FPCGDataCollection Input;
	FPCGDataCollection Output;
	int32 SettingsCrc32;
	int32 ComponentSeed;
};

// TODO: investigate if we need a more evolved data structure here
// since we could want to have a lock per entries structure
typedef TArray<FPCGGraphCacheEntry> FPCGGraphCacheEntries;

/** Core idea is to store cache entries per node, but that will be less efficient
* In cases where we have some subgraph reuse. Under that premise, we can then
* instead store by element, as we will never recreate elements (except for arbitrary tasks)
*/
struct FPCGGraphCache
{
	FPCGGraphCache(TWeakObjectPtr<UObject> InOwner, FPCGRootSet* InRootSet);
	~FPCGGraphCache();

	/** Returns true if data was found from the cache, in which case the outputs are written in OutOutput */
	bool GetFromCache(const IPCGElement* InElement, const FPCGDataCollection& InInput, const UPCGSettings* InSettings, const UPCGComponent* InComponent, FPCGDataCollection& OutOutput) const;

	/** Stores data in the cache for later use */
	void StoreInCache(const IPCGElement* InElement, const FPCGDataCollection& InInput, const UPCGSettings* InSettings, const UPCGComponent* InComponent, const FPCGDataCollection& InOutput);

	/** Removes all entries from the cache, unroots data, etc. */
	void ClearCache();

#if WITH_EDITOR
	/** Clears any cache entry for the given data */
	void CleanFromCache(const IPCGElement* InElement);
#endif

private:
	// Note: we are not going to serialize this as-is, since the pointers will change
	// We will have to serialize on a node id basis most likely
	TMap<const IPCGElement*, FPCGGraphCacheEntries> CacheData;
	TWeakObjectPtr<UObject> Owner = nullptr;

	/** To prevent garbage collection on data in the cache, we'll need to root some data */
	FPCGRootSet* RootSet = nullptr;

	mutable FRWLock CacheLock;
};