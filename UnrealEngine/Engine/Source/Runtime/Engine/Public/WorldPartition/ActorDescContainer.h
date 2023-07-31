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

UCLASS()
class ENGINE_API UActorDescContainer : public UObject, public FActorDescList
{
	GENERATED_UCLASS_BODY()

	friend struct FWorldPartitionHandleUtils;
	friend class FWorldPartitionActorDesc;

public:
	/* Struct of parameters passed to Initialize function. */
	struct ENGINE_API FInitializeParams
	{
		FInitializeParams(UWorld* InWorld, FName InPackageName)
			: World(InWorld)
			, PackageName(InPackageName)
		{}

		/* The world the actor descriptor container is associated with. */
		UWorld* World;
			
		/* The long package name of the container package on disk. */
		FName PackageName;

#if WITH_EDITOR
		/* Custom filter function used to filter actors descriptors. */
		TUniqueFunction<bool(const FWorldPartitionActorDesc*)> FilterActorDesc;
#endif
	};

	UE_DEPRECATED(5.1, "UActorDescContainer::Initialize is deprecated, UActorDescContainer::Initialize with UActorDescContainer::FInitializeParams should be used instead.")
	void Initialize(UWorld* World, FName InPackageName);
	void Initialize(const FInitializeParams& InitParams);
	void Uninitialize();
	
	virtual UWorld* GetWorld() const override;

#if WITH_EDITOR
	bool IsInitialized() const { return bContainerInitialized; }

	void OnObjectPreSave(UObject* Object, FObjectPreSaveContext SaveContext);
	void OnPackageDeleted(UPackage* Package);
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewObjectMap);

	FName GetContainerPackage() const { return ContainerPackageName; }
	void SetContainerPackage(const FName& InContainerPackageName) { ContainerPackageName = InContainerPackageName; }

	FGuid GetContentBundleGuid() const { return ContentBundleGuid; }
	void SetContentBundleGuid(const FGuid& InGetContentBundleGuid) { ContentBundleGuid = InGetContentBundleGuid; }

	bool IsMainPartitionContainer() const;

	FString GetExternalActorPath() const;

	/** Removes an actor desc without the need to load a package */
	bool RemoveActor(const FGuid& ActorGuid);

	void LoadAllActors(TArray<FWorldPartitionReference>& OutReferences);

	bool IsActorDescHandled(const AActor* Actor) const;
#endif

#if WITH_EDITOR
public:
	DECLARE_EVENT_OneParam(UWorldPartition, FActorDescAddedEvent, FWorldPartitionActorDesc*);
	FActorDescAddedEvent OnActorDescAddedEvent;
	
	DECLARE_EVENT_OneParam(UWorldPartition, FActorDescRemovedEvent, FWorldPartitionActorDesc*);
	FActorDescRemovedEvent OnActorDescRemovedEvent;

	DECLARE_MULTICAST_DELEGATE_OneParam(FActorDescContainerInitializeDelegate, UActorDescContainer*);
	static FActorDescContainerInitializeDelegate OnActorDescContainerInitialized;

	const FLinkerInstancingContext* GetInstancingContext() const;
	const FTransform& GetInstanceTransform() const;

	bool HasInvalidActors() const { return InvalidActors.Num() > 0; }
	const TArray<TUniquePtr<FWorldPartitionActorDesc>>& GetInvalidActors() const { return InvalidActors; }
	void ClearInvalidActors() { InvalidActors.Empty(); }
#endif

	UPROPERTY(Transient)
	TObjectPtr<UWorld> World;

protected:
	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	//~ End UObject Interface

#if WITH_EDITOR
	//~ Begin FActorDescList Interface
	virtual void AddActorDescriptor(FWorldPartitionActorDesc* ActorDesc) override;
	virtual void RemoveActorDescriptor(FWorldPartitionActorDesc* ActorDesc) override;
	//~ End FActorDescList Interface

	void OnActorDescAdded(FWorldPartitionActorDesc* NewActorDesc);
	void OnActorDescRemoved(FWorldPartitionActorDesc* ActorDesc);
	void OnActorDescUpdating(FWorldPartitionActorDesc* ActorDesc);
	void OnActorDescUpdated(FWorldPartitionActorDesc* ActorDesc);

	bool ShouldHandleActorEvent(const AActor* Actor);

	bool bContainerInitialized;

	FName ContainerPackageName;
	FGuid ContentBundleGuid;

	TArray<TUniquePtr<FWorldPartitionActorDesc>> InvalidActors;
#endif

private:
#if WITH_EDITOR
	bool ShouldRegisterDelegates();
	void RegisterEditorDelegates();
	void UnregisterEditorDelegates();
#endif
};