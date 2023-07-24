// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "WorldPartitionRuntimeCellData.generated.h"

class UActorContainer;

/** Caches information on streaming source that will be used later on to sort cell. */
UCLASS()
class ENGINE_API UWorldPartitionRuntimeCellData : public UObject
{
	GENERATED_UCLASS_BODY()

	virtual ~UWorldPartitionRuntimeCellData() {}

	bool ShouldResetStreamingSourceInfo() const;
	virtual void ResetStreamingSourceInfo() const;
	virtual void AppendStreamingSourceInfo(const FWorldPartitionStreamingSource& Source, const FSphericalSector& SourceShape) const;
	virtual void MergeStreamingSourceInfo() const;
	virtual int32 SortCompare(const UWorldPartitionRuntimeCellData* InOther, bool bCanUseSortingCache = true) const;

	virtual const FBox& GetContentBounds() const;
	virtual FBox GetCellBounds() const;

	static int32 StreamingSourceCacheEpoch;
	static void DirtyStreamingSourceCacheEpoch() { ++StreamingSourceCacheEpoch; }

	// Source Priority
	mutable uint8 CachedMinSourcePriority;

	// Source Priorities
	mutable TArray<float> CachedSourcePriorityWeights;

	// Epoch used to dirty cache
	mutable int32 CachedSourceInfoEpoch;

	UPROPERTY()
	FBox ContentBounds;
};