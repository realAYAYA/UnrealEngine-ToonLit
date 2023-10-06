// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR

#include "WorldPartition/LoaderAdapter/LoaderAdapterActorList.h"

class AActor;
class FWorldPartitionActorDesc;

class FLoaderAdapterPinnedActors : public FLoaderAdapterActorList
{
public:
	FLoaderAdapterPinnedActors(UWorld* InWorld)
		: FLoaderAdapterActorList(InWorld)
	{}

	static ENGINE_API bool SupportsPinning(FWorldPartitionActorDesc* InActorDesc);
	static ENGINE_API bool SupportsPinning(AActor* InActor);

protected:
	ENGINE_API virtual bool PassActorDescFilter(const FWorldPartitionHandle& ActorHandle) const override;
};

#endif
