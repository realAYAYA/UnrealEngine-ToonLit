// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/PCGGraphCache.h"

#include "PCGComponent.h"
#include "PCGModule.h"

#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeRWLock.h"

static TAutoConsoleVariable<bool> CVarCacheDebugging(
	TEXT("pcg.Cache.EnableDebugging"),
	false,
	TEXT("Enable various features for debugging the graph cache system."));

static TAutoConsoleVariable<int32> CVarCacheMemoryBudgetMB(
	TEXT("pcg.Cache.MemoryBudgetMB"),
	6144,
	TEXT("Memory budget for data in cache (MB)."));

static TAutoConsoleVariable<bool> CVarCacheMemoryBudgetEnabled(
	TEXT("pcg.Cache.EnableMemoryBudget"),
	true,
	TEXT("Whether memory budget is enforced (items purged from cache to respect pcg.Cache.MemoryBudgetMB."));

static int32 GPCGGraphCacheMaxElements = 65536;
static FAutoConsoleVariableRef CVarNiagaraGraphDataCacheSize(
	TEXT("pcg.Cache.GraphCacheMaxElements"),
	GPCGGraphCacheMaxElements,
	TEXT("Maximum number of elements to store within the graph cache."),
	ECVF_ReadOnly
);

FPCGGraphCache::FPCGGraphCache(TWeakObjectPtr<UObject> InOwner, FPCGRootSet* InRootSet)
	: CacheData(GPCGGraphCacheMaxElements)
	, Owner(InOwner)
	, RootSet(InRootSet)
{
	check(InOwner.Get() && InRootSet);
}

FPCGGraphCache::~FPCGGraphCache()
{
	ClearCache();
}

bool FPCGGraphCache::GetFromCache(const UPCGNode* InNode, const IPCGElement* InElement, const FPCGCrc& InDependenciesCrc, const FPCGDataCollection& InInput, const UPCGSettings* InSettings, const UPCGComponent* InComponent, FPCGDataCollection& OutOutput) const
{
	if (!Owner.IsValid())
	{
		return false;
	}

	if(!InDependenciesCrc.IsValid())
	{
		UE_LOG(LogPCG, Warning, TEXT("Invalid dependencies passed to FPCGGraphCache::GetFromCache(), lookup aborted."));
		return false;
	}

	const bool bDebuggingEnabled = IsDebuggingEnabled() && InComponent && InComponent->GetOwner() && InNode;

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCache::GetFromCache);
		FReadScopeLock ScopedReadLock(CacheLock);

		FPCGCacheEntryKey CacheKey(InElement, InDependenciesCrc);
		if (const FPCGDataCollection* Value = const_cast<FPCGGraphCache*>(this)->CacheData.FindAndTouch(CacheKey))
		{
			if (bDebuggingEnabled)
			{
				// Leading spaces to align log content with warnings below - helps readability a lot.
				UE_LOG(LogPCG, Log, TEXT("         [%s] %s\t\tCACHE HIT %u"), *InComponent->GetOwner()->GetName(), *InNode->GetNodeTitle().ToString(), InDependenciesCrc.GetValue());
			}

			OutOutput = *Value;

			return true;
		}
		else
		{
			if (bDebuggingEnabled)
			{
				UE_LOG(LogPCG, Warning, TEXT("[%s] %s\t\tCACHE MISS %u"), *InComponent->GetOwner()->GetName(), *InNode->GetNodeTitle().ToString(), InDependenciesCrc.GetValue());
			}

			return false;
		}
	}
}

void FPCGGraphCache::StoreInCache(const IPCGElement* InElement, const FPCGCrc& InDependenciesCrc, const FPCGDataCollection& InInput, const UPCGSettings* InSettings, const UPCGComponent* InComponent, const FPCGDataCollection& InOutput)
{
	if (!Owner.IsValid() || !ensure(InDependenciesCrc.IsValid()))
	{
		return;
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCache::StoreInCache);
		FWriteScopeLock ScopedWriteLock(CacheLock);

		FPCGCacheEntryKey CacheKey(InElement, InDependenciesCrc);
		CacheData.Add(CacheKey, InOutput);
		
		InOutput.AddToRootSet(*RootSet);

		AddDataToAccountedMemory(InOutput);
	}
}

void FPCGGraphCache::ClearCache()
{
	FWriteScopeLock ScopedWriteLock(CacheLock);

	// Unroot all previously rooted data
	for (FPCGDataCollection CacheEntry : CacheData)
	{
		CacheEntry.RemoveFromRootSet(*RootSet);
	}

	MemoryRecords.Empty();
	TotalMemoryUsed = 0;

	// Remove all entries
	CacheData.Empty(GPCGGraphCacheMaxElements);
}

