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
		UE_DEPRECATED(5.4, "Use constructor with no UWorld param instead")
		FInitializeParams(UWorld* InWorld, FName InPackageName)
			: PackageName(InPackageName)
			, ContainerName(InPackageName.ToString())
		{}

		FInitializeParams(FName InPackageName)
			: PackageName(InPackageName)
			, ContainerName(InPackageName.ToString())
		{}

		FInitializeParams(const FString& InContainerName, FName InPackageName)
			: PackageName(InPackageName)
			, ContainerName(InContainerName)
		{}
		
		/* The long package name of the container package on disk. */
		FName PackageName;

		/* The unique name for the container : defaults to PackageName */
		FString ContainerName;

		/** The associated Content Bundle Guid */
		FGuid ContentBundleGuid;

		/** The associated External Data Layer Asset */
		const UExternalDataLayerAsset* ExternalDataLayerAsset = nullptr;

		/* Custom filter function used to filter actors descriptors. */
		TUniqueFunction<bool(const FWorldPartitionActorDesc*)> FilterActorDesc;
	};

	ENGINE_API void Initialize(const FInitializeParams& InitParams);
	ENGINE_API void Uninitialize();

	bool IsInitialized() const { return bContainerInitialized; }

	ENGINE_API void OnObjectPreSave(UObject* Object, FObjectPreSaveContext SaveContext);
	ENGINE_API void OnPackageDeleted(UPackage* Package);
	ENGINE_API void OnClassDescriptorUpdated(const FWorldPartitionActorDesc* InClassDesc);

	virtual FString GetContainerName() const { return ContainerPackageName.ToString(); }
	FName GetContainerPackage() const { return ContainerPackageName; }
	void SetContainerPackage(const FName& InContainerPackageName) { ContainerPackageName = InContainerPackageName; }

	const UExternalDataLayerAsset* GetExternalDataLayerAsset() const { return ExternalDataLayerAsset; }
	bool HasExternalContent() const;

	FGuid GetContentBundleGuid() const { return ContentBundleGuid; }

	ENGINE_API FString GetExternalActorPath() const;
	ENGINE_API FString GetExternalObjectPath() const;

	/** Removes an actor desc without the need to load a package */
	ENGINE_API bool RemoveActor(const FGuid& ActorGuid);

	ENGINE_API bool IsActorDescHandled(const AActor* Actor) const;

	DECLARE_EVENT_OneParam(UActorDescContainer, FActorDescAddedEvent, FWorldPartitionActorDesc*);
	FActorDescAddedEvent OnActorDescAddedEvent;
	
	DECLARE_EVENT_OneParam(UActorDescContainer, FActorDescRemovedEvent, FWorldPartitionActorDesc*);
	FActorDescRemovedEvent OnActorDescRemovedEvent;

	DECLARE_EVENT_OneParam(UActorDescContainer, FActorDescUpdatingEvent, FWorldPartitionActorDesc*);
	FActorDescUpdatingEvent OnActorDescUpdatingEvent;

	DECLARE_EVENT_OneParam(UActorDescContainer, FActorDescUpdatedEvent, FWorldPartitionActorDesc*);
	FActorDescUpdatedEvent OnActorDescUpdatedEvent;

	DECLARE_MULTICAST_DELEGATE_OneParam(FActorDescContainerInitializeDelegate, UActorDescContainer*);
	static ENGINE_API FActorDescContainerInitializeDelegate OnActorDescContainerInitialized;
	
	bool HasInvalidActors() const { return InvalidActors.Num() > 0; }
	const TArray<FAssetData>& GetInvalidActors() const { return InvalidActors; }
	void ClearInvalidActors() { InvalidActors.Empty(); }

	ENGINE_API void RegisterActorDescriptor(FWorldPartitionActorDesc* ActorDesc);
	ENGINE_API void UnregisterActorDescriptor(FWorldPartitionActorDesc* ActorDesc);

	ENGINE_API void OnActorDescAdded(FWorldPartitionActorDesc* NewActorDesc);
	ENGINE_API void OnActorDescRemoved(FWorldPartitionActorDesc* ActorDesc);
	ENGINE_API void OnActorDescUpdating(FWorldPartitionActorDesc* ActorDesc);
	ENGINE_API void OnActorDescUpdated(FWorldPartitionActorDesc* ActorDesc);

	ENGINE_API bool ShouldHandleActorEvent(const AActor* Actor);

	ENGINE_API const FWorldPartitionActorDesc* GetActorDescByPath(const FString& ActorPath) const;
	ENGINE_API const FWorldPartitionActorDesc* GetActorDescByPath(const FSoftObjectPath& ActorPath) const;
	ENGINE_API const FWorldPartitionActorDesc* GetActorDescByName(FName ActorName) const;

	bool bContainerInitialized;

	FName ContainerPackageName;
	FGuid ContentBundleGuid;

	TArray<FAssetData> InvalidActors;

	//~ Begin Deprecation
	UE_DEPRECATED(5.4, "UActorDescContainer::Update is deprecated.")
	ENGINE_API void Update() {}

	UE_DEPRECATED(5.4, "Use UActorDescContainerInstance::GetInstancingContext instead")
	ENGINE_API const FLinkerInstancingContext* GetInstancingContext() const { return nullptr; }
	
	UE_DEPRECATED(5.4, "Use UActorDescContainerInstance::GetInstanceTransform instead")
	ENGINE_API FTransform GetInstanceTransform() const { return FTransform::Identity; }

	UE_DEPRECATED(5.4, "Use UActorDescContainerInstance::LoadAllActors instead")
	ENGINE_API void LoadAllActors(TArray<FWorldPartitionReference>& OutReferences) {}

	UE_DEPRECATED(5.4, "Use UActorDescContainerInstance::OnObjectsReplaced instead")
	ENGINE_API void OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewObjectMap) {}

	UE_DEPRECATED(5.4, "UActorDescContainer::IsTemplateContainer is deprecated")
	ENGINE_API bool IsTemplateContainer() const { return true;}

	UE_DEPRECATED(5.4, "UActorDescContainer::IsMainWorldPartitionContainer is deprecated")
	ENGINE_API bool IsMainPartitionContainer() const { return false; }
	
	UE_DEPRECATED(5.4, "Use UActorDescContainerInstance::GetWorldPartition instead")
	ENGINE_API UWorldPartition* GetWorldPartition() const { return nullptr; }

	UE_DEPRECATED(5.4, "Use version without UWorld parameter")
	ENGINE_API void RegisterActorDescriptor(FWorldPartitionActorDesc* ActorDesc, UWorld* InWorldContext) {}
	//~ End Deprecation

protected:
	FNameActorDescMap ActorsByName;

	//~ Begin UObject Interface
	ENGINE_API virtual void BeginDestroy() override;
	//~ End UObject Interface

private:
	// GetWorld() should never be called on an ActorDescContainer to avoid any confusion as it can be used as a template
	UWorld* GetWorld() const override { return nullptr; }

	ENGINE_API bool ShouldRegisterDelegates();
	ENGINE_API void RegisterEditorDelegates();
	ENGINE_API void UnregisterEditorDelegates();

	TObjectPtr<const UExternalDataLayerAsset> ExternalDataLayerAsset;
#endif
};
