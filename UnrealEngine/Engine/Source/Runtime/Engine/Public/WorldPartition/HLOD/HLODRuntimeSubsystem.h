// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "UObject/ObjectKey.h"
#include "Containers/Ticker.h"
#include "HLODRuntimeSubsystem.generated.h"


class AWorldPartitionHLOD;
class FSceneViewFamily;
class FHLODResourcesResidencySceneViewExtension;
class UWorldPartition;
class UWorldPartitionRuntimeCell;
class URuntimeHashExternalStreamingObjectBase;
class UWorld;

DECLARE_MULTICAST_DELEGATE_OneParam(FWorldPartitionHLODActorRegisteredEvent, AWorldPartitionHLOD* /* InHLODActor */);
DECLARE_MULTICAST_DELEGATE_OneParam(FWorldPartitionHLODActorUnregisteredEvent, AWorldPartitionHLOD* /* InHLODActor */);


/**
 * UWorldPartitionHLODRuntimeSubsystem
 */
UCLASS(MinimalAPI)
class UWorldPartitionHLODRuntimeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	ENGINE_API UWorldPartitionHLODRuntimeSubsystem();

	//~ Begin USubsystem Interface.
	ENGINE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	ENGINE_API virtual void Deinitialize() override;
	//~ End USubsystem Interface.

	//~ Begin UWorldSubsystem Interface.
	ENGINE_API virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem Interface.

	ENGINE_API void RegisterHLODActor(AWorldPartitionHLOD* InWorldPartitionHLOD);
	ENGINE_API void UnregisterHLODActor(AWorldPartitionHLOD* InWorldPartitionHLOD);

	ENGINE_API void OnCellShown(const UWorldPartitionRuntimeCell* InCell);
	ENGINE_API void OnCellHidden(const UWorldPartitionRuntimeCell* InCell);

	ENGINE_API bool CanMakeVisible(const UWorldPartitionRuntimeCell* InCell);
	ENGINE_API bool CanMakeInvisible(const UWorldPartitionRuntimeCell* InCell);

	ENGINE_API const TArray<AWorldPartitionHLOD*>& GetHLODActorsForCell(const UWorldPartitionRuntimeCell* InCell) const;

	static ENGINE_API bool IsHLODEnabled();

	FWorldPartitionHLODActorRegisteredEvent& OnHLODActorRegisteredEvent() { return HLODActorRegisteredEvent; }
	FWorldPartitionHLODActorUnregisteredEvent& OnHLODActorUnregisteredEvent() { return HLODActorUnregisteredEvent; }

	ENGINE_API void OnExternalStreamingObjectInjected(URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject);
	ENGINE_API void OnExternalStreamingObjectRemoved(URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject);

	ENGINE_API void OnCVarsChanged();

#if WITH_EDITOR
	uint32 GetNumOutdatedHLODActors() const { return OutdatedHLODActors.Num(); }
	static ENGINE_API bool WriteHLODStatsCSV(UWorld* InWorld, const FString& InFilename);
#endif

	UE_DEPRECATED(5.4, "You should perform this logic on the game side.")
	ENGINE_API void SetHLODAlwaysLoadedCullDistance(int32 InCullDistance) {}
	
private:
	struct FCellData
	{
		bool bIsCellVisible = false;
		TArray<AWorldPartitionHLOD*> LoadedHLODs;
	};

	struct FWorldPartitionHLODRuntimeData
	{
		TMap<FGuid, FCellData> CellsData;
	};

	TMap<TObjectPtr<UWorldPartition>, FWorldPartitionHLODRuntimeData> WorldPartitionsHLODRuntimeData;
	ENGINE_API const FCellData* GetCellData(const UWorldPartitionRuntimeCell* InCell) const;
	ENGINE_API FCellData* GetCellData(const UWorldPartitionRuntimeCell* InCell);
	ENGINE_API FCellData* GetCellData(AWorldPartitionHLOD* InWorldPartitionHLOD);

	struct FWorldPartitionHLODWarmupState
	{
		uint32 WarmupLastRequestedFrame = INDEX_NONE;
		uint32 WarmupCallsUntilReady = INDEX_NONE;
		FBox WarmupBounds;
	};

	typedef TMap<FObjectKey, FWorldPartitionHLODWarmupState> FHLODWarmupStateMap;
	FHLODWarmupStateMap HLODActorsToWarmup;

	ENGINE_API void OnBeginRenderViews(const FSceneViewFamily& InViewFamily);
	ENGINE_API bool PrepareToWarmup(const UWorldPartitionRuntimeCell* InCell, AWorldPartitionHLOD* InHLODActor);
	ENGINE_API bool ShouldPerformWarmup() const;
	ENGINE_API bool ShouldPerformWarmupForCell(const UWorldPartitionRuntimeCell* InCell) const;
	bool bCachedShouldPerformWarmup;

	/** Console command used to turn on/off loading & rendering of world partition HLODs */
	static ENGINE_API class FAutoConsoleCommand EnableHLODCommand;

	static ENGINE_API bool WorldPartitionHLODEnabled;

	friend class FHLODResourcesResidencySceneViewExtension;
	TSharedPtr<FHLODResourcesResidencySceneViewExtension, ESPMode::ThreadSafe> SceneViewExtension;

	ENGINE_API void OnWorldPartitionInitialized(UWorldPartition* InWorldPartition);
	ENGINE_API void OnWorldPartitionUninitialized(UWorldPartition* InWorldPartition);

	FWorldPartitionHLODActorRegisteredEvent		HLODActorRegisteredEvent;
	FWorldPartitionHLODActorUnregisteredEvent	HLODActorUnregisteredEvent;

#if WITH_EDITOR
	TSet<AWorldPartitionHLOD*> OutdatedHLODActors;
#endif
};

