// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionEditorHash.h"

#if WITH_EDITOR
#include "Modules/ModuleManager.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionEditorHash)

UWorldPartitionEditorHash::UWorldPartitionEditorHash(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

#if WITH_EDITOR
UWorldPartitionEditorHash::FForEachIntersectingActorParams::FForEachIntersectingActorParams()
	: bIncludeSpatiallyLoadedActors(true)
	, bIncludeNonSpatiallyLoadedActors(true)
{}

int32 UWorldPartitionEditorHash::ForEachIntersectingActor(const FBox& Box, TFunctionRef<void(FWorldPartitionActorDesc*)> InOperation, bool bIncludeSpatiallyLoadedActors, bool bIncludeNonSpatiallyLoadedActors)
{
	UWorldPartitionEditorHash::FForEachIntersectingActorParams ForEachIntersectingActorParams = UWorldPartitionEditorHash::FForEachIntersectingActorParams()
		.SetIncludeSpatiallyLoadedActors(bIncludeSpatiallyLoadedActors)
		.SetIncludeNonSpatiallyLoadedActors(bIncludeNonSpatiallyLoadedActors);

	return ForEachIntersectingActor(Box, InOperation, ForEachIntersectingActorParams);
}
#endif
