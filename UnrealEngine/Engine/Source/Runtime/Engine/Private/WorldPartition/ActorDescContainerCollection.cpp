// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartition/ActorDescContainerCollection.h"

#include "WorldPartition/ActorDescContainer.h"
#include "UObject/UObjectGlobals.h"
#include "WorldPartition/WorldPartition.h"

#if WITH_EDITOR

void FActorDescContainerCollection::AddContainer(UActorDescContainer* Container)
{
	ActorDescContainerCollection.Add(Container);
	Container->OnActorDescAddedEvent.AddRaw(this, &FActorDescContainerCollection::OnActorDescAdded);
	Container->OnActorDescRemovedEvent.AddRaw(this, &FActorDescContainerCollection::OnActorDescRemoved);
}

bool FActorDescContainerCollection::RemoveContainer(UActorDescContainer* Container)
{
	Container->OnActorDescRemovedEvent.RemoveAll(this);
	return ActorDescContainerCollection.RemoveSwap(Container) > 0;
}

bool FActorDescContainerCollection::Contains(const FName& ContainerPackageName) const
{
	 return Find(ContainerPackageName) != nullptr;
}

UActorDescContainer* FActorDescContainerCollection::Find(const FName& ContainerPackageName) const
{
	auto* ContainerPtr = ActorDescContainerCollection.FindByPredicate([&ContainerPackageName](const UActorDescContainer* ActorDescContainer) { return ActorDescContainer->GetContainerPackage() == ContainerPackageName; });
	return ContainerPtr != nullptr ? *ContainerPtr : nullptr;
}

const FWorldPartitionActorDesc* FActorDescContainerCollection::GetActorDesc(const FGuid& Guid) const
{
	const FWorldPartitionActorDesc* ActorDesc = nullptr;
	ForEachActorDescContainerBreakable([&Guid, &ActorDesc](const UActorDescContainer* ActorDescContainer)
	{
		ActorDesc =  ActorDescContainer->GetActorDesc(Guid);
		return ActorDesc == nullptr;
	});

	return ActorDesc;
}

FWorldPartitionActorDesc* FActorDescContainerCollection::GetActorDesc(const FGuid& Guid)
{
	return const_cast<FWorldPartitionActorDesc*>(const_cast<const FActorDescContainerCollection*>(this)->GetActorDesc(Guid));
}

const FWorldPartitionActorDesc& FActorDescContainerCollection::GetActorDescChecked(const FGuid& Guid) const
{
	const FWorldPartitionActorDesc* ActorDesc = GetActorDesc(Guid);
	check(ActorDesc != nullptr);

	static FWorldPartitionActorDesc EmptyDescriptor;
	return ActorDesc != nullptr ? *ActorDesc : EmptyDescriptor;
}

FWorldPartitionActorDesc& FActorDescContainerCollection::GetActorDescChecked(const FGuid& Guid)
{
	return const_cast<FWorldPartitionActorDesc&>(const_cast<const FActorDescContainerCollection*>(this)->GetActorDescChecked(Guid));
}

const FWorldPartitionActorDesc* FActorDescContainerCollection::GetActorDesc(const FString& ActorPath) const
{
	const FWorldPartitionActorDesc* ActorDesc = nullptr;
	ForEachActorDescContainerBreakable([&ActorPath, &ActorDesc](const UActorDescContainer* ActorDescContainer)
	{
		ActorDesc = ActorDescContainer->GetActorDesc(ActorPath);
		return ActorDesc == nullptr;
	});

	return ActorDesc;
}

const FWorldPartitionActorDesc* FActorDescContainerCollection::GetActorDesc(const FSoftObjectPath& InActorPath) const
{
	const FWorldPartitionActorDesc* ActorDesc = nullptr;
	ForEachActorDescContainerBreakable([&InActorPath, &ActorDesc](const UActorDescContainer* ActorDescContainer)
	{
		ActorDesc = ActorDescContainer->GetActorDesc(InActorPath);
		return ActorDesc == nullptr;
	});

	return ActorDesc;
}

bool FActorDescContainerCollection::RemoveActor(const FGuid& ActorGuid)
{
	bool bRemoved = false;
	ForEachActorDescContainerBreakable([&ActorGuid, &bRemoved](UActorDescContainer* ActorDescContainer)
	{
		bRemoved = ActorDescContainer->RemoveActor(ActorGuid);
		return !bRemoved;
	});

	return bRemoved;
}

void FActorDescContainerCollection::OnPackageDeleted(UPackage* Package)
{
	ForEachActorDescContainer([Package](UActorDescContainer* ActorDescContainer)
	{
		ActorDescContainer->OnPackageDeleted(Package);
	});
}

bool FActorDescContainerCollection::IsActorDescHandled(const AActor* Actor) const
{
	bool bIsHandled = false;
	ForEachActorDescContainerBreakable([Actor, &bIsHandled](UActorDescContainer* ActorDescContainer)
	{
		bIsHandled = ActorDescContainer->IsActorDescHandled(Actor);
		return !bIsHandled;
	});

	return bIsHandled;
}

void FActorDescContainerCollection::LoadAllActors(TArray<FWorldPartitionReference>& OutReferences)
{
	ForEachActorDescContainer([&OutReferences](UActorDescContainer* ActorDescContainer)
	{
		ActorDescContainer->LoadAllActors(OutReferences);
	});
}

const UActorDescContainer* FActorDescContainerCollection::GetActorDescContainer(const FGuid& ActorGuid) const
{
	UActorDescContainer* ActorDescContainer = nullptr;
	ForEachActorDescContainerBreakable([&ActorGuid, &ActorDescContainer](UActorDescContainer* InActorDescContainer)
	{
		if (InActorDescContainer->GetActorDesc(ActorGuid) != nullptr)
		{
			ActorDescContainer = InActorDescContainer;
		}
		return ActorDescContainer == nullptr;
	});

	return ActorDescContainer;
}

UActorDescContainer* FActorDescContainerCollection::GetActorDescContainer(const FGuid& ActorGuid)
{
	return const_cast<UActorDescContainer*>(const_cast<const FActorDescContainerCollection*>(this)->GetActorDescContainer(ActorGuid));
}

void FActorDescContainerCollection::ForEachActorDescContainerBreakable(TFunctionRef<bool(UActorDescContainer*)> Func) const
{
	for (UActorDescContainer* ActorDescContainer : ActorDescContainerCollection)
	{
		if (!Func(ActorDescContainer))
		{
			break;
		}
	}
}

void FActorDescContainerCollection::ForEachActorDescContainerBreakable(TFunctionRef<bool(UActorDescContainer*)> Func)
{
	const_cast<const FActorDescContainerCollection*>(this)->ForEachActorDescContainerBreakable(Func);
}

void FActorDescContainerCollection::ForEachActorDescContainer(TFunctionRef<void(UActorDescContainer*)> Func) const
{
	for (UActorDescContainer* ActorDescContainer : ActorDescContainerCollection)
	{
		Func(ActorDescContainer);
	}
}

void FActorDescContainerCollection::ForEachActorDescContainer(TFunctionRef<void(UActorDescContainer*)> Func)
{
	const_cast<const FActorDescContainerCollection*>(this)->ForEachActorDescContainer(Func);
}

void FActorDescContainerCollection::OnActorDescAdded(FWorldPartitionActorDesc* ActorDesc)
{
	OnActorDescAddedEvent.Broadcast(ActorDesc);
}

void FActorDescContainerCollection::OnActorDescRemoved(FWorldPartitionActorDesc* ActorDesc)
{
	OnActorDescRemovedEvent.Broadcast(ActorDesc);
}

#endif