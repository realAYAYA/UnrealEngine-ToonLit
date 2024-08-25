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

	ENGINE_API virtual void ResetStreamingSourceInfo() const;
	ENGINE_API virtual void AppendStreamingSourceInfo(const FWorldPartitionStreamingSource& Source, const FSphericalSector& SourceShape) const;
	ENGINE_API virtual void MergeStreamingSourceInfo() const {}
	ENGINE_API virtual int32 SortCompare(const UWorldPartitionRuntimeCellData* InOther) const;

	/** Returns the cell's content bounds, which is the sum of all actor bounds inside the cell. */
	ENGINE_API virtual const FBox& GetContentBounds() const;

	/** Returns the cell's bounds, which is the uniform size of the cell. */
	ENGINE_API virtual FBox GetCellBounds() const;

	/** Returns the cell's streaming bounds, which is what the underlying runtime hash uses to intersect cells.  */
	ENGINE_API virtual FBox GetStreamingBounds() const;	

	virtual bool IsDebugShown() const { return true; }
	ENGINE_API virtual FString GetDebugName() const;

	static ENGINE_API int32 StreamingSourceCacheEpoch;
	static inline void DirtyStreamingSourceCacheEpoch() { ++StreamingSourceCacheEpoch; }

	// Minimum affecting source priority
	mutable uint8 CachedMinSourcePriority;

	// Determine if the cell was requested by a blocking source
	mutable bool bCachedWasRequestedByBlockingSource;

	// Square distance from the cell to the closest blocking streaming source
	mutable double CachedMinSquareDistanceToBlockingSource;

	// Ratio used to determine the cell streaming performance status
	mutable float CachedMinBlockOnSlowStreamingRatio;

	// Spatial priority based on distance and angle from source
	mutable double CachedMinSpatialSortingPriority;

	// Epoch used to dirty cache
	mutable int32 CachedSourceInfoEpoch;

	UPROPERTY()
	FBox ContentBounds;

	// Optional cell bounds
	UPROPERTY()
	TOptional<FBox> CellBounds;

	UPROPERTY()
	FName GridName;

	UPROPERTY()
	int32 Priority;

	UPROPERTY()
	int32 HierarchicalLevel;

	FStringTest DebugName;
};
