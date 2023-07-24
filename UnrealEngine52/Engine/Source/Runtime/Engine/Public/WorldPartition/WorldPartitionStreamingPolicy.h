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
	void Invalidate() { bIsValid = false; }
	bool IsValid() { return bIsValid; }
	float GetAverageVelocity(const FVector& NewPosition, const float CurrentTime);

private:
	enum { VELOCITY_HISTORY_SAMPLE_COUNT = 16 };
	bool bIsValid;
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
	virtual bool GetIntersectingCells(const TArray<FWorldPartitionStreamingQuerySource>& InSources, TArray<const IWorldPartitionCell*>& OutCells) const;
	virtual void UpdateStreamingState();
	virtual bool CanAddLoadedLevelToWorld(class ULevel* InLevel) const;
	virtual bool DrawRuntimeHash2D(class UCanvas* Canvas, const FVector2D& PartitionCanvasSize, const FVector2D& Offset, FVector2D& OutUsedCanvasSize);
	virtual void DrawRuntimeHash3D();
	virtual void DrawRuntimeCellsDetails(class UCanvas* Canvas, FVector2D& Offset) {}
	virtual void DrawStreamingStatusLegend(class UCanvas* Canvas, FVector2D& Offset) {}

	virtual bool IsStreamingCompleted(const TArray<FWorldPartitionStreamingSource>* InStreamingSources) const;
	virtual bool IsStreamingCompleted(EWorldPartitionRuntimeCellState QueryState, const TArray<FWorldPartitionStreamingQuerySource>& QuerySources, bool bExactState = true) const;

	virtual void OnCellShown(const UWorldPartitionRuntimeCell* InCell);
	virtual void OnCellHidden(const UWorldPartitionRuntimeCell* InCell);

#if WITH_EDITOR
	virtual TSubclassOf<class UWorldPartitionRuntimeCell> GetRuntimeCellClass() const PURE_VIRTUAL(UWorldPartitionStreamingPolicy::GetRuntimeCellClass, return UWorldPartitionRuntimeCell::StaticClass(); );

	// PIE/Game methods
	virtual void PrepareActorToCellRemapping() {}
	virtual void RemapSoftObjectPath(FSoftObjectPath& ObjectPath) {}

	// Editor/Runtime conversions
	virtual bool ConvertEditorPathToRuntimePath(const FSoftObjectPath& InPath, FSoftObjectPath& OutPath) const { return false; }
#endif

#if !UE_BUILD_SHIPPING
	virtual void GetOnScreenMessages(FCoreDelegates::FSeverityMessageMap& OutMessages);
#endif

	virtual UObject* GetSubObject(const TCHAR* SubObjectPath) { return nullptr; }

	const TArray<FWorldPartitionStreamingSource>& GetStreamingSources() const { return StreamingSources; }

	EWorldPartitionStreamingPerformance GetStreamingPerformance() const { return StreamingPerformance; }

protected:
	virtual int32 SetCellsStateToLoaded(const TArray<const UWorldPartitionRuntimeCell*>& ToLoadCells);
	virtual int32 SetCellsStateToActivated(const TArray<const UWorldPartitionRuntimeCell*>& ToActivateCells);
	virtual void SetCellsStateToUnloaded(const TArray<const UWorldPartitionRuntimeCell*>& ToUnloadCells);
	virtual int32 GetCellLoadingCount() const { return 0; }
	virtual int32 GetMaxCellsToLoad() const;
	virtual void UpdateStreamingSources();
	void UpdateStreamingPerformance(const TSet<const UWorldPartitionRuntimeCell*>& InCells);
	bool ShouldSkipCellForPerformance(const UWorldPartitionRuntimeCell* Cell) const;
	bool IsInBlockTillLevelStreamingCompleted(bool bIsCausedByBadStreamingPerformance = false) const;

	const UWorldPartition* WorldPartition;
	TSet<const UWorldPartitionRuntimeCell*> LoadedCells;

	struct FActivatedCells
	{
		void Add(const UWorldPartitionRuntimeCell* InCell);
		void Remove(const UWorldPartitionRuntimeCell* InCell);
		bool Contains(const UWorldPartitionRuntimeCell* InCell) const { return Cells.Contains(InCell); }
		void OnAddedToWorld(const UWorldPartitionRuntimeCell* InCell);
		void OnRemovedFromWorld(const UWorldPartitionRuntimeCell* InCell);

		const TSet<const UWorldPartitionRuntimeCell*>& GetCells() const { return Cells; }
		const TSet<const UWorldPartitionRuntimeCell*>& GetPendingAddToWorldCells() const { return PendingAddToWorldCells; }

	private:

		TSet<const UWorldPartitionRuntimeCell*> Cells;
		TSet<const UWorldPartitionRuntimeCell*> PendingAddToWorldCells;
	};

	FActivatedCells ActivatedCells;
	mutable TArray<const UWorldPartitionRuntimeCell*> SortedAddToWorldCells;

	// Streaming Sources
	TArray<FWorldPartitionStreamingSource> StreamingSources;
	TMap<FName, FStreamingSourceVelocity> StreamingSourcesVelocity;

	TSet<const UWorldPartitionRuntimeCell*> FrameActivateCells;
	TSet<const UWorldPartitionRuntimeCell*> FrameLoadCells;
	
private:
	// Update optimization
	uint32 ComputeUpdateStreamingHash() const;
	uint32 ComputeStreamingSourceHash(const FWorldPartitionStreamingSource& Source) const;
	int32 ComputeServerStreamingEnabledEpoch() const;

	const TSet<FName>& GetServerDisallowedStreamingOutDataLayers();

	static bool IsUpdateStreamingOptimEnabled();

	// CVars to control update optimization
	static bool IsUpdateOptimEnabled;
	static int32 LocationQuantization;
	static int32 RotationQuantization;
	static int32 ForceUpdateFrameCount;
	static FAutoConsoleVariableRef CVarUpdateOptimEnabled;
	static FAutoConsoleVariableRef CVarLocationQuantization;
	static FAutoConsoleVariableRef CVarRotationQuantization;
	static FAutoConsoleVariableRef CVarForceUpdateFrameCount;

	bool bLastUpdateCompletedLoadingAndActivation;
	bool bCriticalPerformanceRequestedBlockTillOnWorld;
	int32 CriticalPerformanceBlockTillLevelStreamingCompletedEpoch;
	int32 DataLayersStatesServerEpoch;
	int32 ContentBundleServerEpoch;
	int32 ServerStreamingEnabledEpoch;
	uint32 UpdateStreamingHash;
	uint32 UpdateStreamingStateCalls;

	TOptional<TSet<FName>> CachedServerDisallowStreamingOutDataLayers;

	EWorldPartitionStreamingPerformance StreamingPerformance;
#if !UE_BUILD_SHIPPING
	void UpdateDebugCellsStreamingPriority(const TSet<const UWorldPartitionRuntimeCell*>& ActivateStreamingCells, const TSet<const UWorldPartitionRuntimeCell*>& LoadStreamingCells);

	double OnScreenMessageStartTime;
	EWorldPartitionStreamingPerformance  OnScreenMessageStreamingPerformance;
#endif
};