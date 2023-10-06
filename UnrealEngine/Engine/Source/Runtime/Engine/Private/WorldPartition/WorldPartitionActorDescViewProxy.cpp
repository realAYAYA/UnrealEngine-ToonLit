// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionActorDescViewProxy.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"
#include "UObject/Package.h"
#include "WorldPartition/WorldPartitionActorDescView.h"

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionActorDesc.h"

FWorldPartitionActorViewProxy::FWorldPartitionActorViewProxy(const FWorldPartitionActorDesc* InActorDesc)
	: FWorldPartitionActorDescView(InActorDesc)
{
	if (AActor* Actor = ActorDesc->GetActor(false))
	{
		if (Actor->GetPackage()->IsDirty())
		{
			CachedActorDesc = Actor->CreateActorDesc();
			ActorDesc = CachedActorDesc.Get();
		}
	}
}
#endif
