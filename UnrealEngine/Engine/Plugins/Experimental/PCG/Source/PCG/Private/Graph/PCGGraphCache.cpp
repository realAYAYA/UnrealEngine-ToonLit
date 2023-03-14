// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGGraphCache.h"
#include "PCGComponent.h"
#include "PCGSettings.h"

#include "Misc/ScopeRWLock.h"
#include "Algo/AnyOf.h"
#include "GameFramework/Actor.h"

namespace PCGGraphCache
{
	constexpr int32 NullComponentSeed = 0;
	constexpr int32 NullSettingsCrc32 = 0;

	// Component seed is only get if we have a component and either no settings or the settings use seed.
	// TODO: Perhaps we should not get the component seed if we have no settings...
	int32 GetComponentSeed(const UPCGSettings* InSettings, const UPCGComponent* InComponent)
	{
		if ((!InSettings || InSettings->UseSeed()) && InComponent)
		{
			return InComponent->Seed;
		}

		return PCGGraphCache::NullComponentSeed;
	}
}

FPCGGraphCacheEntry::FPCGGraphCacheEntry(const FPCGDataCollection& InInput, const UPCGSettings* InSettings, const UPCGComponent* InComponent, const FPCGDataCollection& InOutput, TWeakObjectPtr<UObject> InOwner, FPCGRootSet& OutRootSet)
	: Input(InInput)
	, Output(InOutput)
{
	SettingsCrc32 = InSettings ? InSettings->GetCrc32() : PCGGraphCache::NullSettingsCrc32;
	ComponentSeed = PCGGraphCache::GetComponentSeed(InSettings, InComponent);

	Input.AddToRootSet(OutRootSet);
	Output.AddToRootSet(OutRootSet);
}

bool FPCGGraphCacheEntry::Matches(const FPCGDataCollection& InInput, int32 InSettingsCrc32, int32 InComponentSeed) const
{
	return (SettingsCrc32 == InSettingsCrc32) && (Input == InInput) && (ComponentSeed == InComponentSeed);
}

FPCGGraphCache::FPCGGraphCache(TWeakObjectPtr<UObject> InOwner, FPCGRootSet* InRootSet)
	: Owner(InOwner), RootSet(InRootSet)
{
	check(InOwner.Get() && InRootSet);
}

FPCGGraphCache::~FPCGGraphCache()
{
	ClearCache();
}

bool FPCGGraphCache::GetFromCache(const IPCGElement* InElement, const FPCGDataCollection& InInput, const UPCGSettings* InSettings, const UPCGComponent* InComponent, FPCGDataCollection& OutOutput) const
{
	if (!Owner.IsValid())
	{
		return false;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCache::GetFromCache);
	FReadScopeLock ScopedReadLock(CacheLock);

	if (const FPCGGraphCacheEntries* Entries = CacheData.Find(InElement))
	{
		int32 InSettingsCrc32 = (InSettings ? InSettings->GetCrc32() : PCGGraphCache::NullSettingsCrc32);
		int32 InComponentSeed = PCGGraphCache::GetComponentSeed(InSettings, InComponent);

		for (const FPCGGraphCacheEntry& Entry : *Entries)
		{
			if (Entry.Matches(InInput, InSettingsCrc32, InComponentSeed))
			{
				OutOutput = Entry.Output;
				return true;
			}
		}

		return false;
	}

	return false;
}

void FPCGGraphCache::StoreInCache(const IPCGElement* InElement, const FPCGDataCollection& InInput, const UPCGSettings* InSettings, const UPCGComponent* InComponent, const FPCGDataCollection& InOutput)
{
	if (!Owner.IsValid())
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCache::StoreInCache);
	FWriteScopeLock ScopedWriteLock(CacheLock);

	FPCGGraphCacheEntries* Entries = CacheData.Find(InElement);
	if(!Entries)
	{
		Entries = &(CacheData.Add(InElement));
	}

	Entries->Emplace(InInput, InSettings, InComponent, InOutput, Owner, *RootSet);
}

void FPCGGraphCache::ClearCache()
{
	FWriteScopeLock ScopedWriteLock(CacheLock);

	// Unroot all previously rooted data
	for (TPair<const IPCGElement*, FPCGGraphCacheEntries>& CacheEntry : CacheData)
	{
		for (FPCGGraphCacheEntry& Entry : CacheEntry.Value)
		{
			Entry.Input.RemoveFromRootSet(*RootSet);
			Entry.Output.RemoveFromRootSet(*RootSet);
		}
	}

	// Remove all entries
	CacheData.Reset();
}

#if WITH_EDITOR
void FPCGGraphCache::CleanFromCache(const IPCGElement* InElement)
{
	if (!InElement)
	{
		return;
	}

	FWriteScopeLock ScopeWriteLock(CacheLock);
	FPCGGraphCacheEntries* Entries = CacheData.Find(InElement);
	if (Entries)
	{
		for (FPCGGraphCacheEntry& Entry : *Entries)
		{
			Entry.Input.RemoveFromRootSet(*RootSet);
			Entry.Output.RemoveFromRootSet(*RootSet);
		}
	}

	// Finally, remove all entries matching that element
	CacheData.Remove(InElement);
}
#endif // WITH_EDITOR