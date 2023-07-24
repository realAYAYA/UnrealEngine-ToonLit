// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "UObject/ObjectKey.h"
#include "HLODSubsystem.generated.h"


class AWorldPartitionHLOD;
class FSceneViewFamily;
class FHLODResourcesResidencySceneViewExtension;
class UWorldPartition;
class UWorldPartitionRuntimeCell;
class UWorld;

DECLARE_MULTICAST_DELEGATE_OneParam(FWorldPartitionHLODActorRegisteredEvent, AWorldPartitionHLOD* /* InHLODActor */);
DECLARE_MULTICAST_DELEGATE_OneParam(FWorldPartitionHLODActorUnregisteredEvent, AWorldPartitionHLOD* /* InHLODActor */);


/**
 * UHLODSubsystem
 */
UCLASS()
class ENGINE_API UHLODSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UHLODSubsystem();
	virtual ~UHLODSubsystem();

	//~ Begin USubsystem Interface.
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface.

	//~ Begin UWorldSubsystem Interface.
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	//~ End UWorldSubsystem Interface.

	void RegisterHLODActor(AWorldPartitionHLOD* InWorldPartitionHLOD);
	void UnregisterHLODActor(AWorldPartitionHLOD* InWorldPartitionHLOD);

	void OnCellShown(const UWorldPartitionRuntimeCell* InCell);
	void OnCellHidden(const UWorldPartitionRuntimeCell* InCell);

	bool CanMakeVisible(const UWorldPartitionRuntimeCell* InCell);
	bool CanMakeInvisible(const UWorldPartitionRuntimeCell* InCell);

	const TArray<AWorldPartitionHLOD*>& GetHLODActorsForCell(const UWorldPartitionRuntimeCell* InCell) const;

	static bool IsHLODEnabled();

	FWorldPartitionHLODActorRegisteredEvent& OnHLODActorRegisteredEvent() { return HLODActorRegisteredEvent; }
	FWorldPartitionHLODActorUnregisteredEvent& OnHLODActorUnregisteredEvent() { return HLODActorUnregisteredEvent; }

	void SetHLODAlwaysLoadedCullDistance(int32 InCullDistance);

	void OnCVarsChanged();
	
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
	const FCellData* GetCellData(const UWorldPartitionRuntimeCell* InCell) const;
	FCellData* GetCellData(const UWorldPartitionRuntimeCell* InCell);
	FCellData* GetCellData(AWorldPartitionHLOD* InWorldPartitionHLOD);

	TArray<AWorldPartitionHLOD*> AlwaysLoadedHLODActors;

	struct FWorldPartitionHLODWarmupState
	{
		uint32 WarmupStartFrame = INDEX_NONE;
		uint32 WarmupEndFrame = INDEX_NONE;
		FVector Location;
	};

	typedef TMap<FObjectKey, FWorldPartitionHLODWarmupState> FHLODWarmupStateMap;
	FHLODWarmupStateMap HLODActorsToWarmup;

	void OnBeginRenderViews(const FSceneViewFamily& InViewFamily);
	bool PrepareToWarmup(const UWorldPartitionRuntimeCell* InCell, AWorldPartitionHLOD* InHLODActor);
	bool ShouldPerformWarmup() const;
	bool ShouldPerformWarmupForCell(const UWorldPartitionRuntimeCell* InCell) const;
	bool bCachedShouldPerformWarmup;

	/** Console command used to turn on/off loading & rendering of world partition HLODs */
	static class FAutoConsoleCommand EnableHLODCommand;

	static bool WorldPartitionHLODEnabled;

	friend class FHLODResourcesResidencySceneViewExtension;
	TSharedPtr<FHLODResourcesResidencySceneViewExtension, ESPMode::ThreadSafe> SceneViewExtension;

	void OnWorldPartitionInitialized(UWorldPartition* InWorldPartition);
	void OnWorldPartitionUninitialized(UWorldPartition* InWorldPartition);

	FWorldPartitionHLODActorRegisteredEvent		HLODActorRegisteredEvent;
	FWorldPartitionHLODActorUnregisteredEvent	HLODActorUnregisteredEvent;
};

