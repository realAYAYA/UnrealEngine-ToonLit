// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "WorldPartitionActorDescView.h"

#if WITH_EDITOR
/**
 * A view on top of an actor desc, which reverts to the actor if its loaded and dirty.
 */
class FWorldPartitionActorViewProxy : public FWorldPartitionActorDescView
{
public:
	FWorldPartitionActorViewProxy(const FWorldPartitionActorDesc* InActorDesc);

private:
	TUniquePtr<FWorldPartitionActorDesc> CachedActorDesc;
};
#endif