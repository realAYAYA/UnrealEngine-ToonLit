// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CacheCollection.generated.h"

class UChaosCache;

UCLASS(Experimental)
class CHAOSCACHING_API UChaosCacheCollection : public UObject
{
	GENERATED_BODY()
public:

	//~ Begin UObject Interface
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	//~ End UObject Interface
	
	UChaosCache* FindCache(const FName& CacheName) const;
	UChaosCache* FindOrAddCache(const FName& CacheName);

	void FlushAllCacheWrites();

	const TArray<UChaosCache*>& GetCaches() const {return Caches;}

	/** Return the max duration of all the caches stored in the collection */
	float GetMaxDuration() const;

	UPROPERTY(EditAnywhere, Instanced, Category="Caching", meta=(EditFixedOrder))
	TArray<TObjectPtr<UChaosCache>> Caches;
};