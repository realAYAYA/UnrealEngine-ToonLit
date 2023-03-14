// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"

#if WITH_EDITOR
#include "WorldPartition/ContentBundle/WorldDataLayerReference.h"
#include "WorldPartition/ContentBundle/ContentBundleBase.h"
#include "WorldPartition/Cook/WorldPartitionCookPackageGenerator.h"
#endif

#include "ContentBundleEditor.generated.h"

class AActor;

#if WITH_EDITOR

class URuntimeHashExternalStreamingObjectBase;
class UWorldPartitionRuntimeCell;

class ENGINE_API FContentBundleEditor : public FContentBundleBase, IWorldPartitionCookPackageGenerator
{
	friend class UContentBundleUnsavedActorMonitor;

public:
	FContentBundleEditor(TSharedPtr<FContentBundleClient>& InClient, UWorld* InWorld);

	//~ Begin IContentBundle Interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	virtual bool IsValid() const override;
	//~ End IContentBundle Interface

	const FGuid& GetGuid() const { return Guid; }

	void StartEditing();
	void StopEditing();
	bool IsBeingEdited() const { return bIsBeingEdited; }

	void InjectBaseContent();

	bool AddActor(AActor* InActor);
	bool ContainsActor(const AActor* InActor) const;
	bool GetActors(TArray<AActor*>& Actors);
	bool HasUserPlacedActors() const;
	uint32 GetActorCount() const;
	uint32 GetUnsavedActorAcount() const;

	void ReferenceAllActors();
	void UnreferenceAllActors();

	void GenerateStreaming(TArray<FString>* OutPackageToGenerate);

	URuntimeHashExternalStreamingObjectBase* GetStreamingObject() const { return ExternalStreamingObject; }

	void OnBeginCook(IWorldPartitionCookPackageContext& CookContext);

	bool GatherPackagesToCook(class IWorldPartitionCookPackageContext& CookContext);
	bool PopulateGeneratorPackageForCook(class IWorldPartitionCookPackageContext& CookContext, const TArray<FWorldPartitionCookPackage*>& PackagesToCook, TArray<UPackage*>& OutModifiedPackages);
	bool PopulateGeneratedPackageForCook(class IWorldPartitionCookPackageContext& CookContext, const FWorldPartitionCookPackage& PackagesToCook, TArray<UPackage*>& OutModifiedPackages);
	bool HasCookedContent() const { return ExternalStreamingObject != nullptr; }

protected:
	//~ Begin IContentBundle Interface
	virtual void DoInitialize() override;
	virtual void DoUninitialize() override;
	virtual void DoInjectContent() override;
	virtual void DoRemoveContent() override;
	//~ End IContentBundle Interface

private:
	void OnUnsavedActorDeleted(AActor* Actor);

	void BroadcastChanged();
	bool BuildContentBundleContainerPackagePath(FString& ContainerPackagePath) const;
	UPackage* CreateActorPackage(const FName& ActorName) const;
	FName BuildWorlDataLayersName() const;

	void RegisterDelegates();
	void UnregisterDelegates();
	
	void OnActorDescAdded(FWorldPartitionActorDesc* ActorDesc);
	void OnActorDescRemoved(FWorldPartitionActorDesc* ActorDesc);

	class UContentBundleUnsavedActorMonitor* UnsavedActorMonitor;

	TWeakObjectPtr<UActorDescContainer> ActorDescContainer;
	FWorldDataLayersReference WorldDataLayersActorReference;

	TArray<FWorldPartitionReference> ForceLoadedActors;

	TMap<uint32, const UWorldPartitionRuntimeCell*> CookPackageIdsToCell;
	URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject;

	FGuid Guid;

	bool bIsBeingEdited;
};

#endif

UCLASS()
class UContentBundleUnsavedActorMonitor : public UObject
{
	GENERATED_BODY()

public:
	~UContentBundleUnsavedActorMonitor();

#if WITH_EDITOR
	void Initialize(FContentBundleEditor& InContentBundleEditor);
	void Uninitialize();

	void StartListenOnActorEvents();
	void StopListeningOnActorEvents();

	void MonitorActor(AActor* InActor);
	bool StopMonitoringActor(AActor* InActor);

	uint32 GetActorCount() const { return UnsavedActors.Num(); }

	bool IsMonitoringActors() const { return !UnsavedActors.IsEmpty(); }
	bool IsMonitoring(const AActor* Actor) const;

	const TArray<TWeakObjectPtr<AActor>>& GetUnsavedActors() const { return UnsavedActors; }
#endif

private:
#if WITH_EDITOR
	void OnActorDeleted(AActor* InActor);
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> UnsavedActors;
#endif

#if WITH_EDITOR
	FContentBundleEditor* ContentBundle;
#endif
};