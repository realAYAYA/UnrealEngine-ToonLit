// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "StringDev.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "WorldPartitionRuntimeCellData.generated.h"

class UActorContainer;

/** Caches information on streaming source that will be used later on to sort cell. */
UCLASS(MinimalAPI)
class UWorldPartitionRuntimeCellData : public UObject
{
	GENERATED_UCLASS_BODY()

	virtual ~UWorldPartitionRuntimeCellData() {}

	//~Begin UObject Interface
	ENGINE_API void Serialize(FArchive& Ar);
	//~End UObject Interface

	inline bool ShouldResetStreamingSourceInfo() const { return CachedSourceInfoEpoch != StreamingSourceCacheEpoch; }

	ENGINE_API virtual void ResetStreamingSourceInfo() const;
	ENGINE_API virtual void AppendStreamingSourceInfo(const FWorldPartitionStreamingSource& Source, const FSphericalSector& SourceShape) const;
	ENGINE_API virtual void MergeStreamingSourceInfo() const;
	ENGINE_API virtual int32 SortCompare(const UWorldPartitionRuntimeCellData* InOther, bool bCanUseSortingCache = true) const;

	ENGINE_API virtual const FBox& GetContentBounds() const;
	ENGINE_API virtual FBox GetCellBounds() const;

	virtual bool IsDebugShown() const { return true; }
	ENGINE_API virtual FString GetDebugName() const;

	static ENGINE_API int32 StreamingSourceCacheEpoch;
	static inline void DirtyStreamingSourceCacheEpoch() { ++StreamingSourceCacheEpoch; }

	// Source Priority
	mutable uint8 CachedMinSourcePriority;

	// Source Priorities
	mutable TArray<float> CachedSourcePriorityWeights;

	// Epoch used to dirty cache
	mutable int32 CachedSourceInfoEpoch;

	UPROPERTY()
	FBox ContentBounds;

	FStringTest DebugName;
};
