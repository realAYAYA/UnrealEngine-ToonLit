// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "WorldPartition/WorldPartitionActorLoaderInterface.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "Templates/PimplPtr.h"

#include "HLODEditorSubsystem.generated.h"


class AActor;
class AWorldPartitionHLOD;
class UPrimitiveComponent;
class UWorldPartition;
struct FWorldPartitionHLODEditorData;

/**
 * UWorldPartitionHLODEditorSubsystem
 */
UCLASS()
class WORLDPARTITIONEDITOR_API UWorldPartitionHLODEditorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UWorldPartitionHLODEditorSubsystem();
	virtual ~UWorldPartitionHLODEditorSubsystem();

	//~ Begin USubsystem Interface.
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface.

	//~ Begin UWorldSubsystem Interface.
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem Interface.

	//~ Begin FTickableGameObject Interface
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject Interface
	
private:
	bool IsHLODInEditorEnabled();
	void SetHLODInEditorEnabled(bool bInEnable);

	void OnWorldPartitionInitialized(UWorldPartition* InWorldPartition);
	void OnWorldPartitionUninitialized(UWorldPartition* InWorldPartition);

	void OnLoaderAdapterStateChanged(const IWorldPartitionActorLoaderInterface::ILoaderAdapter* LoaderAdapter);

	void ForceHLODStateUpdate();
	
private:
	FVector CachedCameraLocation;
	double CachedHLODMinDrawDistance;
	double CachedHLODMaxDrawDistance;
	bool bCachedShowHLODsOverLoadedRegions;
	bool bForceHLODStateUpdate;

	TPimplPtr<FWorldPartitionHLODEditorData> HLODEditorData;
};
