// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_BeginExecution)

FName FRigUnit_BeginExecution::EventName = TEXT("Forwards Solve");
FName FRigUnit_PreBeginExecution::EventName = TEXT("Pre Forwards Solve");
FName FRigUnit_PostBeginExecution::EventName = TEXT("Post Forwards Solve");

FRigUnit_BeginExecution_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	ExecuteContext.SetEventName(FRigUnit_BeginExecution::EventName);
}

FRigUnit_PreBeginExecution_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	ExecuteContext.SetEventName(FRigUnit_PreBeginExecution::EventName);
}

FRigUnit_PostBeginExecution_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	ExecuteContext.SetEventName(FRigUnit_PostBeginExecution::EventName);
}

