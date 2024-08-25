// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionActorDescViewProxy.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/WorldPartitionActorDescView.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
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
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
