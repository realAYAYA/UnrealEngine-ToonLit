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
#include "WorldPartition/WorldPartitionRuntimeContainerResolving.h"
#include "WorldPartitionStreamingPolicy.generated.h"

class UWorldPartition;
class FWorldPartitionDraw2DContext;

USTRUCT()
struct FActivatedCells
{
	GENERATED_USTRUCT_BODY()

	void Add(const UWorldPartitionRuntimeCell* InCell);
	void Remove(const UWorldPartitionRuntimeCell* InCell);
	bool Contains(const UWorldPartitionRuntimeCell* InCell) const { return Cells.Contains(InCell); }
	void OnAddedToWorld(const UWorldPartitionRuntimeCell* InCell);
	void OnRemovedFromWorld(const UWorldPartitionRuntimeCell* InCell);

	const TSet<TObjectPtr<const UWorldPartitionRuntimeCell>>& GetCells() const { return Cells; }
	const TSet<const UWorldPartitionRuntimeCell*>& GetPendingAddToWorldCells() const { return PendingAddToWorldCells; }

private:

	UPROPERTY(Transient)
	TSet<TObjectPtr<const UWorldPartitionRuntimeCell>> Cells;

	TSet<const UWorldPartitionRuntimeCell*> PendingAddToWorldCells;
};

UCLASS(Abstract, Within = WorldPartition)
class UWorldPartitionStreamingPolicy : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	virtual bool GetIntersectingCells(const TArray<FWorldPartitionStreamingQuerySource>& InSources, TArray<const IWorldPartitionCell*>& OutCells) const;
	virtual void UpdateStreamingState();
	virtual bool CanAddCellToWorld(const UWorldPartitionRuntimeCell* InCell) const;
	virtual bool DrawRuntimeHash2D(FWorldPartitionDraw2DContext& DrawContext);
	virtual void DrawRuntimeHash3D();
	virtual void DrawRuntimeCellsDetails(class UCanvas* Canvas, FVector2D& Offset) {}
	
	virtual bool IsStreamingCompleted(const TArray<FWorldPartitionStreamingSource>* InStreamingSources) const;
	virtual bool IsStreamingCompleted(EWorldPartitionRuntimeCellState QueryState, const TArray<FWorldPartitionStreamingQuerySource>& QuerySources, bool bExactState = true) const;

	virtual void OnCellShown(const UWorldPartitionRuntimeCell* InCell);
	virtual void OnCellHidden(const UWorldPartitionRuntimeCell* InCell);

	UE_DEPRECATED(5.3, "CanAddLoadedLevelToWorld is deprecated.")
	virtual bool CanAddLoadedLevelToWorld(class ULevel* InLevel) const { return true; }

#if WITH_EDITOR
	virtual TSubclassOf<class UWorldPartitionRuntimeCell> GetRuntimeCellClass() const PURE_VIRTUAL(UWorldPartitionStreamingPolicy::GetRuntimeCellClass, return UWorldPartitionRuntimeCell::StaticClass(); );

	// PIE/Game methods
	virtual void PrepareActorToCellRemapping() {}
	virtual void SetContainerResolver(const FWorldPartitionRuntimeContainerResolver& InContainerResolver) {}
	virtual void RemapSoftObjectPath(FSoftObjectPath& ObjectPath) const {}

	UE_DEPRECATED(5.4, "Use StoreStreamingContentToExternalStreamingObject instead.")
	virtual bool StoreToExternalStreamingObject(URuntimeHashExternalStreamingObjectBase& OutExternalStreamingObject) { return StoreStreamingContentToExternalStreamingObject(OutExternalStreamingObject); }
	virtual bool StoreStreamingContentToExternalStreamingObject(URuntimeHashExternalStreamingObjectBase& OutExternalStreamingObject) { return true; }
	virtual bool ConvertContainerPathToEditorPath(const FActorContainerID& InContainerID, const FSoftObjectPath& InPath, FSoftObjectPath& OutPath) const { return false; }
#endif

#if !UE_BUILD_SHIPPING
	virtual void GetOnScreenMessages(FCoreDelegates::FSeverityMessageMap& OutMessages);
