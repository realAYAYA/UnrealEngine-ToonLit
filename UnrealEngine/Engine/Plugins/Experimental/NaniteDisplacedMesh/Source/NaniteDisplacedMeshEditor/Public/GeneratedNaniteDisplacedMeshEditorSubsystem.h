// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"

#include "Delegates/IDelegateInstance.h"
#include "GameFramework/Actor.h"
#include "UObject/Object.h"
#include "UObject/ObjectKey.h"


#include "GeneratedNaniteDisplacedMeshEditorSubsystem.generated.h"

class UNaniteDisplacedMesh;
class UFactory;

struct FNaniteDisplacedMeshParams;
struct FPropertyChangedEvent;

template<typename Type>
class TSubclassOf;

UCLASS()
class NANITEDISPLACEDMESHEDITOR_API UGeneratedNaniteDisplacedMeshEditorSubsystem : public UEditorSubsystem, public FUObjectArray::FUObjectDeleteListener
{
	GENERATED_BODY()

	/**
	 * Utility functions to setup an automatic update of the level actors that have a generated NaniteDisplacedMesh constructed some assets data.
	 */
public:
	using FOnActorDependencyChanged = TUniqueFunction<void (AActor* /*ActorToUpdate*/, UObject* /*AssetChanged*/, FPropertyChangedEvent& /*PropertyChangedEvent*/)>;
	
	struct FActorClassHandler
	{
		FOnActorDependencyChanged Callback;

		// If empty it will accept any change
		TMap<UClass*, TSet<FProperty*>> PropertiesToWatchPerAssetType;
	};

	/**
	 * Tell the system what to callback when a dependency was changed for an actor the specified type.
	 */
	void RegisterClassHandler(const TSubclassOf<AActor>& ActorClass, FActorClassHandler&& ActorClassHandler);
	void UnregisterClassHandler(const TSubclassOf<AActor>& ActorClass);

	/**
	 * Tell the system to track for change to the dependencies of the actor.
	 * The system will invoke a callback after a change to any asset that this actor has a dependency.
	 */
	void UpdateActorDependencies(AActor* Actor, TArray<TObjectKey<UObject>>&& Dependencies);

	/**
	 * Tell the system to stop tracking stuff for this actor.
	 */
	void RemoveActor(const AActor* ActorToRemove);

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void OnObjectPreEditChange(UObject* Object, const class FEditPropertyChain& EditPropertyChain);
	void OnObjectPostEditChange(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent);
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap);
	void OnLevelActorDeleted(AActor* Actor);

	void OnAssetPreImport(UFactory* InFactory, UClass* InClass, UObject* InParent, const FName& Name, const TCHAR* Type);
	void OnAssetPostImport(UFactory* InFactory, UObject* InCreatedObject);

	bool CanObjectBeTracked(UObject* InObject);
	bool RemoveActor(TObjectKey<AActor> InActorToRemove, uint32 InWeakActorHash);

	FActorClassHandler* FindClassHandler(UClass* Class);
	bool ShouldCallback(UClass* AssetClass, const FActorClassHandler& ClassHandler, const FPropertyChangedEvent& PropertyChangedEvent);

	void UpdateDisplacedMeshesDueToAssetChanges(UObject* Asset);
	void UpdateDisplacementMeshToAssets(UNaniteDisplacedMesh* DisplacementMesh);
	void WaitForDependentDisplacedMeshesToFinishTheirCompilation(UObject* AssetAboutToChange);

private:
	// Track the change to asset for actors
	TMap<UClass*, FActorClassHandler> ActorClassHandlers;
	TMap<TObjectKey<AActor>, TArray<TObjectKey<UObject>>> ActorsToDependencies;
	TMap<TObjectKey<UObject>, TSet<TObjectKey<AActor>>> DependenciesToActors;

	// Track re import of asset for the displaced meshes
	struct FBidirectionalAssetsAndDisplacementMeshMap
	{
		void RemoveDisplacedMesh(UNaniteDisplacedMesh* DisplacedMesh);
		void RemoveAssetForReimportTracking(UObject* Object);

		void AddDisplacedMesh(UNaniteDisplacedMesh* Mesh, TSet<UObject*>&& AssetsToTrack);

		const TArray<TObjectKey<UNaniteDisplacedMesh>> GetMeshesThatUseAsset(UObject* Object);
		const TArray<TObjectKey<UNaniteDisplacedMesh>> GetMeshesThatUseAsset(UObject* Object, uint32 Hash);

		void ReplaceObject(UObject* OldObject, UObject* NewObject);
	private:
		TMap<UNaniteDisplacedMesh*, TSet<UObject*>> MeshToAssets;
		TMap<UObject*, TSet<UNaniteDisplacedMesh*>> AssetToMeshes;
	};

	FBidirectionalAssetsAndDisplacementMeshMap MeshesAndAssetsReimportTracking;

	bool bIsEngineCollectingGarbage = false;

	FDelegateHandle OnPreEditChangeHandle;
	FDelegateHandle OnPostEditChangeHandle;
	FDelegateHandle OnObjectsReplacedHandle;
	FDelegateHandle OnLevelActorDeletedHandle;
	FDelegateHandle OnAssetReimportHandle;
	FDelegateHandle OnAssetPostImportHandle;
	FDelegateHandle OnAssetPreImportHandle;
	FDelegateHandle OnInMemoryAssetDeletedHandle;
	FDelegateHandle OnPreGarbageCollectHandle;
	FDelegateHandle OnPostGarbageCollectHandle;

private:
	// Begin FUObjectArray::FUObjectDeleteListener Api
	virtual void NotifyUObjectDeleted(const UObjectBase *Object, int32 Index) override;
	virtual void OnUObjectArrayShutdown() override;
	// End FUObjectArray::FUObjectDeleteListener Api

	void UpdateIsEngineCollectingGarbage(bool bIsCollectingGarbage);
};