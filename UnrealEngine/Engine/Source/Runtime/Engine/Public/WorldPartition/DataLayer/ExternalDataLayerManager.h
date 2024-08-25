// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreFwd.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/DataLayer/WorldDataLayersActorDesc.h"
#include "WorldPartition/DataLayer/ExternalDataLayerEngineSubsystem.h"

#include "ExternalDataLayerManager.generated.h"

class UActorDescContainerInstance;
class UDataLayerManager;
class UExternalDataLayerAsset;
class UExternalDataLayerInstance;
class UWorldPartitionRuntimeCell;
class URuntimeHashExternalStreamingObjectBase;
class AWorldDataLayers;

UCLASS(Within = WorldPartition)
class ENGINE_API UExternalDataLayerManager : public UObject
{
	GENERATED_BODY()

public:
	template <class T>
	static UExternalDataLayerManager* GetExternalDataLayerManager(const T* InObject)
	{
		UWorldPartition* WorldPartition = IsValid(InObject) ? FWorldPartitionHelpers::GetWorldPartition(InObject) : nullptr;
		return WorldPartition ? WorldPartition->GetExternalDataLayerManager() : nullptr;
	}

private:
	//~ Begin Initialization/Deinitialization
	UExternalDataLayerManager();
	void Initialize();
	bool IsInitialized() const { return bIsInitialized; }
	void DeInitialize();
	//~ End Initialization/Deinitialization

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	//~ Begin Injection/Removal
	void UpdateExternalDataLayerInjectionState(const UExternalDataLayerAsset* InExternalDataLayerAsset);
	bool CanInjectExternalDataLayerAsset(const UExternalDataLayerAsset* InExternalDataLayerAsset, FText* OutReason = nullptr) const;
	bool IsExternalDataLayerInjected(const UExternalDataLayerAsset* InExternalDataLayerAsset) const;
	bool InjectExternalDataLayer(const UExternalDataLayerAsset* InExternalDataLayerAsset);
	bool RemoveExternalDataLayer(const UExternalDataLayerAsset* InExternalDataLayerAsset);
	bool InjectIntoGameWorld(const UExternalDataLayerAsset* InExternalDataLayerAsset);
	bool RemoveFromGameWorld(const UExternalDataLayerAsset* InExternalDataLayerAsset);
	bool RegisterExternalStreamingObjectForGameWorld(const UExternalDataLayerAsset* InExternalDataLayerAsset);
	bool UnregisterExternalStreamingObjectForGameWorld(const UExternalDataLayerAsset* InExternalDataLayerAsset);
	//~ End Injection/Removal

	// Monitor when EDL state changes
	void OnExternalDataLayerAssetRegistrationStateChanged(const UExternalDataLayerAsset* InExternalDataLayerAsset, EExternalDataLayerRegistrationState InOldState, EExternalDataLayerRegistrationState InNewState);

	FString GetExternalDataLayerLevelRootPath(const UExternalDataLayerAsset* InExternalDataLayerAsset) const;
	FString GetExternalStreamingObjectPackagePath(const UExternalDataLayerAsset* InExternalDataLayerAsset) const;
	const UExternalDataLayerInstance* GetExternalDataLayerInstance(const UExternalDataLayerAsset* InExternalDataLayerAsset) const;
	UExternalDataLayerInstance* GetExternalDataLayerInstance(const UExternalDataLayerAsset* InExternalDataLayerAsset);
	UDataLayerManager& GetDataLayerManager() const;

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
	//~ End UObject Interface

	// Used in editor
	UActorDescContainerInstance* RegisterExternalDataLayerActorDescContainer(const UExternalDataLayerAsset* InExternalDataLayerAsset);
	bool UnregisterExternalDataLayerActorDescContainer(const UExternalDataLayerAsset* InExternalDataLayerAsset);
	bool OnActorExternalDataLayerAssetChanged(AActor* InActor);
	bool RegisterExternalDataLayerInstance(UExternalDataLayerInstance* InExternalDataLayerInstance);
	bool UnregisterExternalDataLayerInstance(UExternalDataLayerInstance* InExternalDataLayerInstance);
	const UExternalDataLayerAsset* GetMatchingExternalDataLayerAssetForObjectPath(const FSoftObjectPath& InObjectPath);
	const UExternalDataLayerAsset* GetActorEditorContextCurrentExternalDataLayer() const;
	AWorldDataLayers* GetWorldDataLayers(const UExternalDataLayerAsset* InExternalDataLayerAsset, bool bInAllowCreate = false) const;
	bool OnActorPreSpawnInitialization(AActor* InActor, const UExternalDataLayerAsset* InExternalDataLayerAsset);
	FString GetActorPackageName(const UExternalDataLayerAsset* InExternalDataLayerAsset, const ULevel* InDestinationLevel, const FString& InActorPath) const;
	bool SetupActorPackageForExternalDataLayerAsset(AActor* InActor, const UExternalDataLayerAsset* InExternalDataLayerAsset);
	URuntimeHashExternalStreamingObjectBase* CreateExternalStreamingObjectUsingStreamingGeneration(const UExternalDataLayerAsset* InExternalDataLayerAsset);

	// Used for PIE/-game
	void OnBeginPlay();
	void OnEndPlay();

	//~ Begin Cooking
	UWorldPartitionRuntimeCell* GetCellForCookPackage(const FString& InCookPackageName) const;
	URuntimeHashExternalStreamingObjectBase* GetExternalStreamingObjectForCookPackage(const FString& InCookPackageName) const;
	void ForEachExternalStreamingObjects(TFunctionRef<bool(URuntimeHashExternalStreamingObjectBase*)> Func) const;
	//~ End Cooking
#endif

private:
	bool IsRunningGameOrInstancedWorldPartition() const { return bIsRunningGameOrInstancedWorldPartition; }

	bool bIsInitialized;
	bool bIsRunningGameOrInstancedWorldPartition;

	UPROPERTY(Transient)
	TMap<TObjectPtr<const UExternalDataLayerAsset>, TObjectPtr<URuntimeHashExternalStreamingObjectBase>> ExternalStreamingObjects;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	TSet<TObjectPtr<const UExternalDataLayerAsset>> InjectedExternalDataLayerAssets;
#endif

#if WITH_EDITOR
	using FExternalDataLayerContainerMap = TMap<TObjectPtr<const UExternalDataLayerAsset>, TObjectPtr<UActorDescContainerInstance>>;
	FExternalDataLayerContainerMap EDLContainerMap;
	TMap<TObjectPtr<const UExternalDataLayerAsset>, FWorldPartitionReference> EDLWorldDataLayersMap;
	TSet<TObjectPtr<const UExternalDataLayerAsset>> PreEditUndoExternalDataLayerAssets;
#endif

	friend class FDataLayerMode;
	friend class AWorldDataLayers;
	friend class UWorldPartition;
	friend class UExternalDataLayerInstance;
	friend class UDataLayerEditorSubsystem;
	friend class ULevelInstanceSubsystem;
	friend class UWorldPartitionRuntimeLevelStreamingCell;
	friend class UGameFeatureActionConvertContentBundleWorldPartitionBuilder;
};