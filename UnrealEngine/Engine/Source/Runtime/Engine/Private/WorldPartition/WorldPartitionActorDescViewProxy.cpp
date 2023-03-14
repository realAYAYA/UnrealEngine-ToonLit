// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionActorDescViewProxy.h"
#include "GameFramework/Actor.h"

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionActorDesc.h"

FWorldPartitionActorViewProxy::FWorldPartitionActorViewProxy(const FWorldPartitionActorDesc* InActorDesc)
	: FWorldPartitionActorDescView(InActorDesc)
{
	if (AActor* Actor = ActorDesc->GetActor())
	{
		if (Actor->GetPackage()->IsDirty())
		{
			CachedActorDesc = Actor->CreateActorDesc();
			ActorDesc = CachedActorDesc.Get();
		}
	}
}
#endif