bool FPCGGraphCache::EnforceMemoryBudget()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCache::FPCGGraphCache::EnforceMemoryBudget);

	if (!CVarCacheMemoryBudgetEnabled.GetValueOnAnyThread())
	{
		return false;
	}

	if (CacheData.Num() == GPCGGraphCacheMaxElements)
	{
		UE_LOG(LogPCG, Warning, TEXT("Graph cache full (%d entries). Consider increasing GPCGGraphCacheMaxElements in code."), GPCGGraphCacheMaxElements);
	}

	const uint64 MemoryBudget = static_cast<uint64>(CVarCacheMemoryBudgetMB.GetValueOnAnyThread()) * 1024 * 1024;
	if (TotalMemoryUsed <= MemoryBudget)
	{
		return false;
	}

	{
		FWriteScopeLock ScopeWriteLock(CacheLock);

		while (TotalMemoryUsed > MemoryBudget && CacheData.Num() > 0)
		{
			FPCGDataCollection RemovedData = CacheData.RemoveLeastRecent();
			RemovedData.RemoveFromRootSet(*RootSet);
			RemoveFromMemoryTotal(RemovedData);
		}
	}

	return true;
}

#if WITH_EDITOR
void FPCGGraphCache::CleanFromCache(const IPCGElement* InElement, const UPCGSettings* InSettings/*= nullptr*/)
{
	if (!InElement)
	{
		return;
	}

	if (IsDebuggingEnabled())
	{
		UE_LOG(LogPCG, Warning, TEXT("[] \t\tCACHE: PURGED [%s]"), InSettings ? *InSettings->GetDefaultNodeTitle().ToString() : TEXT("AnonymousElement"));
	}

	{
		FWriteScopeLock ScopeWriteLock(CacheLock);

		TArray<FPCGCacheEntryKey> Keys;
		CacheData.GetKeys(Keys);

		for (const FPCGCacheEntryKey& Key : Keys)
		{
			if (Key.GetElement() != InElement)
			{
				continue;
			}

			if (const FPCGDataCollection* Data = CacheData.Find(Key))
			{
				RemoveFromMemoryTotal(*Data);
				Data->RemoveFromRootSet(*RootSet);
				
				CacheData.Remove(Key);
			}
		}
	}
}

uint32 FPCGGraphCache::GetGraphCacheEntryCount(IPCGElement* InElement) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCache::GetFromCache);
	FReadScopeLock ScopedReadLock(CacheLock);

	uint32 Count = 0;

	TArray<FPCGCacheEntryKey> Keys;
	CacheData.GetKeys(Keys);

	for (const FPCGCacheEntryKey& Key : Keys)
	{
		if (Key.GetElement() == InElement)
		{
			Count++;
		}
	}

	return Count;
}
#endif // WITH_EDITOR

void FPCGGraphCache::AddDataToAccountedMemory(const FPCGDataCollection& InCollection)
{
	for (const FPCGTaggedData& Data : InCollection.TaggedData)
	{
		if (Data.Data)
		{
			Data.Data->VisitDataNetwork([this](const UPCGData* Data)
			{
				if (Data)
				{
					FResourceSizeEx ResSize = FResourceSizeEx(EResourceSizeMode::Exclusive);
					// Calculate data size. Function is non-const but is const-like, especially when
					// resource mode is Exclusive. The other mode calls a function to find all outer'd
					// objects which is non-const.
					const_cast<UPCGData*>(Data)->GetResourceSizeEx(ResSize);
					const SIZE_T DataSize = ResSize.GetDedicatedSystemMemoryBytes();

					// Find or add record
					FCachedMemoryRecord& NewRecord = MemoryRecords.FindOrAdd(Data->UID);

					// Cache the size
					NewRecord.MemoryPerInstance = DataSize;

					if (NewRecord.InstanceCount == 0)
					{
						// Account for memory if first instance
						TotalMemoryUsed += DataSize;
					}

					// Count instances
					NewRecord.InstanceCount++;
				}
			});
		}
	}
}

void FPCGGraphCache::RemoveFromMemoryTotal(const FPCGDataCollection& InCollection)
{
	for (const FPCGTaggedData& Data : InCollection.TaggedData)
	{
		if (Data.Data)
		{
			Data.Data->VisitDataNetwork([this](const UPCGData* Data)
			{
				FCachedMemoryRecord* Record = Data ? MemoryRecords.Find(Data->UID) : nullptr;
				if (ensure(Record))
				{
					// Update instance count
					if (ensure(Record->InstanceCount > 0))
					{
						--Record->InstanceCount;
					}

					if (Record->InstanceCount == 0)
					{
						// Last instance removed, update accordingly
						if (ensure(TotalMemoryUsed > Record->MemoryPerInstance))
						{
							TotalMemoryUsed -= Record->MemoryPerInstance;
						}

						MemoryRecords.Remove(Data->UID);
					}
				}
			});
		}
	}
}

bool FPCGGraphCache::IsDebuggingEnabled() const
{
	return CVarCacheDebugging.GetValueOnAnyThread();
}
