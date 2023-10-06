// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeTask_FindSlotEntranceLocation.h"
#include "StateTreeExecutionContext.h"
#include "SmartObjectSubsystem.h"
#include "VisualLogger/VisualLogger.h"
#include "StateTreeLinker.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeTask_FindSlotEntranceLocation)

FStateTreeTask_FindSlotEntranceLocation::FStateTreeTask_FindSlotEntranceLocation()
{
	// No tick needed.
	bShouldCallTick = false;
	// No need to update bound properties after enter state.
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

bool FStateTreeTask_FindSlotEntranceLocation::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	return true;
}

bool FStateTreeTask_FindSlotEntranceLocation::UpdateResult(const FStateTreeExecutionContext& Context) const
{
	const USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.ReferenceSlot.IsValid())
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[StateTreeTask_FindSlotNavigationLocation] Expected valid ReferenceSlot handle."));
		return false;
	}

	if (!InstanceData.UserActor)
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[StateTreeTask_FindSlotNavigationLocation] Expected valid UserActor handle."));
		return false;
	}

	FSmartObjectSlotEntranceLocationRequest Request;
	Request.UserActor = InstanceData.UserActor;
	Request.ValidationFilter = ValidationFilter;
	Request.SelectMethod = SelectMethod;
	Request.bProjectNavigationLocation = bProjectNavigationLocation;
	Request.bTraceGroundLocation = bTraceGroundLocation;
	Request.bCheckEntranceLocationOverlap = bCheckEntranceLocationOverlap;
	Request.bCheckSlotLocationOverlap = bCheckSlotLocationOverlap;
	Request.bCheckTransitionTrajectory = bCheckTransitionTrajectory;
	Request.LocationType = LocationType;
	Request.SearchLocation = InstanceData.UserActor->GetActorLocation();

	FSmartObjectSlotEntranceLocationResult EntryLocation;
	if (SmartObjectSubsystem.FindEntranceLocationForSlot(InstanceData.ReferenceSlot, Request, EntryLocation))
	{
		InstanceData.EntryTransform = FTransform(EntryLocation.Rotation, EntryLocation.Location);
		InstanceData.EntranceTags = EntryLocation.Tags;
		return true;
	}

	return false;
}

EStateTreeRunStatus FStateTreeTask_FindSlotEntranceLocation::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (!UpdateResult(Context))
	{
		return EStateTreeRunStatus::Failed;
	}
	
	return EStateTreeRunStatus::Running;
}
