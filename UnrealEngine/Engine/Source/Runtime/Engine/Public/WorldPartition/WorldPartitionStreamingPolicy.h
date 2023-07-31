// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * WorldPartitionStreamingPolicy
 *
 * Base class for World Partition Runtime Streaming Policy
 *
 */

#pragma once

#include "Containers/Set.h"
#include "Misc/CoreDelegates.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "WorldPartitionStreamingPolicy.generated.h"

class UWorldPartition;

/**
 * Helper to compute streaming source velocity based on position history.
 */
struct FStreamingSourceVelocity
{
	FStreamingSourceVelocity(const FName& InSourceName);
	float GetAverageVelocity(const FVector& NewPosition, const float CurrentTime);

private:
	enum { VELOCITY_HISTORY_SAMPLE_COUNT = 16 };
	FName SourceName;
	int32 LastIndex;
	float LastUpdateTime;
	FVector LastPosition;
	float VelocitiesHistorySum;
	TArray<float, TInlineAllocator<VELOCITY_HISTORY_SAMPLE_COUNT>> VelocitiesHistory;
};

UCLASS(Abstract, Within = WorldPartition)
class UWorldPartitionStreamingPolicy : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	virtual void UpdateStreamingState();
	virtual bool CanAddLoadedLevelToWorld(class ULevel* InLevel) const;
	virtual bool DrawRuntimeHash2D(class UCanvas* Canvas, const FVector2D& PartitionCanvasSize, const FVector2D& Offset, FVector2D& OutUsedCanvasSize);
	virtual void DrawRuntimeHash3D();
	virtual void DrawRuntimeCellsDetails(class UCanvas* Canvas, FVector2D& Offset) {}
	virtual void DrawStreamingStatusLegend(class UCanvas* Canvas, FVector2D& Offset) {}

	virtual bool IsStreamingCompleted(const FWorldPartitionStreamingSource* InStreamingSource) const;
	virtual bool IsStreamingCompleted(EWorldPartitionRuntimeCellState QueryState, const TArray<FWorldPartitionStreamingQuerySource>& QuerySources, bool bExactState = true) const;

#if WITH_EDITOR
	virtual TSubclassOf<class UWorldPartitionRuntimeCell> GetRuntimeCellClass() const PURE_VIRTUAL(UWorldPartitionStreamingPolicy::GetRuntimeCellClass, return UWorldPartitionRuntimeCell::StaticClass(); );

	// PIE/Game methods
	virtual void PrepareActorToCellRemapping() {}
	virtual void RemapSoftObjectPath(FSoftObjectPath& ObjectPath) {}
#endif

#if !UE_BUILD_SHIPPING
	virtual void GetOnScreenMessages(FCoreDelegates::FSeverityMessageMap& OutMessages);
#endif

	virtual UObject* GetSubObject(const TCHAR* SubObjectPath) { return nullptr; }

	const TArray<FWorldPartitionStreamingSource>& GetStreamingSources() const { return StreamingSources; }

	EWorldPartitionStreamingPerformance GetStreamingPerformance() const { return StreamingPerformance; }

protected:
	virtual void SetCellsStateToLoaded(const TArray<const UWorldPartitionRuntimeCell*>& ToLoadCells);
	virtual void SetCellsStateToActivated(const TArray<const UWorldPartitionRuntimeCell*>& ToActivateCells);
	virtual void SetCellsStateToUnloaded(const TArray<const UWorldPartitionRuntimeCell*>& ToUnloadCells);
	virtual int32 GetCellLoadingCount() const { return 0; }
	virtual int32 GetMaxCellsToLoad() const;
	virtual void UpdateStreamingSources();
	void UpdateStreamingPerformance(const TSet<const UWorldPartitionRuntimeCell*>& CellsToActivate);
	bool ShouldSkipCellForPerformance(const UWorldPartitionRuntimeCell* Cell) const;
	bool IsInBlockTillLevelStreamingCompleted(bool bIsCausedByBadStreamingPerformance = false) const;

	const UWorldPartition* WorldPartition;
	TSet<const UWorldPartitionRuntimeCell*> LoadedCells;
	TSet<const UWorldPartitionRuntimeCell*> ActivatedCells;

	// Streaming Sources
	TArray<FWorldPartitionStreamingSource> StreamingSources;
	TMap<FName, FStreamingSourceVelocity> StreamingSourcesVelocity;

	TSet<const UWorldPartitionRuntimeCell*> FrameActivateCells;
	TSet<const UWorldPartitionRuntimeCell*> FrameLoadCells;
	
	bool bCriticalPerformanceRequestedBlockTillOnWorld;
	int32 CriticalPerformanceBlockTillLevelStreamingCompletedEpoch;
	mutable TArray<const UWorldPartitionRuntimeCell*> SortedAddToWorldCells;

	int32 DataLayersStatesServerEpoch;
	int32 ContentBundleServerEpoch;
	int32 ServerStreamingEnabledEpoch;

	EWorldPartitionStreamingPerformance StreamingPerformance;
#if !UE_BUILD_SHIPPING
	void UpdateDebugCellsStreamingPriority(const TSet<const UWorldPartitionRuntimeCell*>& ActivateStreamingCells, const TSet<const UWorldPartitionRuntimeCell*>& LoadStreamingCells);

	double OnScreenMessageStartTime;
	EWorldPartitionStreamingPerformance  OnScreenMessageStreamingPerformance;
#endif
};