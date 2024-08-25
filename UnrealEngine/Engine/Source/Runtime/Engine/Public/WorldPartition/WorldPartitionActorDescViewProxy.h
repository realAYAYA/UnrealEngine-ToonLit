// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR

#include "WorldPartitionActorDescView.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
/**
 * A view on top of an actor desc, which reverts to the actor if its loaded and dirty.
 */
class ENGINE_API UE_DEPRECATED(5.4, "Class is deprecated") FWorldPartitionActorViewProxy : public FWorldPartitionActorDescView
{
public:
	FWorldPartitionActorViewProxy(const FWorldPartitionActorDesc* InActorDesc);

private:
	TUniquePtr<FWorldPartitionActorDesc> CachedActorDesc;
};
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#endif
