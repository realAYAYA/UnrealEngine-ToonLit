// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeTask_GetSlotEntranceTags.h"
#include "StateTreeExecutionContext.h"
#include "SmartObjectSubsystem.h"
#include "VisualLogger/VisualLogger.h"
#include "StateTreeLinker.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeTask_GetSlotEntranceTags)

FStateTreeTask_GetSlotEntranceLocation::FStateTreeTask_GetSlotEntranceLocation()
{
	// No tick needed.
	bShouldCallTick = false;
	// No need to update bound properties after enter state.
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

bool FStateTreeTask_GetSlotEntranceLocation::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	return true;
}

bool FStateTreeTask_GetSlotEntranceLocation::UpdateResult(const FStateTreeExecutionContext& Context) const
{
	const USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.SlotEntranceHandle.IsValid())
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[FStateTreeTask_GetSlotEntranceLocation] Expected valid SlotEntranceHandle handle."));
		return false;
	}

	// Make request without validation to just get the entrance tags.
	FSmartObjectSlotEntranceLocationRequest Request;
	Request.UserActor = nullptr;
	Request.ValidationFilter = USmartObjectSlotValidationFilter::StaticClass();
	Request.bProjectNavigationLocation = false;
	Request.bTraceGroundLocation = false;
	Request.bCheckEntranceLocationOverlap = false;
	Request.bCheckSlotLocationOverlap = false;
	Request.bCheckTransitionTrajectory = false;
	
	FSmartObjectSlotEntranceLocationResult EntryLocation;
	if (SmartObjectSubsystem.UpdateEntranceLocation(InstanceData.SlotEntranceHandle, Request, EntryLocation))
	{
		InstanceData.EntranceTags = EntryLocation.Tags;
		return true;
	}

	return false;
}

EStateTreeRunStatus FStateTreeTask_GetSlotEntranceLocation::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (!UpdateResult(Context))
	{
		return EStateTreeRunStatus::Failed;
	}
	
	return EStateTreeRunStatus::Running;
}
