// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionVolume.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionVolume)

ADEPRECATED_WorldPartitionVolume::ADEPRECATED_WorldPartitionVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}
