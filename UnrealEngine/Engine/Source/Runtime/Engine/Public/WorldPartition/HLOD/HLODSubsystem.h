// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "UObject/ObjectKey.h"
#include "Containers/Ticker.h"
#include "HLODSubsystem.generated.h"


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
 * UHLODSubsystem
 */
UCLASS(MinimalAPI)
class UHLODSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	ENGINE_API UHLODSubsystem();
	ENGINE_API virtual ~UHLODSubsystem();

	//~ Begin USubsystem Interface.
	ENGINE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	ENGINE_API virtual void Deinitialize() override;
	//~ End USubsystem Interface.

	//~ Begin UWorldSubsystem Interface.
	ENGINE_API virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	ENGINE_API virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	//~ End UWorldSubsystem Interface.

	ENGINE_API bool Tick(float DeltaTime);

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

	ENGINE_API void SetHLODAlwaysLoadedCullDistance(int32 InCullDistance);

	ENGINE_API void OnCVarsChanged();

#if WITH_EDITOR
	uint32 GetNumOutdatedHLODActors() const { return OutdatedHLODActors.Num(); }
	static ENGINE_API bool WriteHLODStatsCSV(UWorld* InWorld, const FString& InFilename);
#endif
	
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

	struct FDrawDistanceQueue
	{
		float DrawDistance;
		TObjectPtr<AWorldPartitionHLOD> HLODActor;
	};
	
	TMap<TObjectPtr<UWorldPartition>, FWorldPartitionHLODRuntimeData> WorldPartitionsHLODRuntimeData;
	ENGINE_API const FCellData* GetCellData(const UWorldPartitionRuntimeCell* InCell) const;
	ENGINE_API FCellData* GetCellData(const UWorldPartitionRuntimeCell* InCell);
	ENGINE_API FCellData* GetCellData(AWorldPartitionHLOD* InWorldPartitionHLOD);

	TArray<AWorldPartitionHLOD*> AlwaysLoadedHLODActors;

	struct FWorldPartitionHLODWarmupState
	{
		uint32 WarmupStartFrame = INDEX_NONE;
		uint32 WarmupEndFrame = INDEX_NONE;
		FVector Location;
	};

	typedef TMap<FObjectKey, FWorldPartitionHLODWarmupState> FHLODWarmupStateMap;
	FHLODWarmupStateMap HLODActorsToWarmup;

	ENGINE_API void OnBeginRenderViews(const FSceneViewFamily& InViewFamily);
	ENGINE_API bool PrepareToWarmup(const UWorldPartitionRuntimeCell* InCell, AWorldPartitionHLOD* InHLODActor);
	ENGINE_API bool ShouldPerformWarmup() const;
	ENGINE_API bool ShouldPerformWarmupForCell(const UWorldPartitionRuntimeCell* InCell) const;
	bool bCachedShouldPerformWarmup;

	TArray<FDrawDistanceQueue> OperationQueue;
	FTSTicker::FDelegateHandle TickHandle;
	int32 LastSetCullDistance;

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

