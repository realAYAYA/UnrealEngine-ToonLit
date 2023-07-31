// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "UObject/NameTypes.h"
#include "SceneViewExtension.h"

#include "HLODSubsystem.generated.h"

class UWorldPartition;
class UWorldPartitionRuntimeCell;
class AWorldPartitionHLOD;

class FHLODResourcesResidencySceneViewExtension : public FWorldSceneViewExtension
{
public:
	FHLODResourcesResidencySceneViewExtension(const FAutoRegister& AutoRegister, UWorld* InWorld)
		: FWorldSceneViewExtension(AutoRegister, InWorld)
	{
	}

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}

	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
};

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
	//~ End UWorldSubsystem Interface.

	void RegisterHLODActor(AWorldPartitionHLOD* InWorldPartitionHLOD);
	void UnregisterHLODActor(AWorldPartitionHLOD* InWorldPartitionHLOD);

	void OnCellShown(const UWorldPartitionRuntimeCell* InCell);
	void OnCellHidden(const UWorldPartitionRuntimeCell* InCell);

	bool RequestUnloading(const UWorldPartitionRuntimeCell* InCell);

	static bool IsHLODEnabled();
	
private:
	struct FCellData
	{
		bool						bIsCellVisible;
		TSet<AWorldPartitionHLOD*>	LoadedHLODs;

		uint32						WarmupStartFrame;
		uint32						WarmupEndFrame;

		FCellData() 
			: bIsCellVisible(false)
			, WarmupStartFrame(INDEX_NONE)
			, WarmupEndFrame(INDEX_NONE)
		{
		}
	};

	struct FWorldPartitionHLODRuntimeData
	{
		TMap<FName, FCellData> CellsData;
		TSet<FCellData*> CellsToWarmup;
	};
	
	TMap<TObjectPtr<UWorldPartition>, FWorldPartitionHLODRuntimeData> WorldPartitionsHLODRuntimeData;
		
	void OnWorldPartitionInitialized(UWorldPartition* InWorldPartition);
	void OnWorldPartitionUninitialized(UWorldPartition* InWorldPartition);
	void OnBeginRenderViews(const FSceneViewFamily& InViewFamily);

	void MakeRenderResourcesResident(const FCellData& CellHLODs, const FSceneViewFamily& InViewFamily);

	/** Console command used to turn on/off loading & rendering of world partition HLODs */
	static class FAutoConsoleCommand EnableHLODCommand;

	static bool WorldPartitionHLODEnabled;

	friend class FHLODResourcesResidencySceneViewExtension;
	TSharedPtr<FHLODResourcesResidencySceneViewExtension, ESPMode::ThreadSafe> SceneViewExtension;
};
