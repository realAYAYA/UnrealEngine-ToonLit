// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionTabFactory.h"

FTabSpawnerEntry& FAvaTransitionTabFactory::RegisterTabSpawner(TSharedRef<FTabManager> InTabManager, const FApplicationMode* InCurrentApplicationMode) const
{
	return FWorkflowTabFactory::RegisterTabSpawner(InTabManager, InCurrentApplicationMode)
		.SetReadOnlyBehavior(ReadOnlyBehavior);
}
