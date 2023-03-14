// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartition/ContentBundle/ContentBundleWorldSubsystem.h"

#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/ContentBundle/ContentBundle.h"
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "WorldPartition/ContentBundle/ContentBundleEngineSubsystem.h"
#include "WorldPartition/ContentBundle/ContentBundleClient.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ContentBundleWorldSubsystem)


#if WITH_EDITOR
#include "WorldPartition/ContentBundle/ContentBundleEditor.h"
#include "Editor.h"
#endif

UContentBundleManager::UContentBundleManager()
{

}

void UContentBundleManager::Initialize()
{
#if WITH_EDITOR
	if (!GetWorld()->IsGameWorld())
	{
		PIEDuplicateHelper = NewObject<UContentBundleDuplicateForPIEHelper>(this);
		PIEDuplicateHelper->Initialize();
	}
#endif

	GetWorld()->OnWorldPartitionInitialized().AddUObject(this, &UContentBundleManager::OnWorldPartitionInitialized);
	GetWorld()->OnWorldPartitionUninitialized().AddUObject(this, &UContentBundleManager::OnWorldPartitionUninitialized);
}

void UContentBundleManager::Deinitialize()
{
	GetWorld()->OnWorldPartitionInitialized().RemoveAll(this);
	GetWorld()->OnWorldPartitionUninitialized().RemoveAll(this);

	for (TUniquePtr<FContentBundleContainer>& ContentBundleContainer : ContentBundleContainers)
	{
		ContentBundleContainer->Deinitialize();
	}
	ContentBundleContainers.Empty();

#if WITH_EDITOR
	if(PIEDuplicateHelper != nullptr)
	{
		PIEDuplicateHelper->Deinitialize();
		PIEDuplicateHelper = nullptr;
	}
#endif
}

void UContentBundleManager::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UContentBundleManager* Subsystem = CastChecked<UContentBundleManager>(InThis);
	for (TUniquePtr<FContentBundleContainer>& ContentBundleContainer : Subsystem->ContentBundleContainers)
	{
		ContentBundleContainer->AddReferencedObjects(Collector);
	}
}

#if WITH_EDITOR

bool UContentBundleManager::GetEditorContentBundle(TArray<TSharedPtr<FContentBundleEditor>>& OutContentBundles)
{
	for (TUniquePtr<FContentBundleContainer>& ContentBundleContainer : ContentBundleContainers)
	{
		OutContentBundles.Append(ContentBundleContainer->GetEditorContentBundles());
	}

	return !OutContentBundles.IsEmpty();
}

TSharedPtr<FContentBundleEditor> UContentBundleManager::GetEditorContentBundle(const UContentBundleDescriptor* Descriptor, const UWorld* ContentBundleWorld) const
{
	for (const TUniquePtr<FContentBundleContainer>& ContentBundleContainer : ContentBundleContainers)
	{
		if (ContentBundleContainer->GetInjectedWorld() == ContentBundleWorld)
		{
			return ContentBundleContainer->GetEditorContentBundle(Descriptor);
		}
	}

	return nullptr;
}

#endif

uint32 UContentBundleManager::GetContentBundleContainerIndex(const UWorld* InjectedWorld)
{
	return ContentBundleContainers.IndexOfByPredicate([InjectedWorld](const TUniquePtr<FContentBundleContainer>& ContentBundleContainer)
		{
			return ContentBundleContainer->GetInjectedWorld() == InjectedWorld;
		});
}

TUniquePtr<FContentBundleContainer>* UContentBundleManager::GetContentBundleContainer(const UWorld* InjectedWorld)
{
	uint32 ContainerIndex = GetContentBundleContainerIndex(InjectedWorld);
	if (ContainerIndex != INDEX_NONE)
	{
		return &ContentBundleContainers[ContainerIndex];
	}

	return nullptr;
}

void UContentBundleManager::OnWorldPartitionInitialized(UWorldPartition* InWorldPartition)
{
	if (InWorldPartition->GetWorld() != InWorldPartition->GetTypedOuter<UWorld>())
	{
		return;
	}

	check(GetContentBundleContainer(InWorldPartition->GetTypedOuter<UWorld>()) == nullptr);
	TUniquePtr<FContentBundleContainer>& ContentBundleContainer = ContentBundleContainers.Emplace_GetRef(MakeUnique<FContentBundleContainer>(InWorldPartition->GetTypedOuter<UWorld>()));
	ContentBundleContainer->Initialize();
}

void UContentBundleManager::OnWorldPartitionUninitialized(UWorldPartition* InWorldPartition)
{
	if (InWorldPartition->GetWorld() != InWorldPartition->GetTypedOuter<UWorld>())
	{
		return;
	}

	uint32 ContainerIndex = GetContentBundleContainerIndex(InWorldPartition->GetTypedOuter<UWorld>());
	check(ContainerIndex != INDEX_NONE);
	ContentBundleContainers[ContainerIndex]->Deinitialize();
	ContentBundleContainers.RemoveAtSwap(ContainerIndex);
}

#if WITH_EDITOR

void UContentBundleDuplicateForPIEHelper::Initialize()
{
	FEditorDelegates::EndPIE.AddUObject(this, &UContentBundleDuplicateForPIEHelper::OnPIEEnded);
}

void UContentBundleDuplicateForPIEHelper::Deinitialize()
{
	FEditorDelegates::EndPIE.RemoveAll(this);

	Clear();
}

bool UContentBundleDuplicateForPIEHelper::StoreContentBundleStreamingObect(const FContentBundleEditor& ContentBundleEditor, URuntimeHashExternalStreamingObjectBase* StreamingObject)
{
	FGuid ContentBundleUid = ContentBundleEditor.GetDescriptor()->GetGuid();
	if (!StreamingObjects.Contains(ContentBundleUid))
	{
		StreamingObjects.Add(ContentBundleUid, StreamingObject);
	}
	
	return true;
}

URuntimeHashExternalStreamingObjectBase* UContentBundleDuplicateForPIEHelper::RetrieveContentBundleStreamingObject(const FContentBundle& ContentBundle) const
{
	const TObjectPtr<URuntimeHashExternalStreamingObjectBase>* ContentBundleStreamingObject = StreamingObjects.Find(ContentBundle.GetDescriptor()->GetGuid());
	return ContentBundleStreamingObject != nullptr ? *ContentBundleStreamingObject : nullptr;
}

void UContentBundleDuplicateForPIEHelper::OnPIEEnded(const bool bIsSimulating)
{
	Clear();
}

#endif
