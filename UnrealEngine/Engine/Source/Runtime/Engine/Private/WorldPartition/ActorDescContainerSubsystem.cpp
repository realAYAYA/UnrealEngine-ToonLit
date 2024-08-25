// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/ActorDescContainerSubsystem.h"
#include "Engine/Engine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActorDescContainerSubsystem)

#if WITH_EDITOR
UActorDescContainerSubsystem* UActorDescContainerSubsystem::Get()
{
	if (!IsEngineExitRequested() && GEngine)
	{
		return GEngine->GetEngineSubsystem<UActorDescContainerSubsystem>();
	}

	return nullptr;
}

UActorDescContainerSubsystem& UActorDescContainerSubsystem::GetChecked()
{
	UActorDescContainerSubsystem* ContainerSubsystem = Get();
	check(ContainerSubsystem);
	return *Get();
}

void UActorDescContainerSubsystem::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UActorDescContainerSubsystem* This = CastChecked<UActorDescContainerSubsystem>(InThis);

	This->ContainerManager.AddReferencedObjects(Collector);
}

void UActorDescContainerSubsystem::FContainerManager::FRegisteredContainer::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Container);
}

void UActorDescContainerSubsystem::FContainerManager::FRegisteredContainer::UpdateBounds()
{
	Bounds.Init();
	for (FActorDescList::TIterator<> ActorDescIt(Container); ActorDescIt; ++ActorDescIt)
	{
		if (ActorDescIt->IsMainWorldOnly())
		{
			continue;
		}

		FWorldPartitionActorDesc* ActorDesc = Container->GetActorDesc(ActorDescIt->GetGuid());
		const FBox RuntimeBounds = ActorDesc->GetRuntimeBounds();
		if (RuntimeBounds.IsValid)
		{
			Bounds += RuntimeBounds;
		}
	}
}

void UActorDescContainerSubsystem::FContainerManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto& [Name, ContainerInstance] : RegisteredContainers)
	{
		ContainerInstance.AddReferencedObjects(Collector);
	}
}

void UActorDescContainerSubsystem::FContainerManager::UnregisterContainer(UActorDescContainer* Container)
{
	FString ContainerName = Container->GetContainerName();
	FRegisteredContainer& RegisteredContainer = RegisteredContainers.FindChecked(ContainerName);

	if (--RegisteredContainer.RefCount == 0)
	{
		RegisteredContainer.Container->Uninitialize();
		RegisteredContainers.FindAndRemoveChecked(ContainerName);
	}
}

FBox UActorDescContainerSubsystem::FContainerManager::GetContainerBounds(const FString& ContainerName) const
{
	if (const FRegisteredContainer* RegisteredContainer = RegisteredContainers.Find(ContainerName))
	{
		return RegisteredContainer->Bounds;
	}
	return FBox(ForceInit);
}

void UActorDescContainerSubsystem::FContainerManager::UpdateContainerBounds(const FString& ContainerName)
{
	if (FRegisteredContainer* RegisteredContainer = RegisteredContainers.Find(ContainerName))
	{
		RegisteredContainer->UpdateBounds();
	}
}

void UActorDescContainerSubsystem::FContainerManager::UpdateContainerBoundsFromPackage(FName ContainerPackage)
{
	for (auto& [ContainerName, RegisteredContainer] : RegisteredContainers)
	{
		if (RegisteredContainer.Container->GetContainerPackage() == ContainerPackage)
		{
			RegisteredContainer.UpdateBounds();
		}
	}
}

void UActorDescContainerSubsystem::FContainerManager::SetContainerPackage(UActorDescContainer* Container, FName PackageName)
{
	// Remove and Copy existing Registration (with previous ContainerName: ex: /Temp/Untitled)
	FRegisteredContainer RegisteredContainer;
	const bool bRegistered = RegisteredContainers.RemoveAndCopyValue(Container->GetContainerName(), RegisteredContainer);

	// Update Name
	Container->SetContainerPackage(PackageName);
		
	if(bRegistered)
	{
		// Check if we have an existing container registered with the new name, which means we are saving a map over another one
		FRegisteredContainer ReplacedContainer;
		if (RegisteredContainers.RemoveAndCopyValue(Container->GetContainerName(), ReplacedContainer))
		{
			// Move it out of the way with unique package name without losing the ref counts so that NotifyContainerReplaced listeners will be able to properly unregister
			FString ReplacePackageName = PackageName.ToString() + TEXT("_Replaced_") + FGuid::NewGuid().ToString();
			ReplacedContainer.Container->SetContainerPackage(*ReplacePackageName);
			RegisteredContainers.Add(ReplacedContainer.Container->GetContainerName(), ReplacedContainer);
		}
		
		check(RegisteredContainer.Container == Container);
		RegisteredContainers.Add(Container->GetContainerName(), RegisteredContainer);

		if (ReplacedContainer.Container)
		{
			Owner->NotifyContainerReplaced(ReplacedContainer.Container, RegisteredContainer.Container);
		}
	}
}
#endif