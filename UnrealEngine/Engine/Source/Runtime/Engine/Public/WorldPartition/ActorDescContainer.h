// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "WorldPartition/ActorDescList.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "AssetRegistry/AssetData.h"
#include "ActorDescContainer.generated.h"

class FLinkerInstancingContext;
class UWorldPartition;

UCLASS(MinimalAPI)
class UActorDescContainer : public UObject, public FActorDescList
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	friend struct FWorldPartitionHandleUtils;
	friend class FWorldPartitionActorDesc;

	using FNameActorDescMap = TMap<FName, TUniquePtr<FWorldPartitionActorDesc>*>;

public:
	/* Struct of parameters passed to Initialize function. */
	struct FInitializeParams
	{
		FInitializeParams(UWorld* InWorld, FName InPackageName)
			: World(InWorld)
			, PackageName(InPackageName)
		{}

		/* The world the actor descriptor container is associated with. */
		UWorld* World;
			
		/* The long package name of the container package on disk. */
		FName PackageName;

		/* Custom filter function used to filter actors descriptors. */
		TUniqueFunction<bool(const FWorldPartitionActorDesc*)> FilterActorDesc;
	};

	UE_DEPRECATED(5.1, "UActorDescContainer::Initialize is deprecated, UActorDescContainer::Initialize with UActorDescContainer::FInitializeParams should be used instead.")
	ENGINE_API void Initialize(UWorld* InWorld, FName InPackageName);
	ENGINE_API void Initialize(const FInitializeParams& InitParams);
	ENGINE_API void Update();
	ENGINE_API void Uninitialize();

	bool IsInitialized() const { return bContainerInitialized; }

	ENGINE_API void OnObjectPreSave(UObject* Object, FObjectPreSaveContext SaveContext);
	ENGINE_API void OnPackageDeleted(UPackage* Package);
	ENGINE_API void OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewObjectMap);
	ENGINE_API void OnClassDescriptorUpdated(const FWorldPartitionActorDesc* InClassDesc);

	FName GetContainerPackage() const { return ContainerPackageName; }
	void SetContainerPackage(const FName& InContainerPackageName) { ContainerPackageName = InContainerPackageName; }

	FGuid GetContentBundleGuid() const { return ContentBundleGuid; }
	void SetContentBundleGuid(const FGuid& InGetContentBundleGuid) { ContentBundleGuid = InGetContentBundleGuid; }

	ENGINE_API bool IsTemplateContainer() const;
	ENGINE_API bool IsMainPartitionContainer() const;
	ENGINE_API UWorldPartition* GetWorldPartition() const;

	ENGINE_API FString GetExternalActorPath() const;

	/** Removes an actor desc without the need to load a package */
	ENGINE_API bool RemoveActor(const FGuid& ActorGuid);

	ENGINE_API void LoadAllActors(TArray<FWorldPartitionReference>& OutReferences);

	ENGINE_API bool IsActorDescHandled(const AActor* Actor) const;

	DECLARE_EVENT_OneParam(UWorldPartition, FActorDescAddedEvent, FWorldPartitionActorDesc*);
	FActorDescAddedEvent OnActorDescAddedEvent;
	
	DECLARE_EVENT_OneParam(UWorldPartition, FActorDescRemovedEvent, FWorldPartitionActorDesc*);
	FActorDescRemovedEvent OnActorDescRemovedEvent;

	DECLARE_MULTICAST_DELEGATE_OneParam(FActorDescContainerInitializeDelegate, UActorDescContainer*);
	static ENGINE_API FActorDescContainerInitializeDelegate OnActorDescContainerInitialized;

	ENGINE_API const FLinkerInstancingContext* GetInstancingContext() const;
	ENGINE_API const FTransform& GetInstanceTransform() const;

	bool HasInvalidActors() const { return InvalidActors.Num() > 0; }
	const TArray<FAssetData>& GetInvalidActors() const { return InvalidActors; }
	void ClearInvalidActors() { InvalidActors.Empty(); }

	ENGINE_API void RegisterActorDescriptor(FWorldPartitionActorDesc* ActorDesc, UWorld* InWorldContext);
	ENGINE_API void UnregisterActorDescriptor(FWorldPartitionActorDesc* ActorDesc);

	ENGINE_API void OnActorDescAdded(FWorldPartitionActorDesc* NewActorDesc);
	ENGINE_API void OnActorDescRemoved(FWorldPartitionActorDesc* ActorDesc);
	ENGINE_API void OnActorDescUpdating(FWorldPartitionActorDesc* ActorDesc);
	ENGINE_API void OnActorDescUpdated(FWorldPartitionActorDesc* ActorDesc);

	ENGINE_API bool ShouldHandleActorEvent(const AActor* Actor);

	ENGINE_API const FWorldPartitionActorDesc* GetActorDescByName(const FString& ActorPath) const;
	ENGINE_API const FWorldPartitionActorDesc* GetActorDescByName(const FSoftObjectPath& InActorPath) const;

	bool bContainerInitialized;

	FName ContainerPackageName;
	FGuid ContentBundleGuid;

	TArray<FAssetData> InvalidActors;

protected:
	FNameActorDescMap ActorsByName;

	//~ Begin UObject Interface
	ENGINE_API virtual void BeginDestroy() override;
	//~ End UObject Interface

private:
	// GetWorld() should never be called on an ActorDescContainer to avoid any confusion as it can be used as a template
	virtual UWorld* GetWorld() const override { return nullptr; }

	ENGINE_API bool ShouldRegisterDelegates();
	ENGINE_API void RegisterEditorDelegates();
	ENGINE_API void UnregisterEditorDelegates();
#endif
};
