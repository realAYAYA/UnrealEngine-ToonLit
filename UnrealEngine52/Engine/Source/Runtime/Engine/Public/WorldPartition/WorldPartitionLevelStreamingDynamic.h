// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * WorldPartitionLevelStreamingDynamic
 *
 * Dynamically controlled world partition level streaming implementation.
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingDynamic.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeLevelStreamingCell.h"
#if WITH_EDITOR
#include "WorldPartition/WorldPartitionLevelHelper.h"
#endif
#include "WorldPartitionLevelStreamingDynamic.generated.h"

UCLASS()
class ENGINE_API UWorldPartitionLevelStreamingDynamic : public ULevelStreamingDynamic
{
	GENERATED_UCLASS_BODY()

	void Load();
	void Unload();
	void Activate();
	void Deactivate();
	UWorld* GetStreamingWorld() const override;
	void SetShouldBeAlwaysLoaded(bool bInShouldBeAlwaysLoaded) { bShouldBeAlwaysLoaded = bInShouldBeAlwaysLoaded; }
	const UWorldPartitionRuntimeCell* GetWorldPartitionRuntimeCell() const { return StreamingCell.Get(); }

	virtual bool ShouldBeAlwaysLoaded() const override { return bShouldBeAlwaysLoaded; }
	virtual bool ShouldBlockOnUnload() const override;
	virtual bool ShouldRequireFullVisibilityToRender() const override { return true; }

	virtual bool CanMakeVisible() override;
	virtual bool CanMakeInvisible() override;

#if !WITH_EDITOR
	virtual void PostLoad();
#endif

	void Initialize(const UWorldPartitionRuntimeLevelStreamingCell& InCell);

	virtual bool CanReplicateStreamingStatus() const override { return false; }

#if WITH_EDITOR
	static UWorldPartitionLevelStreamingDynamic* LoadInEditor(UWorld* World, FName LevelStreamingName, const TArray<FWorldPartitionRuntimeCellObjectMapping>& InPackages);
	static void UnloadFromEditor(UWorldPartitionLevelStreamingDynamic* InLevelStreaming);

	// Override ULevelStreaming
	virtual bool RequestLevel(UWorld* PersistentWorld, bool bAllowLevelLoadRequests, EReqLevelBlock BlockPolicy) override;
	virtual void BeginDestroy() override;
	virtual TOptional<FFolder::FRootObject> GetFolderRootObject() const override;

	bool GetLoadSucceeded() const { return bLoadSucceeded; }

private:
	void CreateRuntimeLevel();
	bool IssueLoadRequests();
	void FinalizeRuntimeLevel();
	void OnCleanupLevel();

	void Initialize(UWorld* OuterWorld, const TArray<FWorldPartitionRuntimeCellObjectMapping>& InPackages);

	FName OriginalLevelPackageName;
	TArray<FWorldPartitionRuntimeCellObjectMapping> ChildPackages;
	TArray<FWorldPartitionRuntimeCellObjectMapping> ChildPackagesToLoad;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<ULevel> RuntimeLevel;
#endif

	FDelegateHandle OnCleanupLevelDelegateHandle;
	bool bLoadRequestInProgress;
	bool bLoadSucceeded;
	FWorldPartitionLevelHelper::FPackageReferencer PackageReferencer;
#endif

private:
	void UpdateShouldSkipMakingVisibilityTransactionRequest();
	bool CanChangeVisibility(bool bMakeVisible) const;

	UPROPERTY()
	bool bShouldBeAlwaysLoaded;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UActorContainer> UnsavedActorsContainer;
#endif

	UPROPERTY()
	TWeakObjectPtr<const UWorldPartitionRuntimeLevelStreamingCell> StreamingCell;

	UPROPERTY()
	TWeakObjectPtr<UWorldPartition> OuterWorldPartition;
};