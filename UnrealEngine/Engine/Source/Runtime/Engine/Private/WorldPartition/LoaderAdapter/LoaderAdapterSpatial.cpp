// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/LoaderAdapter/LoaderAdapterSpatial.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionEditorHash.h"
#include "Engine/Level.h"
#include "Engine/World.h"

#if WITH_EDITOR
ILoaderAdapterSpatial::ILoaderAdapterSpatial(UWorld* InWorld)
	: ILoaderAdapter(InWorld)
	, bIncludeSpatiallyLoadedActors(true)
	, bIncludeNonSpatiallyLoadedActors(false)
{}

void ILoaderAdapterSpatial::ForEachActor(TFunctionRef<void(const FWorldPartitionHandle&)> InOperation) const
{
	if (GetBoundingBox().IsSet())
	{
		if (UWorldPartition* WorldPartition = GetWorld()->GetWorldPartition())
		{
			HandleIntersectingContainer(WorldPartition, WorldPartition->GetInstanceTransform(), *GetBoundingBox(), InOperation);
		}
	}
}

void ILoaderAdapterSpatial::HandleIntersectingContainer(UWorldPartition* InWorldPartition, const FTransform InInstanceTransform, const FBox& InBoundingBox, TFunctionRef<void(const FWorldPartitionHandle&)> InOperation) const
{
	UWorldPartitionEditorHash::FForEachIntersectingActorParams ForEachIntersectingActorParams = UWorldPartitionEditorHash::FForEachIntersectingActorParams()
		.SetIncludeSpatiallyLoadedActors(bIncludeSpatiallyLoadedActors)
		.SetIncludeNonSpatiallyLoadedActors(bIncludeNonSpatiallyLoadedActors);

	const FBox LocalBoundingBox = InBoundingBox.InverseTransformBy(InInstanceTransform);
	InWorldPartition->EditorHash->ForEachIntersectingActor(LocalBoundingBox, [this, InWorldPartition, &InInstanceTransform, &InOperation](FWorldPartitionActorDesc* ActorDesc)
	{
		const FBox WorldActorEditorBox = ActorDesc->GetEditorBounds().TransformBy(InInstanceTransform);
		if (Intersect(WorldActorEditorBox))
		{
			FWorldPartitionHandle ActorHandle(InWorldPartition, ActorDesc->GetGuid());
			InOperation(ActorHandle);

			if (ActorHandle->GetIsSpatiallyLoaded() && ActorHandle->IsContainerInstance())
			{
				FWorldPartitionActorDesc::FContainerInstance ContainerInstance;
				if (ActorHandle->GetContainerInstance(ContainerInstance))
				{
					if (UWorldPartition* ContainerWorldPartition = ContainerInstance.LoadedLevel ? ContainerInstance.LoadedLevel->GetWorldPartition() : nullptr)
					{
						const FBox InnerBoundingBox = ContainerInstance.bSupportsPartialEditorLoading ? *GetBoundingBox() : FBox(FVector(-HALF_WORLD_MAX), FVector(HALF_WORLD_MAX));
						HandleIntersectingContainer(ContainerWorldPartition, ContainerWorldPartition->GetInstanceTransform(), InnerBoundingBox, InOperation);
					}
				}
			}
		}
	}, ForEachIntersectingActorParams);
}
#endif
