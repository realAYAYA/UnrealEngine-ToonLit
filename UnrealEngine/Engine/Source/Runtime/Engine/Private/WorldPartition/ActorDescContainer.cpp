// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/ActorDescContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActorDescContainer)

#if WITH_EDITOR
#include "Editor.h"
#include "UObject/LinkerInstancingContext.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionLog.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/WorldPartitionActorDescUtils.h"
#include "UObject/ObjectSaveContext.h"
#include "Engine/World.h"

UActorDescContainer::FActorDescContainerInitializeDelegate UActorDescContainer::OnActorDescContainerInitialized;
#endif

UActorDescContainer::UActorDescContainer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, World(nullptr)
#if WITH_EDITOR
	, bContainerInitialized(false)
#endif
{}

void UActorDescContainer::Initialize(UWorld* InWorld, FName InPackageName)
{
	Initialize({ InWorld, InPackageName });
}

void UActorDescContainer::Initialize(const FInitializeParams& InitParams)
{
	check(!World || World == InitParams.World);
	World = InitParams.World;

#if WITH_EDITOR
	check(!bContainerInitialized);
	ContainerPackageName = InitParams.PackageName;
	TArray<FAssetData> Assets;

	if (!ContainerPackageName.IsNone())
	{
		const FString ContainerExternalActorsPath = GetExternalActorPath();

		// Do a synchronous scan of the level external actors path.			
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		AssetRegistry.ScanPathsSynchronous({ ContainerExternalActorsPath }, /*bForceRescan*/true, /*bIgnoreDenyListScanFilters*/false);

		FARFilter Filter;
		Filter.bRecursivePaths = true;
		Filter.bIncludeOnlyOnDiskAssets = true;
		Filter.PackagePaths.Add(*ContainerExternalActorsPath);

		AssetRegistry.GetAssets(Filter, Assets);
	}

	for (const FAssetData& Asset : Assets)
	{
		TUniquePtr<FWorldPartitionActorDesc> ActorDesc = FWorldPartitionActorDescUtils::GetActorDescriptorFromAssetData(Asset);

		if (ActorDesc.IsValid() && (!InitParams.FilterActorDesc || InitParams.FilterActorDesc(ActorDesc.Get())))
		{
			AddActorDescriptor(ActorDesc.Release());
		}
		else
		{
			InvalidActors.Emplace(MoveTemp(ActorDesc));
		}
	}

	OnActorDescContainerInitialized.Broadcast(this);

	RegisterEditorDelegates();

	bContainerInitialized = true;
#endif
}

void UActorDescContainer::Uninitialize()
{
#if WITH_EDITOR
	if (bContainerInitialized)
	{
		UnregisterEditorDelegates();
		bContainerInitialized = false;
	}

	for (TUniquePtr<FWorldPartitionActorDesc>& ActorDescPtr : ActorDescList)
	{
		if (FWorldPartitionActorDesc* ActorDesc = ActorDescPtr.Get())
		{
			RemoveActorDescriptor(ActorDesc);
		}
		ActorDescPtr.Reset();
	}
#endif
	World = nullptr;
}

UWorld* UActorDescContainer::GetWorld() const
{
	if (World)
	{
		return World;
	}
	return Super::GetWorld();
}

void UActorDescContainer::BeginDestroy()
{
	Super::BeginDestroy();

	Uninitialize();
}

#if WITH_EDITOR
FString UActorDescContainer::GetExternalActorPath() const
{
	return ULevel::GetExternalActorsPath(ContainerPackageName.ToString());
}

bool UActorDescContainer::IsActorDescHandled(const AActor* Actor) const
{
	if (Actor->GetContentBundleGuid() == GetContentBundleGuid())
	{
		const FString ActorPackageName = Actor->GetPackage()->GetName();
		const FString ExternalActorPath = GetExternalActorPath() / TEXT("");
		return ActorPackageName.StartsWith(ExternalActorPath);
	}
	return false;
}

bool UActorDescContainer::IsMainPartitionContainer() const
{
	return GetWorld() != nullptr && GetWorld()->GetWorldPartition()->GetActorDescContainer() == this;
}

void UActorDescContainer::AddActorDescriptor(FWorldPartitionActorDesc* ActorDesc)
{
	FActorDescList::AddActorDescriptor(ActorDesc);
	ActorDesc->SetContainer(this);
}

void UActorDescContainer::RemoveActorDescriptor(FWorldPartitionActorDesc* ActorDesc)
{
	ActorDesc->SetContainer(nullptr);
	FActorDescList::RemoveActorDescriptor(ActorDesc);
}

bool UActorDescContainer::ShouldHandleActorEvent(const AActor* Actor)
{
	if (Actor != nullptr)
	{
		bool bHandlesActor = GetActorDesc(Actor->GetActorGuid()) != nullptr || IsActorDescHandled(Actor);
		return bHandlesActor && Actor->IsMainPackageActor() && (Actor->GetLevel() != nullptr);
		
	}
	
	return false;
}

void UActorDescContainer::OnObjectPreSave(UObject* Object, FObjectPreSaveContext SaveContext)
{
	if (!SaveContext.IsProceduralSave() && !(SaveContext.GetSaveFlags() & SAVE_FromAutosave))
	{
		if (const AActor* Actor = Cast<AActor>(Object))
		{
			if (ShouldHandleActorEvent(Actor))
			{
				check(IsValidChecked(Actor));
				if (TUniquePtr<FWorldPartitionActorDesc>* ExistingActorDesc = GetActorDescriptor(Actor->GetActorGuid()))
				{
					// Existing actor
					OnActorDescUpdating(ExistingActorDesc->Get());
					FWorldPartitionActorDescUtils::UpdateActorDescriptorFomActor(Actor, *ExistingActorDesc);
					OnActorDescUpdated(ExistingActorDesc->Get());
				}
				else
				{
					// New actor
					FWorldPartitionActorDesc* const AddedActorDesc = AddActor(Actor);
					OnActorDescAdded(AddedActorDesc);
				}
			}
		}
	}
}

