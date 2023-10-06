// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/LoaderAdapter/LoaderAdapterList.h"
#include "WorldPartition/ActorDescContainerCollection.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "Engine/Level.h"

#if WITH_EDITOR
FLoaderAdapterList::FLoaderAdapterList(UWorld* InWorld)
	: ILoaderAdapter(InWorld)
{}

void FLoaderAdapterList::ForEachActor(TFunctionRef<void(const FWorldPartitionHandle&)> InOperation) const
{
	for (const FWorldPartitionHandle& Actor : Actors)
	{
		if (Actor.IsValid())
		{
			InOperation(Actor);
			HandleActorContainer(Actor, InOperation);
		}
	}
}

void FLoaderAdapterList::HandleActorContainer(const FWorldPartitionHandle& InActor, TFunctionRef<void(const FWorldPartitionHandle&)> InOperation) const
{
	if (InActor->IsContainerInstance())
	{
		FWorldPartitionActorDesc::FContainerInstance ContainerInstance;
		if (InActor->GetContainerInstance(ContainerInstance))
		{
			if (ContainerInstance.bSupportsPartialEditorLoading)
			{
				if (UWorldPartition* ContainerWorldPartition = ContainerInstance.LoadedLevel ? ContainerInstance.LoadedLevel->GetWorldPartition() : nullptr)
				{
					for (FActorDescContainerCollection::TIterator<> ActorDescIterator(ContainerWorldPartition); ActorDescIterator; ++ActorDescIterator)
					{
						FWorldPartitionHandle ActorHandle(ContainerWorldPartition, ActorDescIterator->GetGuid());
						InOperation(ActorHandle);
						HandleActorContainer(ActorHandle, InOperation);
					}
				}
			}
		}
	}
}
#endif
