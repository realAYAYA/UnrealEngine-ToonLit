// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeDebugTextTask.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeDebugTextTask)

bool FStateTreeDebugTextTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(ReferenceActorHandle);
	return true;
}

EStateTreeRunStatus FStateTreeDebugTextTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (!bEnabled)
	{
		return EStateTreeRunStatus::Succeeded;
	}
	
	const UWorld* World = Context.GetWorld();

	AActor* ReferenceActor = Context.GetExternalDataPtr(ReferenceActorHandle);
	if (World == nullptr && ReferenceActor != nullptr)
	{
		World = ReferenceActor->GetWorld();
	}
	else if (ReferenceActor == nullptr)
	{
		ReferenceActor = Cast<AActor>(Context.GetOwner());
	}

	// Reference actor is not required (offset will be used as a global world location)
	// but a valid world is required.
	if (World == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}

	DrawDebugString(World, Offset, Text, ReferenceActor, TextColor,	/*Duration*/-1,	/*DrawShadows*/true, FontScale);
	
	return EStateTreeRunStatus::Running;
}

void FStateTreeDebugTextTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (!bEnabled)
	{
		return;
	}

	const UWorld* World = Context.GetWorld();

	AActor* ReferenceActor = Context.GetExternalDataPtr(ReferenceActorHandle);
	if (World == nullptr && ReferenceActor != nullptr)
	{
		World = ReferenceActor->GetWorld();
	}	
	else if (ReferenceActor == nullptr)
	{
		ReferenceActor = Cast<AActor>(Context.GetOwner());
	}
	
	// Reference actor is not required (offset was used as a global world location)
	// but a valid world is required.
	if (World == nullptr)
	{
		return;
	}

	// Drawing an empty text will remove the HUD DebugText entries associated to the target actor
	DrawDebugString(World, Offset, "",	ReferenceActor);
}