#endif

	// Editor/Runtime conversions
	virtual bool ConvertEditorPathToRuntimePath(const FSoftObjectPath& InPath, FSoftObjectPath& OutPath) const { return false; }
	virtual UObject* GetSubObject(const TCHAR* SubObjectPath) { return nullptr; }

	const TArray<FWorldPartitionStreamingSource>& GetStreamingSources() const { return StreamingSources; }

	EWorldPartitionStreamingPerformance GetStreamingPerformance() const { return StreamingPerformance; }

	static bool IsUpdateStreamingOptimEnabled();

	virtual bool InjectExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject) { return true; }
	virtual bool RemoveExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject) { return true; }

	virtual void SetShouldMergeStreamingSourceInfo(bool bInShouldMergeStreamingSourceInfo) { bShouldMergeStreamingSourceInfo = bInShouldMergeStreamingSourceInfo; }

protected:
	virtual void SetCellStateToLoaded(const UWorldPartitionRuntimeCell* InCell, int32& InOutMaxCellsToLoad);
	virtual void SetCellStateToActivated(const UWorldPartitionRuntimeCell* InCell, int32& InOutMaxCellsToLoad);
	virtual void SetCellsStateToUnloaded(const TArray<const UWorldPartitionRuntimeCell*>& ToUnloadCells);
	virtual void GetCellsToUpdate(TArray<const UWorldPartitionRuntimeCell*>& OutToLoadCells, TArray<const UWorldPartitionRuntimeCell*>& OutToActivateCells);
	virtual void GetCellsToReprioritize(TArray<const UWorldPartitionRuntimeCell*>& OutToLoadCells, TArray<const UWorldPartitionRuntimeCell*>& OutToActivateCells);
	virtual void UpdateStreamingSources(bool bCanOptimizeUpdate);
	void UpdateStreamingPerformance(const TSet<const UWorldPartitionRuntimeCell*>& InCells);
	bool ShouldSkipCellForPerformance(const UWorldPartitionRuntimeCell* Cell) const;
	bool IsInBlockTillLevelStreamingCompleted(bool bIsCausedByBadStreamingPerformance = false) const;

	const UWorldPartition* WorldPartition;
	UPROPERTY(Transient)
	TSet<TObjectPtr<const UWorldPartitionRuntimeCell>> LoadedCells;

	UPROPERTY(Transient)
	FActivatedCells ActivatedCells;

	// Streaming Sources
	TArray<FWorldPartitionStreamingSource> StreamingSources;

	TSet<const UWorldPartitionRuntimeCell*> FrameActivateCells;
	TSet<const UWorldPartitionRuntimeCell*> FrameLoadCells;

	// Used by UWorldPartitionSubsystem
	UPROPERTY(Transient)
	TArray<TObjectPtr<const UWorldPartitionRuntimeCell>> ToActivateCells;
	
	UPROPERTY(Transient)
	TArray<TObjectPtr<const UWorldPartitionRuntimeCell>> ToLoadCells;
	
	int32 ProcessedToActivateCells;
	int32 ProcessedToLoadCells;

private:
	// Update optimization
	uint32 ComputeUpdateStreamingHash(bool bCanOptimizeUpdate) const;
	int32 ComputeServerStreamingEnabledEpoch() const;

	const TSet<FName>& GetServerDisallowedStreamingOutDataLayers();

	// CVars to control update optimization
	static bool IsUpdateOptimEnabled;
	static int32 ForceUpdateFrameCount;
	static FAutoConsoleVariableRef CVarUpdateOptimEnabled;
	static FAutoConsoleVariableRef CVarForceUpdateFrameCount;

	bool bCriticalPerformanceRequestedBlockTillOnWorld;
	
	UPROPERTY()
	bool bShouldMergeStreamingSourceInfo;

	int32 CriticalPerformanceBlockTillLevelStreamingCompletedEpoch;
	int32 ServerStreamingStateEpoch;
	int32 ServerStreamingEnabledEpoch;
	uint32 UpdateStreamingHash;
	uint32 UpdateStreamingSourcesHash;
	uint32 UpdateStreamingStateCalls;

	TOptional<TSet<FName>> CachedServerDisallowStreamingOutDataLayers;

	EWorldPartitionStreamingPerformance StreamingPerformance;
#if !UE_BUILD_SHIPPING
	void UpdateDebugCellsStreamingPriority(const TSet<const UWorldPartitionRuntimeCell*>& ActivateStreamingCells, const TSet<const UWorldPartitionRuntimeCell*>& LoadStreamingCells);

	double OnScreenMessageStartTime;
	EWorldPartitionStreamingPerformance  OnScreenMessageStreamingPerformance;
#endif

	friend class UWorldPartitionSubsystem;
};