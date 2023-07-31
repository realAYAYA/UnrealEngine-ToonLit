// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/MassClaimSmartObjectTask.h"
#include "MassCommonFragments.h"
#include "MassAIBehaviorTypes.h"
#include "MassSignalSubsystem.h"
#include "MassSmartObjectHandler.h"
#include "MassSmartObjectFragments.h"
#include "MassStateTreeExecutionContext.h"
#include "MassNavigationFragments.h"
#include "MassZoneGraphNavigationFragments.h"
#include "MassZoneGraphNavigationUtils.h"
#include "Engine/World.h"
#include "StateTreeLinker.h"

//----------------------------------------------------------------------//
// FMassClaimSmartObjectTask
//----------------------------------------------------------------------//

FMassClaimSmartObjectTask::FMassClaimSmartObjectTask()
{
	// This task should not react to Enter/ExitState when the state is reselected.
	bShouldStateChangeOnReselect = false;
}

bool FMassClaimSmartObjectTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectUserHandle);
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	Linker.LinkExternalData(MassSignalSubsystemHandle);

	return true;
}

EStateTreeRunStatus FMassClaimSmartObjectTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// Retrieve fragments and subsystems
	USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	UMassSignalSubsystem& SignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
	FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const FMassSmartObjectCandidateSlots* CandidateSlots = InstanceData.CandidateSlots.GetPtr<FMassSmartObjectCandidateSlots>();
	if (CandidateSlots == nullptr)
	{
		MASSBEHAVIOR_LOG(Log, TEXT("Candidate slots not set"));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ClaimedSlot.Invalidate();
	
	// Setup MassSmartObject handler and claim
	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	const FMassSmartObjectHandler MassSmartObjectHandler(MassContext.GetEntityManager(), MassContext.GetEntitySubsystemExecutionContext(), SmartObjectSubsystem, SignalSubsystem);
	
	InstanceData.ClaimedSlot = MassSmartObjectHandler.ClaimCandidate(MassContext.GetEntity(), SOUser, *CandidateSlots);

	// Treat claiming a slot as consuming all the candidate slots.
	// This is done here because of the limited ways we can communicate between FindSmartObject() and ClaimSmartObject().
	// InteractionCooldownEndTime is used by the FindSmartObject() to invalidate the candidates.
	SOUser.InteractionCooldownEndTime = Context.GetWorld()->GetTimeSeconds() + InteractionCooldown;

	if (!InstanceData.ClaimedSlot.IsValid())
	{
		MASSBEHAVIOR_LOG(Log, TEXT("Failed to claim smart object slot from %d candidates"), CandidateSlots->NumSlots);
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

void FMassClaimSmartObjectTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// Succeeded or not, prevent interactions for a specified duration.
	SOUser.InteractionCooldownEndTime = Context.GetWorld()->GetTimeSeconds() + InteractionCooldown;

	if (InstanceData.ClaimedSlot.IsValid())
	{
		const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
		USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
		UMassSignalSubsystem& SignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
		const FMassSmartObjectHandler MassSmartObjectHandler(MassContext.GetEntityManager(), MassContext.GetEntitySubsystemExecutionContext(), SmartObjectSubsystem, SignalSubsystem);

		MassSmartObjectHandler.ReleaseSmartObject(MassContext.GetEntity(), SOUser, InstanceData.ClaimedSlot);
	}
	else
	{
		MASSBEHAVIOR_LOG(VeryVerbose, TEXT("Exiting state with an invalid ClaimHandle: nothing to do."));
	}
}

EStateTreeRunStatus FMassClaimSmartObjectTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	// Prevent FindSmartObject() to query new objects while claimed.
	// This is done here because of the limited ways we can communicate between FindSmartObject() and ClaimSmartObject().
	// InteractionCooldownEndTime is used by the FindSmartObject() to invalidate the candidates.
	SOUser.InteractionCooldownEndTime = Context.GetWorld()->GetTimeSeconds() + InteractionCooldown;

	// Check that the claimed slot is still valid, and if not, fail the task.
	// The slot can become invalid if the whole SO or slot becomes invalidated.
	if (InstanceData.ClaimedSlot.IsValid())
	{
		const USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
		if (!SmartObjectSubsystem.IsClaimedSmartObjectValid(InstanceData.ClaimedSlot))
		{
			InstanceData.ClaimedSlot.Invalidate();
		}
	}

	return InstanceData.ClaimedSlot.IsValid() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}
