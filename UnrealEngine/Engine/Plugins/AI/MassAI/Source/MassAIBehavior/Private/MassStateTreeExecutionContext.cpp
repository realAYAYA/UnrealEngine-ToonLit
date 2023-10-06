// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassStateTreeExecutionContext.h"
#include "MassStateTreeTypes.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Engine/World.h"

FMassStateTreeExecutionContext::FMassStateTreeExecutionContext(UObject& InOwner, const UStateTree& InStateTree, FStateTreeInstanceData& InInstanceData,
															   FMassEntityManager& InEntityManager, UMassSignalSubsystem& InSignalSubsystem, FMassExecutionContext& InContext)
	: FStateTreeExecutionContext(InOwner, InStateTree, InInstanceData)
	, EntityManager(&InEntityManager)
	, SignalSubsystem(&InSignalSubsystem)
	, EntitySubsystemExecutionContext(&InContext)
{
}

void FMassStateTreeExecutionContext::BeginDelayedTransition(const FStateTreeTransitionDelayedState& DelayedState)
{
	if (SignalSubsystem != nullptr && Entity.IsSet())
	{
		// Tick again after the games time has passed to see if the condition still holds true.
		SignalSubsystem->DelaySignalEntity(UE::Mass::Signals::DelayedTransitionWakeup, Entity, DelayedState.TimeLeft + KINDA_SMALL_NUMBER);
	}
}
