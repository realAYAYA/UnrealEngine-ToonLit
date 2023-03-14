// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/CacheCollection.h"
#include "Chaos/ChaosCache.h"
#include "Algo/Find.h"
#include "Async/ParallelFor.h"

void UChaosCacheCollection::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
#if WITH_EDITOR
	int32 NumCaches = Caches.Num();

	OutTags.Add(FAssetRegistryTag(TEXT("Number of Observed Components"), FString::FromInt(NumCaches), FAssetRegistryTag::TT_Numerical));

	float Duration = 0.0;
	for (int32 CacheIdx = 0; CacheIdx < NumCaches; ++CacheIdx)
	{
		if (UChaosCache* Cache = Caches[CacheIdx])
		{
			Duration = FMath::Max(Duration, Cache->RecordedDuration);
		}
	}	
	OutTags.Add(FAssetRegistryTag(TEXT("Recorded Duration"), FString::Printf(TEXT("%.2f"), Duration), FAssetRegistryTag::TT_Numerical));
#endif

	Super::GetAssetRegistryTags(OutTags);
}

UChaosCache* UChaosCacheCollection::FindCache(const FName& CacheName) const
{
	TObjectPtr<UChaosCache> const* ExistingCache = Algo::FindByPredicate(Caches, [&CacheName](const UChaosCache* Test)
	{
		if (Test)
		{
			return Test->GetFName() == CacheName;
		}
		return false;
	});

	return ExistingCache ? *ExistingCache : nullptr;
}

UChaosCache* UChaosCacheCollection::FindOrAddCache(const FName& CacheName)
{
	FName        FinalName   = CacheName;
	UChaosCache* ResultCache = nullptr;

	if(FinalName != NAME_None)
	{
		TObjectPtr<UChaosCache>* ExistingCache = Algo::FindByPredicate(Caches, [&FinalName](const UChaosCache* Test)
		{
			if (Test)
			{
				return Test->GetFName() == FinalName;
			}
			return false;
		});

		ResultCache = ExistingCache ? *ExistingCache : nullptr;
	}

	if(!ResultCache)
	{
		// Final check for unique name, or no name - generate one from the base name
		// (GetPlainNameString so we don't get increasing strings of appended numbers)
		if(StaticFindObject(UChaosCache::StaticClass(), this, *FinalName.ToString()) || FinalName == NAME_None)
		{
			FinalName = MakeUniqueObjectName(this, UChaosCache::StaticClass(), *FinalName.GetPlainNameString());
		}

		ResultCache = NewObject<UChaosCache>(this, FinalName, RF_Transactional);

		Caches.Add(ResultCache);
	}

	return ResultCache;
}

void UChaosCacheCollection::FlushAllCacheWrites()
{
	ParallelFor(Caches.Num(), [this](int32 InIndex)
	{
		if (Caches[InIndex])
		{
			Caches[InIndex]->FlushPendingFrames();
		}
	});
}

float UChaosCacheCollection::GetMaxDuration() const
{
	float MaxDuration = 0.0;
    for( UChaosCache* CacheInstance : Caches)
    {
    	MaxDuration = FMath::Max(CacheInstance->GetDuration(), MaxDuration);
    }
	return MaxDuration;
}