void UActorDescContainer::OnPackageDeleted(UPackage* Package)
{
	AActor* Actor = AActor::FindActorInPackage(Package);

	if (ShouldHandleActorEvent(Actor))
	{
		RemoveActor(Actor->GetActorGuid());
	}
}

void UActorDescContainer::OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewObjectMap)
{
	// Patch up Actor pointers in ActorDescs
	for (auto [OldObject, NewObject] : OldToNewObjectMap)
	{
		if (AActor* OldActor = Cast<AActor>(OldObject))
		{
			AActor* NewActor = Cast<AActor>(NewObject);
			if (FWorldPartitionActorDesc* ActorDesc = GetActorDesc(OldActor->GetActorGuid()))
			{				
				FWorldPartitionActorDescUtils::ReplaceActorDescriptorPointerFromActor(OldActor, NewActor, ActorDesc);
			}
		}
	}
}

bool UActorDescContainer::RemoveActor(const FGuid& ActorGuid)
{
	if (TUniquePtr<FWorldPartitionActorDesc>* ExistingActorDesc = GetActorDescriptor(ActorGuid))
	{
		OnActorDescRemoved(ExistingActorDesc->Get());
		RemoveActorDescriptor(ExistingActorDesc->Get());
		ExistingActorDesc->Reset();
		return true;
	}

	return false;
}

void UActorDescContainer::LoadAllActors(TArray<FWorldPartitionReference>& OutReferences)
{
	FWorldPartitionLoadingContext::FDeferred LoadingContext;
	OutReferences.Reserve(OutReferences.Num() + GetActorDescCount());
	for (FActorDescList::TIterator<> ActorDescIterator(this); ActorDescIterator; ++ActorDescIterator)
	{
		FWorldPartitionActorDesc* ActorDesc = *ActorDescIterator;
		OutReferences.Emplace(this, ActorDesc->GetGuid());
	}
}

bool UActorDescContainer::ShouldRegisterDelegates()
{
	UWorld* OuterWorld = GetTypedOuter<UWorld>();
	// No need to register delegates for level instances
	bool bIsInstance = OuterWorld && OuterWorld->IsInstanced() && !OuterWorld->GetPackage()->HasAnyPackageFlags(PKG_NewlyCreated);
	return GEditor && !IsTemplate() && World && !World->IsGameWorld() && !bIsInstance;
}

void UActorDescContainer::RegisterEditorDelegates()
{
	if (ShouldRegisterDelegates())
	{
		FCoreUObjectDelegates::OnObjectPreSave.AddUObject(this, &UActorDescContainer::OnObjectPreSave);
		FEditorDelegates::OnPackageDeleted.AddUObject(this, &UActorDescContainer::OnPackageDeleted);
		FCoreUObjectDelegates::OnObjectsReplaced.AddUObject(this, &UActorDescContainer::OnObjectsReplaced);
	}
}

void UActorDescContainer::UnregisterEditorDelegates()
{
	if (ShouldRegisterDelegates())
	{
		FCoreUObjectDelegates::OnObjectPreSave.RemoveAll(this);
		FEditorDelegates::OnPackageDeleted.RemoveAll(this);
		FCoreUObjectDelegates::OnObjectsReplaced.RemoveAll(this);
	}
}

void UActorDescContainer::OnActorDescAdded(FWorldPartitionActorDesc* NewActorDesc)
{
	OnActorDescAddedEvent.Broadcast(NewActorDesc);

	if (UWorldPartition* WorldPartition = GetTypedOuter<UWorldPartition>())
	{
		WorldPartition->OnActorDescAdded(NewActorDesc);
	}
}

void UActorDescContainer::OnActorDescRemoved(FWorldPartitionActorDesc* ActorDesc)
{
	OnActorDescRemovedEvent.Broadcast(ActorDesc);

	if (UWorldPartition* WorldPartition = GetTypedOuter<UWorldPartition>())
	{
		WorldPartition->OnActorDescRemoved(ActorDesc);
	}
}

void UActorDescContainer::OnActorDescUpdating(FWorldPartitionActorDesc* ActorDesc)
{
	if (UWorldPartition* WorldPartition = GetTypedOuter<UWorldPartition>())
	{
		WorldPartition->OnActorDescUpdating(ActorDesc);
	}
}

void UActorDescContainer::OnActorDescUpdated(FWorldPartitionActorDesc* ActorDesc)
{
	if (UWorldPartition* WorldPartition = GetTypedOuter<UWorldPartition>())
	{
		WorldPartition->OnActorDescUpdated(ActorDesc);
	}
}

const FLinkerInstancingContext* UActorDescContainer::GetInstancingContext() const
{
	const FLinkerInstancingContext* InstancingContext = nullptr;

	if (UWorldPartition* WorldPartition = GetTypedOuter<UWorldPartition>())
	{
		WorldPartition->GetInstancingContext(InstancingContext);
	}

	return InstancingContext;
}

const FTransform& UActorDescContainer::GetInstanceTransform() const
{
	if (UWorldPartition* WorldPartition = GetTypedOuter<UWorldPartition>())
	{
		return WorldPartition->GetInstanceTransform();
	}

	return FTransform::Identity;
}
#endif

