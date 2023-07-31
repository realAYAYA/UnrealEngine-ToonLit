// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionMiniMapVolume.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionMiniMapVolume)

AWorldPartitionMiniMapVolume::AWorldPartitionMiniMapVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.DoNotCreateDefaultSubobject(TEXT("Sprite")))
{
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}
