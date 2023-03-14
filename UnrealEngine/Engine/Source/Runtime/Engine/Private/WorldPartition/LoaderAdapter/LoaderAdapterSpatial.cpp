// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/LoaderAdapter/LoaderAdapterSpatial.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionEditorHash.h"
#include "Engine/World.h"

#if WITH_EDITOR
ILoaderAdapterSpatial::ILoaderAdapterSpatial(UWorld* InWorld)
	: ILoaderAdapter(InWorld)
	, bIncludeSpatiallyLoadedActors(true)
	, bIncludeNonSpatiallyLoadedActors(false)
{}

void ILoaderAdapterSpatial::ForEachActor(TFunctionRef<void(const FWorldPartitionHandle&)> InOperation) const
{
	if (UWorldPartition* WorldPartition = GetWorld()->GetWorldPartition())
	{
		UWorldPartitionEditorHash::FForEachIntersectingActorParams ForEachIntersectingActorParams;
		ForEachIntersectingActorParams.bIncludeSpatiallyLoadedActors = bIncludeSpatiallyLoadedActors;
		ForEachIntersectingActorParams.bIncludeNonSpatiallyLoadedActors = bIncludeNonSpatiallyLoadedActors;

		WorldPartition->EditorHash->ForEachIntersectingActor(*GetBoundingBox(), [this, WorldPartition, &InOperation](FWorldPartitionActorDesc* ActorDesc)
		{
			if (Intersect(ActorDesc->GetBounds()))
			{
				FWorldPartitionHandle ActorHandle(WorldPartition, ActorDesc->GetGuid());
				InOperation(ActorHandle);
			}
		}, ForEachIntersectingActorParams);
	}
}
#endif
