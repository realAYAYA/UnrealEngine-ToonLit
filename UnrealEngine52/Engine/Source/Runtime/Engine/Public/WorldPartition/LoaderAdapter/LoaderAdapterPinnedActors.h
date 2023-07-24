// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR

#include "WorldPartition/LoaderAdapter/LoaderAdapterActorList.h"

class AActor;
class FWorldPartitionActorDesc;

class ENGINE_API FLoaderAdapterPinnedActors : public FLoaderAdapterActorList
{
public:
	FLoaderAdapterPinnedActors(UWorld* InWorld)
		: FLoaderAdapterActorList(InWorld)
	{}

	static FText GetUnloadedReason(FWorldPartitionActorDesc* InActorDesc);
	static bool SupportsPinning(FWorldPartitionActorDesc* InActorDesc);
	static bool SupportsPinning(AActor* InActor);

protected:
	virtual bool PassActorDescFilter(const FWorldPartitionHandle& ActorHandle) const override;
};

#endif